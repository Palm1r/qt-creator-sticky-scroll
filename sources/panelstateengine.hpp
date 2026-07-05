// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#pragma once

#include "foldingscanner.hpp"
#include "scopemodels.hpp"
#include "stickyscrolltypes.hpp"

#include <texteditor/textdocumentlayout.h>

#include <QHash>
#include <QTextBlock>
#include <QTextDocument>

#include <concepts>
#include <optional>
#include <utility>

namespace StickyScroll {

template <typename T>
concept ViewportGeometry = requires(const T geometry, int blockNumber, int row) {
    { geometry.blockForVisibleRow(row) } -> std::convertible_to<int>;
    { geometry.lastVisibleBlock() } -> std::convertible_to<int>;
    { geometry.blockTop(blockNumber) } -> std::convertible_to<std::optional<qreal>>;
    { geometry.rowHeight() } -> std::convertible_to<qreal>;
};

template <ViewportGeometry Geometry, ScopeModel Model = BraceScopeModel>
class PanelStateEngine
{
public:
    PanelStateEngine(const QTextDocument *doc, const Geometry &geometry, int maxLines,
                     Model model = {})
        : m_doc(doc)
        , m_geometry(geometry)
        , m_maxLines(maxLines)
        , m_model(std::move(model))
    {}

    PanelState compute()
    {
        PanelState state;
        growChain(state.chain);

        if constexpr (!Model::braceDynamics) {
            includeFirstVisibleFoldStart(state.chain);
            return state;
        } else {
            const qreal rowHeight = m_geometry.rowHeight();
            const int sizeBeforeFixpoint = state.chain.rows.size();
            state.pushOffset = pushOffsetFor(state.chain, state.fullHeight(rowHeight));
            if (state.pushOffset <= 0) {
                while (true) {
                    const int before = state.chain.rows.size();
                    captureTouchingScopes(state.chain);
                    growChain(state.chain);
                    if (state.chain.rows.size() == before)
                        break;
                }
            }

            if (state.chain.rows.isEmpty())
                return state;

            if (state.chain.rows.size() != sizeBeforeFixpoint)
                state.pushOffset = pushOffsetFor(state.chain, state.fullHeight(rowHeight));
            return state;
        }
    }

private:
    ScopeChain enclosingFor(int blockNumber)
    {
        const auto cached = m_memo.constFind(blockNumber);
        if (cached != m_memo.constEnd())
            return *cached;
        return *m_memo.insert(blockNumber, m_model.enclosing(m_doc, blockNumber, m_maxLines));
    }

    void growChain(ScopeChain &chain)
    {
        for (int i = 0; i <= m_maxLines; ++i) {
            const int candidate = m_geometry.blockForVisibleRow(chain.rows.size());
            if (candidate < 0)
                break;
            ScopeChain refined = enclosingFor(candidate);
            if (refined.rows == chain.rows)
                break;
            if (refined.rows.size() < chain.rows.size())
                break;
            if constexpr (!Model::braceDynamics) {
                if (refined.rows.size() == chain.rows.size())
                    break;
            }
            chain = std::move(refined);
        }
    }

    void includeFirstVisibleFoldStart(ScopeChain &chain)
    {
        const int firstVisible = m_geometry.blockForVisibleRow(0);
        if (firstVisible < 0 || chain.rows.contains(firstVisible))
            return;
        const QTextBlock block = m_doc->findBlockByNumber(firstVisible);
        if (!m_model.isFoldStart(block))
            return;
        if (chain.rows.size() >= m_maxLines && !chain.rows.isEmpty())
            chain.rows.removeFirst();
        chain.rows.append(firstVisible);
        chain.innermostFoldStart = firstVisible;
        chain.innermostRowCount = 1;
    }

