// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#pragma once

#include "stickyscrolltypes.hpp"

#include <QList>
#include <QStringList>
#include <QTextBlock>
#include <QTextDocument>

#include <functional>

namespace StickyScroll {

class FoldingScanner
{
public:
    using ChainProvider = std::function<ScopeChain(int blockNumber, int maxLines)>;
    using HeaderResolver = std::function<QList<int>(const QTextBlock &foldStart)>;

    static int parenBalance(const QTextBlock &block);
    static QList<int> headerRows(const QTextBlock &foldStart);
    static ScopeChain enclosingHeaders(const QTextDocument *doc, int blockNumber, int maxLines,
                                       const HeaderResolver &resolver = {});
    static int braceScopeAnchor(const QTextDocument *doc, int foldStartBlockNumber,
                                int lastVisibleBlockNumber);
    static ScopeChain indentationHeaders(const QTextDocument *doc, int blockNumber, int maxLines);
    static bool isIndentationFoldStart(const QTextBlock &block);
    static ScopeChain headingHeaders(const QTextDocument *doc, int blockNumber, int maxLines);
    static bool isHeadingFoldStart(const QTextBlock &block);
    static QStringList logLines(const QList<int> &blockNumbers);

private:
    static QTextBlock declarationAbove(const QTextBlock &brace);
    static bool startsInitializerToken(const QString &trimmed);
    static QTextBlock skipInitializerClause(const QTextBlock &headerBottom, bool allowTrailingStyle);
    static int leadingIndentColumns(const QTextBlock &block);
    static int atxHeadingLevel(const QTextBlock &block);
    static bool isFenceMarker(const QTextBlock &block);
    static bool insideFence(const QTextBlock &block);
};

}
