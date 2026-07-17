// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#include "stickyscrollwidget.hpp"

#include "panellayout.hpp"
#include "scanutil.hpp"
#include "scoperesolver.hpp"
#include "stickyscrollsettings.hpp"

#include <texteditor/fontsettings.h>
#include <texteditor/syntaxhighlighter.h>
#include <texteditor/textdocument.h>
#include <texteditor/textdocumentlayout.h>
#include <texteditor/texteditor.h>
#include <texteditor/texteditorconstants.h>

#include <utils/plaintextedit/texteditorlayout.h>

#include <QFontMetricsF>
#include <QHash>
#include <QLoggingCategory>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextDocument>

using namespace TextEditor;

namespace StickyScroll {

const int kShadowHeight = 6;
const int kShadowAlpha = 70;
const int kBorderAlpha = 120;
const int kBorderHeight = 2;
const int kLineNumberPadding = 4;
const int kContentsChangedDebounceMs = 150;
const qreal kDimInnermost = 0.15;
const qreal kDimOuter = 0.45;

static qreal rowDimFactor(int row, int firstInnermostRow)
{
    return row < firstInnermostRow ? kDimOuter : kDimInnermost;
}

static int maxStickyLines()
{
    return int(settings().maxLines());
}

class EditorGeometry
{
public:
    EditorGeometry(TextEditorWidget *editor, qreal rowHeight)
        : m_editor(editor)
        , m_rowHeight(rowHeight)
    {}

    int blockForVisibleRow(int row) const { return m_editor->blockNumberForVisibleRow(row); }
    int lastVisibleBlock() const { return m_editor->lastVisibleBlockNumber(); }
    qreal rowHeight() const { return m_rowHeight; }

    std::optional<qreal> blockTop(int blockNumber) const
    {
        const QTextBlock block = m_editor->document()->findBlockByNumber(blockNumber);
        if (!block.isValid())
            return std::nullopt;
        const QRect rect = m_editor->cursorRect(QTextCursor(block));
        if (!rect.isValid())
            return std::nullopt;
        return rect.top();
    }

private:
    TextEditorWidget *m_editor;
    qreal m_rowHeight;
};

class ShadowWidget final : public QWidget
{
public:
    explicit ShadowWidget(QWidget *parent)
        : QWidget(parent)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground);
        hide();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        QLinearGradient gradient(0, 0, 0, height());
        gradient.setColorAt(0, QColor(0, 0, 0, kShadowAlpha));
        gradient.setColorAt(1, QColor(0, 0, 0, 0));
        painter.fillRect(rect(), gradient);
    }
};

StickyScrollWidget::StickyScrollWidget(TextEditorWidget *editor)
    : QWidget(editor)
    , m_editor(editor)
    , m_shadow(new ShadowWidget(editor))
{
    setFocusPolicy(Qt::NoFocus);
    setCursor(Qt::PointingHandCursor);
    setMouseTracking(true);

    m_updateTimer.setSingleShot(true);
    connect(&m_updateTimer, &QTimer::timeout, this, &StickyScrollWidget::updateSticky);

    connect(editor->verticalScrollBar(), &QScrollBar::valueChanged,
            this, &StickyScrollWidget::scheduleUpdate);
    connect(editor->verticalScrollBar(), &QScrollBar::rangeChanged,
            this, &StickyScrollWidget::scheduleUpdate);
    connect(editor->horizontalScrollBar(), &QScrollBar::valueChanged,
            this, [this] {
                if (settings().followHorizontalScroll())
                    update();
            });
    connect(editor, &TextEditorWidget::resized, this, &StickyScrollWidget::scheduleUpdate);

    connect(editor, &Utils::PlainTextEdit::updateRequest,
            this, [this](const QRect &, int dy) {
                if (dy != 0)
                    scheduleUpdate();
            });
    connect(editor->textDocument(), &TextDocument::fontSettingsChanged,
            this, [this] {
                m_layoutCache.clear();
                scheduleUpdate();
            });

    connect(editor->document(), &QTextDocument::contentsChanged,
            this, [this] { m_updateTimer.start(kContentsChangedDebounceMs); });

    if (SyntaxHighlighter *highlighter = editor->textDocument()->syntaxHighlighter()) {
        connect(highlighter, &SyntaxHighlighter::finished,
                this, [this] {
                    m_layoutCache.clear();
                    m_updateTimer.start(0);
                    update();
                });
    }

    hide();
    scheduleUpdate();
}

StickyScrollWidget::~StickyScrollWidget()
{
    delete m_shadow;
}

void StickyScrollWidget::setStickyEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;
    m_enabled = enabled;
    if (enabled)
        scheduleUpdate();
    else
        hidePanel();
}