    void captureTouchingScopes(ScopeChain &chain)
    {
        const qreal rowHeight = m_geometry.rowHeight();
        while (true) {
            const int belowNumber = m_geometry.blockForVisibleRow(chain.rows.size());
            if (belowNumber < 0)
                break;
            const QTextBlock below = m_doc->findBlockByNumber(belowNumber);
            if (!below.isValid())
                break;

            int foldStartNumber = -1;
            QList<int> group;
            QTextBlock it = below;
            for (int i = 0; i < kMaxHeaderRows && it.isValid(); ++i, it = it.next()) {
                if (TextEditor::TextBlockUserData::canFold(it)) {
                    group = FoldingScanner::headerRows(it);
                    if (!group.isEmpty() && group.first() <= belowNumber)
                        foldStartNumber = it.blockNumber();
                    break;
                }
            }
            if (foldStartNumber < 0)
                break;
            if (foldStartNumber == chain.innermostFoldStart)
                break;
            bool alreadyShown = false;
            for (const int row : std::as_const(group))
                alreadyShown = alreadyShown || chain.rows.contains(row);
            if (alreadyShown)
                break;
            if (chain.rows.size() + group.size() > m_maxLines)
                break;

            const std::optional<qreal> headerTop = m_geometry.blockTop(group.first());
            if (!headerTop || *headerTop > chain.rows.size() * rowHeight)
                break;

            qCDebug(stickyLog).noquote()
                << "capture: below" << belowNumber + 1
                << "group" << FoldingScanner::logLines(group).join(',')
                << "foldStart" << foldStartNumber + 1
                << "headerTop" << *headerTop
                << "slotTop" << chain.rows.size() * rowHeight;

            chain.rows += group;
            chain.innermostRowCount = group.size();
            chain.innermostFoldStart = foldStartNumber;
        }
    }

    qreal pushOffsetFor(const ScopeChain &chain, qreal fullHeight)
    {
        if (chain.innermostFoldStart < 0 || chain.innermostRowCount <= 0)
            return 0;

        const QTextBlock start = m_doc->findBlockByNumber(chain.innermostFoldStart);
        if (!start.isValid())
            return 0;

        const int startIndent = TextEditor::TextBlockUserData::foldingIndent(start);
        const int lastVisible = m_geometry.lastVisibleBlock();
        QTextBlock it = start.next();
        for (; it.isValid(); it = it.next()) {
            if (TextEditor::TextBlockUserData::foldingIndent(it) <= startIndent)
                break;
            if (it.blockNumber() > lastVisible)
                return 0;
        }
        if (!it.isValid())
            return 0;

        const QTextBlock anchorBlock = it.text().trimmed().startsWith(QLatin1Char('}'))
                                           ? it
                                           : it.previous();
        if (!anchorBlock.isValid() || anchorBlock.blockNumber() <= chain.innermostFoldStart)
            return 0;
        const std::optional<qreal> anchorTop = m_geometry.blockTop(anchorBlock.blockNumber());
        if (!anchorTop)
            return 0;
        const qreal intrusion = fullHeight - kPanelBorderPx - *anchorTop;

        if (intrusion > 0) {
            qCDebug(stickyLog).noquote()
                << "push: scope start line" << chain.innermostFoldStart + 1
                << "indent" << startIndent
                << "| anchor line" << anchorBlock.blockNumber() + 1
                << "text" << anchorBlock.text().trimmed().left(30)
                << "| anchorTop" << *anchorTop
                << "fullHeight" << fullHeight
                << "intrusion" << intrusion;
        }

        const qreal maxPush = m_geometry.rowHeight() * chain.innermostRowCount;
        return intrusion >= maxPush ? maxPush : 0;
    }

    const QTextDocument *m_doc;
    const Geometry &m_geometry;
    const int m_maxLines;
    Model m_model;
    QHash<int, ScopeChain> m_memo;
};

template <ViewportGeometry Geometry, ScopeModel Model = BraceScopeModel>
PanelState computePanelState(const QTextDocument *doc, const Geometry &geometry, int maxLines,
                             Model model = {})
{
    return PanelStateEngine<Geometry, Model>(doc, geometry, maxLines, std::move(model)).compute();
}

}
