// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#pragma once

#include "scanutil.hpp"
#include "scopemodel.hpp"
#include "stickyscrolltypes.hpp"
#include "symbolindex.hpp"

#include <texteditor/textdocumentlayout.h>

#include <QList>
#include <QTextBlock>
#include <QTextDocument>

#include <functional>
#include <utility>

namespace StickyScroll {

class BraceScanner
{
public:
    using HeaderResolver = std::function<QList<int>(const QTextBlock &foldStart)>;

    static QList<int> headerRows(const QTextBlock &foldStart);
    static ScopeChain enclosingHeaders(const QTextDocument *doc, int blockNumber, int maxLines,
                                       const HeaderResolver &resolver = {});
    static int braceScopeAnchor(const QTextDocument *doc, int foldStartBlockNumber,
                                int lastVisibleBlockNumber);

private:
    static QTextBlock declarationAbove(const QTextBlock &brace);
    static bool startsInitializerToken(const QString &trimmed);
    static QTextBlock skipInitializerClause(const QTextBlock &headerBottom, bool allowTrailingStyle);
};

struct BraceScopeModel
{
    static constexpr bool braceDynamics = true;

    ScopeChain enclosing(const QTextDocument *doc, int blockNumber, int maxLines) const
    {
        return BraceScanner::enclosingHeaders(doc, blockNumber, maxLines);
    }
    QList<int> headerRows(const QTextBlock &block) const
    {
        return BraceScanner::headerRows(block);
    }
    int pushAnchorBlock(const QTextDocument *doc, int foldStartBlockNumber,
                        int lastVisibleBlockNumber) const
    {
        return BraceScanner::braceScopeAnchor(doc, foldStartBlockNumber, lastVisibleBlockNumber);
    }
    bool isFoldStart(const QTextBlock &block) const
    {
        return TextEditor::TextBlockUserData::canFold(block);
    }
};

struct RefinedBraceScopeModel
{
    static constexpr bool braceDynamics = true;

    SymbolIndex symbols;

    ScopeChain enclosing(const QTextDocument *doc, int blockNumber, int maxLines) const
    {
        ScopeChain chain = BraceScanner::enclosingHeaders(
            doc, blockNumber, maxLines,
            [this](const QTextBlock &foldStart) { return symbols.headerRowsFor(foldStart); });
        return symbols.mergeEnclosingScopes(doc, std::move(chain), blockNumber, maxLines);
    }
    QList<int> headerRows(const QTextBlock &block) const
    {
        QList<int> rows = symbols.headerRowsFor(block);
        if (!rows.isEmpty())
            return rows;
        return BraceScanner::headerRows(block);
    }
    int pushAnchorBlock(const QTextDocument *doc, int foldStartBlockNumber,
                        int lastVisibleBlockNumber) const
    {
        const QTextBlock start = doc->findBlockByNumber(foldStartBlockNumber);
        const int symbolEnd = start.isValid() ? symbols.scopeEndFor(start) : -1;
        if (symbolEnd >= 0)
            return symbolEnd > lastVisibleBlockNumber ? -1 : symbolEnd;
        return BraceScanner::braceScopeAnchor(doc, foldStartBlockNumber, lastVisibleBlockNumber);
    }
    bool isFoldStart(const QTextBlock &block) const
    {
        return TextEditor::TextBlockUserData::canFold(block);
    }
};

static_assert(ScopeModel<BraceScopeModel>);
static_assert(ScopeModel<RefinedBraceScopeModel>);

}