void StickyScrollWidget::hidePanel()
{
    hide();
    if (m_shadow)
        m_shadow->hide();
}

void StickyScrollWidget::refresh()
{
    scheduleUpdate();
    update();
}

void StickyScrollWidget::setSymbolSpans(const QList<SymbolSpan> &spans, int docRevision)
{
    if (m_symbols.spans() == spans && m_symbolsRevision == docRevision)
        return;
    m_symbols.setSpans(spans);
    m_symbolsRevision = docRevision;
    scheduleUpdate();
}

void StickyScrollWidget::scheduleUpdate()
{
    if (m_updateTimer.isActive() && m_updateTimer.remainingTime() > 0)
        return;
    m_updateTimer.start(0);
}

void StickyScrollWidget::updateSticky()
{
    if (!m_enabled) {
        hidePanel();
        return;
    }

    QTextDocument *doc = m_editor->document();
    const EditorGeometry editorGeometry(m_editor, lineHeight());
    const int maxLines = maxStickyLines();
    TextDocument *textDocument = m_editor->textDocument();
    const PanelState state = computePanelState(
        doc, editorGeometry, maxLines,
        ScopeContext{textDocument->mimeType(), m_symbols, m_symbolsRevision});

    const int revision = doc->revision();
    const PanelLayout layout = reconcilePanel({geometry(), m_state, m_paintedRevision, isVisible()},
                                              state, m_editor->viewport()->geometry(),
                                              editorGeometry.rowHeight(), revision, kShadowHeight);
    if (layout.resetHover)
        m_hoverRow = -1;
    m_state = state;
    m_paintedRevision = revision;

    if (layout.action == PanelAction::Hide) {
        hidePanel();
        return;
    }
    if (layout.action == PanelAction::Keep)
        return;

    m_layoutCache.clear();
    setGeometry(layout.panel);
    if (m_shadow) {
        m_shadow->setGeometry(layout.shadow);
        m_shadow->show();
        m_shadow->raise();
    }
    show();
    raise();
    update();
    if (m_shadow)
        m_shadow->update();

    qCDebug(stickyLog).noquote()
        << "update: top line" << m_editor->firstVisibleBlockNumber() + 1
        << "| rows" << logLines(m_state.chain.rows).join(',')
        << "| innermost start" << m_state.chain.innermostFoldStart + 1
        << "rowCount" << m_state.chain.innermostRowCount
        << "| push" << m_state.pushOffset
        << "-> height" << height();
}

QList<QTextLayout::FormatRange> StickyScrollWidget::formatsForBlock(const QTextBlock &block) const
{
    if (QTextLayout *docLayout = block.layout()) {
        QList<QTextLayout::FormatRange> formats = docLayout->formats();
        if (!formats.isEmpty())
            return formats;
    }
    if (Utils::TextEditorLayout *editorLayout = m_editor->editorLayout()) {
        if (QTextLayout *blockLayout = editorLayout->blockLayout(block))
            return blockLayout->formats();
    }
    return {};
}

qreal StickyScrollWidget::lineHeight() const
{
    if (Utils::TextEditorLayout *editorLayout = m_editor->editorLayout()) {
        const int spacing = editorLayout->lineSpacing();
        if (spacing > 0)
            return spacing;
    }
    const FontSettings &fontSettings = m_editor->textDocument()->fontSettings();
    const qreal spacing = fontSettings.lineSpacing();
    if (spacing > 0)
        return spacing;
    const qreal fallback = QFontMetricsF(fontSettings.font()).lineSpacing();
    return fallback > 0 ? fallback : 1;
}

void StickyScrollWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    const FontSettings &fontSettings = m_editor->textDocument()->fontSettings();
    const QTextCharFormat baseFormat = fontSettings.toTextCharFormat(C_TEXT);
    painter.fillRect(rect(), baseFormat.background());

    const qreal rowHeight = lineHeight();
    const int gutterWidth = m_editor->viewport()->geometry().x();

    const QTextCharFormat lineNumberFormat = fontSettings.toTextCharFormat(C_LINE_NUMBER);
    if (gutterWidth > 0) {
        QBrush gutterBrush = lineNumberFormat.background();
        if (gutterBrush.style() == Qt::NoBrush)
            gutterBrush = baseFormat.background();
        painter.fillRect(QRect(0, 0, gutterWidth, height()), gutterBrush);
    }
    QColor panelTint = baseFormat.foreground().color();
    panelTint.setAlphaF(settings().panelTint() / 100.0);
    painter.fillRect(rect(), panelTint);
    const QFont extraAreaFont = m_editor->extraArea()->font();
    int foldBoxSpacing = QFontMetrics(extraAreaFont).lineSpacing();
    if (Utils::TextEditorLayout *editorLayout = m_editor->editorLayout()) {
        if (editorLayout->relativeLineSpacing() != 100)
            foldBoxSpacing = editorLayout->lineSpacing();
    }
    const int foldBoxWidth = m_editor->codeFoldingVisible()
                                 ? foldBoxSpacing + foldBoxSpacing % 2 + 1
                                 : 0;

    QTextDocument *doc = m_editor->document();
    QTextOption option = doc->defaultTextOption();
    option.setWrapMode(QTextOption::NoWrap);

    const int horizontalScroll = settings().followHorizontalScroll()
                                     ? m_editor->horizontalScrollBar()->value()
                                     : 0;
    const qreal xOffset = gutterWidth + doc->documentMargin() - horizontalScroll;
    const int firstInnermostRow = m_state.chain.firstInnermostRow();
    const qreal deEmphasisIntensity = settings().deEmphasizeScopes()
                                          ? settings().deEmphasisIntensity() / 100.0
                                          : 0.0;

    for (int i = 0; i < m_state.chain.rows.size(); ++i) {
        const int blockNumber = m_state.chain.rows.at(i);
        const QTextBlock block = doc->findBlockByNumber(blockNumber);
        if (!block.isValid())
            continue;

        const bool sliding = m_state.pushOffset > 0 && i >= firstInnermostRow;
        if (sliding) {
            painter.save();
            const qreal slotTop = firstInnermostRow * rowHeight;
            painter.setClipRect(QRectF(0, slotTop, width(), height() - slotTop));
        }
        const qreal y = i * rowHeight - (sliding ? m_state.pushOffset : 0);

        if (i == m_hoverRow && !sliding) {
            const QTextCharFormat hoverFormat = fontSettings.toTextCharFormat(C_CURRENT_LINE);
            if (hoverFormat.background().style() != Qt::NoBrush)
                painter.fillRect(QRectF(gutterWidth, y, width() - gutterWidth, rowHeight),
                                 hoverFormat.background());
        }

        const qreal dim = i == m_hoverRow
                              ? 0.0
                              : deEmphasisIntensity * rowDimFactor(i, firstInnermostRow);
        painter.setOpacity(1.0 - dim);

        if (m_editor->lineNumbersVisible() && gutterWidth > 0) {
            painter.setFont(extraAreaFont);
            painter.setPen(lineNumberFormat.foreground().color());
            const QRectF numberRect(0, y, gutterWidth - foldBoxWidth - kLineNumberPadding,
                                    rowHeight);
            painter.drawText(numberRect, Qt::AlignRight, QString::number(blockNumber + 1));
        }

        std::shared_ptr<QTextLayout> layout = m_layoutCache.value(blockNumber);
        if (!layout) {
            layout = std::make_shared<QTextLayout>(block.text());
            layout->setFont(fontSettings.font());
            layout->setTextOption(option);
            layout->setFormats(formatsForBlock(block));
            layout->beginLayout();
            QTextLine line = layout->createLine();
            line.setPosition(QPointF(0, 0));
            layout->endLayout();
            m_layoutCache.insert(blockNumber, layout);
        }
        painter.setPen(baseFormat.foreground().color());
        painter.save();
        painter.setClipRect(QRectF(gutterWidth, 0, width() - gutterWidth, height()),
                            Qt::IntersectClip);
        layout->draw(&painter, QPointF(xOffset, y));
        painter.restore();
        painter.setOpacity(1.0);

        if (sliding)
            painter.restore();
    }

    QColor border = baseFormat.foreground().color();
    border.setAlpha(kBorderAlpha);
    painter.fillRect(QRect(0, height() - kBorderHeight, width(), kBorderHeight), border);
}

int StickyScrollWidget::rowAt(qreal y) const
{
    const qreal rowHeight = lineHeight();
    const qreal slotTop = m_state.chain.firstInnermostRow() * rowHeight;
    if (y >= slotTop)
        y += m_state.pushOffset;
    const int row = int(y / rowHeight);
    return row >= 0 && row < m_state.chain.rows.size() ? row : -1;
}

void StickyScrollWidget::mousePressEvent(QMouseEvent *event)
{
    const int row = rowAt(event->position().y());
    if (row >= 0) {
        event->accept();
        emit navigateToLine(m_state.chain.rows.at(row) + 1);
        return;
    }
    QWidget::mousePressEvent(event);
}

void StickyScrollWidget::mouseMoveEvent(QMouseEvent *event)
{
    const int row = rowAt(event->position().y());
    if (row != m_hoverRow) {
        m_hoverRow = row;
        update();
    }
    QWidget::mouseMoveEvent(event);
}

void StickyScrollWidget::leaveEvent(QEvent *event)
{
    if (m_hoverRow != -1) {
        m_hoverRow = -1;
        update();
    }
    QWidget::leaveEvent(event);
}

}
