// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#include "stickyscrollwidget.hpp"

#include "panelstateengine.hpp"
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

enum class ScopeFormat { Brace, Indentation, Markdown };

constexpr ScopeFormat kDefaultScopeFormat = ScopeFormat::Brace;

static ScopeFormat scopeFormatFor(const QString &mimeType)
{
    static const QHash<QString, ScopeFormat> byMime = {
        {"text/x-c++src", kDefaultScopeFormat},
        {"text/x-c++hdr", kDefaultScopeFormat},
        {"text/x-csrc", kDefaultScopeFormat},
        {"text/x-chdr", kDefaultScopeFormat},
        {"text/x-python", kDefaultScopeFormat},
        {"application/json", kDefaultScopeFormat},
        {"text/javascript", kDefaultScopeFormat},
        {"application/javascript", kDefaultScopeFormat},
        {"text/x-cmake", kDefaultScopeFormat},
        {"text/x-qml", kDefaultScopeFormat},
        {"text/x-yaml", ScopeFormat::Indentation},
        {"text/yaml", ScopeFormat::Indentation},
        {"application/x-yaml", ScopeFormat::Indentation},
        {"application/yaml", ScopeFormat::Indentation},
        {"text/markdown", ScopeFormat::Markdown},
        {"text/x-markdown", ScopeFormat::Markdown},
    };
    return byMime.value(mimeType, kDefaultScopeFormat);
}

template <typename Geometry>
static PanelState computePanelStateFor(ScopeFormat format, const QTextDocument *doc,
                                       const Geometry &geometry, int maxLines)
{
    switch (format) {
    case ScopeFormat::Indentation:
        return computePanelState(doc, geometry, maxLines, IndentationScopeModel{});
    case ScopeFormat::Markdown:
        return computePanelState(doc, geometry, maxLines, MarkdownScopeModel{});
    case ScopeFormat::Brace:
        break;
    }
    return computePanelState(doc, geometry, maxLines);
}

const int kShadowHeight = 6;
const int kShadowAlpha = 70;
const int kBorderAlpha = 60;
const int kLineNumberPadding = 4;
const int kContentsChangedDebounceMs = 150;

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
            this, [this] { update(); });
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
}

void StickyScrollWidget::setSymbolSpans(const QList<SymbolSpan> &spans, int docRevision)
{
    if (m_symbols.spans == spans && m_symbolsRevision == docRevision)
        return;
    m_symbols.spans = spans;
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
    const ScopeFormat format = scopeFormatFor(textDocument->mimeType());
    const bool symbolsFresh = format == ScopeFormat::Brace && !m_symbols.isEmpty()
                              && m_symbolsRevision == doc->revision();
    const PanelState state
        = symbolsFresh ? computePanelState(doc, editorGeometry, maxLines,
                                           RefinedBraceScopeModel{m_symbols})
                       : computePanelStateFor(format, doc, editorGeometry, maxLines);

    if (state.chain.rows.isEmpty()) {
        m_state = state;
        m_hoverRow = -1;
        hidePanel();
        return;
    }

    const qreal fullHeight = state.fullHeight(editorGeometry.rowHeight());
    const QRect viewportRect = m_editor->viewport()->geometry();
    const int panelHeight = qRound(fullHeight - state.pushOffset);
    const int panelWidth = viewportRect.x() + viewportRect.width();
    const QRect panelGeometry(0, viewportRect.y(), panelWidth, panelHeight);

    const int revision = doc->revision();
    const bool unchanged = isVisible() && panelGeometry == geometry()
                           && state == m_state
                           && revision == m_paintedRevision;
    if (state.chain.rows != m_state.chain.rows)
        m_hoverRow = -1;
    m_state = state;
    m_paintedRevision = revision;
    if (unchanged)
        return;

    m_layoutCache.clear();
    setGeometry(panelGeometry);
    if (m_shadow) {
        m_shadow->setGeometry(0, viewportRect.y() + panelHeight, panelWidth, kShadowHeight);
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
        << "| rows" << FoldingScanner::logLines(m_state.chain.rows).join(',')
        << "| innermost start" << m_state.chain.innermostFoldStart + 1
        << "rowCount" << m_state.chain.innermostRowCount
        << "| fullHeight" << fullHeight << "push" << m_state.pushOffset
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
    return QFontMetricsF(fontSettings.font()).lineSpacing();
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

    const qreal xOffset = gutterWidth + doc->documentMargin()
                          - m_editor->horizontalScrollBar()->value();
    const int firstInnermostRow = m_state.chain.firstInnermostRow();

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
        layout->draw(&painter, QPointF(xOffset, y));

        if (sliding)
            painter.restore();
    }

    QColor border = baseFormat.foreground().color();
    border.setAlpha(kBorderAlpha);
    painter.fillRect(QRect(0, height() - 1, width(), 1), border);
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
