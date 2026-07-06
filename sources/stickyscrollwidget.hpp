// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#pragma once

#include "stickyscrolltypes.hpp"
#include "symbolindex.hpp"

#include <QHash>
#include <QList>
#include <QPointer>
#include <QTextLayout>
#include <QTimer>
#include <QWidget>

#include <memory>

QT_BEGIN_NAMESPACE
class QTextBlock;
QT_END_NAMESPACE

namespace TextEditor { class TextEditorWidget; }

namespace StickyScroll {

class StickyScrollWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit StickyScrollWidget(TextEditor::TextEditorWidget *editor);
    ~StickyScrollWidget() override;

    void setStickyEnabled(bool enabled);
    void refresh();
    void setSymbolSpans(const QList<SymbolSpan> &spans, int docRevision);

signals:
    void navigateToLine(int line);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    void scheduleUpdate();
    void updateSticky();
    void hidePanel();
    int rowAt(qreal y) const;
    QList<QTextLayout::FormatRange> formatsForBlock(const QTextBlock &block) const;
    qreal lineHeight() const;

    TextEditor::TextEditorWidget *m_editor;
    QPointer<QWidget> m_shadow;
    PanelState m_state;
    int m_hoverRow = -1;
    QTimer m_updateTimer;
    bool m_enabled = true;
    int m_paintedRevision = -1;
    SymbolIndex m_symbols;
    int m_symbolsRevision = -1;
    QHash<int, std::shared_ptr<QTextLayout>> m_layoutCache;
};

}
