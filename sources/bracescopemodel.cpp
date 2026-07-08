// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#include "bracescopemodel.hpp"

#include <texteditor/textdocumentlayout.h>

#include <utility>

using namespace TextEditor;

namespace StickyScroll {

QTextBlock BraceScanner::declarationAbove(const QTextBlock &brace)
{
    const auto isBlank = [](const QTextBlock &block) {
        return block.text().trimmed().isEmpty();
    };
    QTextBlock line = brace.previous();
    for (int skipped = 0;
         skipped < kHeaderBlankLookback && line.isValid() && isBlank(line);
         ++skipped) {
        line = line.previous();
    }
    return line.isValid() && !isBlank(line) ? line : brace;
}

bool BraceScanner::startsInitializerToken(const QString &trimmed)
{
    if (trimmed.startsWith(QLatin1Char(',')))
        return true;
    return trimmed.startsWith(QLatin1Char(':')) && !trimmed.startsWith(QLatin1String("::"));
}

QTextBlock BraceScanner::skipInitializerClause(const QTextBlock &headerBottom,
                                               bool allowTrailingStyle)
{
    QTextBlock line = headerBottom;
    for (int steps = 0; steps < kInitializerLookback; ++steps) {
        const QTextBlock above = line.previous();
        if (!above.isValid())
            break;
        const QString aboveText = above.text().trimmed();
        if (aboveText.isEmpty())
            break;
        const QString text = line.text().trimmed();
        bool continuation = startsInitializerToken(text) || startsInitializerToken(aboveText);
        if (!continuation && allowTrailingStyle && !text.endsWith(QLatin1Char(':'))) {
            continuation = aboveText.endsWith(QLatin1String("),"))
                           || aboveText.endsWith(QLatin1String("},"))
                           || (aboveText.endsWith(QLatin1Char(':'))
                               && !aboveText.endsWith(QLatin1String("::"))
                               && aboveText.contains(QLatin1Char(')')));
        }
        if (!continuation)
            break;
        line = above;
    }
    return line;
}

QList<int> BraceScanner::headerRows(const QTextBlock &foldStart)
{
    const bool braceOnOwnLine = foldStart.text().trimmed() == QLatin1String("{");
    const QTextBlock candidate = braceOnOwnLine ? declarationAbove(foldStart) : foldStart;
    const QTextBlock headerBottom = braceOnOwnLine ? skipInitializerClause(candidate, braceOnOwnLine)
                                                   : candidate;

    QList<int> rows{headerBottom.blockNumber()};
    int unmatchedCloseParens = -parenBalance(headerBottom);
    QTextBlock line = headerBottom;
    while (unmatchedCloseParens > 0 && rows.size() < kMaxHeaderRows) {
        line = line.previous();
        if (!line.isValid())
            break;
        rows.prepend(line.blockNumber());
        unmatchedCloseParens -= parenBalance(line);
    }
    return rows;
}

ScopeChain BraceScanner::enclosingHeaders(const QTextDocument *doc, int blockNumber, int maxLines,
                                          const HeaderResolver &resolver)
{
    const QTextBlock block = doc->findBlockByNumber(blockNumber);
    if (!block.isValid())
        return {};

    QList<QList<int>> headersInnermostFirst;
    ScopeChain chain;
    int enclosingIndent = TextBlockUserData::foldingIndent(block);
    for (QTextBlock line = block.previous(); line.isValid() && enclosingIndent > 0;
         line = line.previous()) {
        const int lineIndent = TextBlockUserData::foldingIndent(line);
        const bool opensEnclosingScope = lineIndent < enclosingIndent
                                         && TextBlockUserData::canFold(line);
        if (!opensEnclosingScope)
            continue;
        if (headersInnermostFirst.isEmpty())
            chain.innermostFoldStart = line.blockNumber();
        QList<int> rows;
        if (resolver)
            rows = resolver(line);
        if (rows.isEmpty())
            rows = headerRows(line);
        headersInnermostFirst.append(rows);
        enclosingIndent = lineIndent;
    }
    if (headersInnermostFirst.isEmpty())
        return chain;

    chain.innermostRowCount = headersInnermostFirst.first().size();

    for (const QList<int> &header : std::as_const(headersInnermostFirst)) {
        const bool fitsBudget = chain.rows.size() + header.size() <= maxLines;
        if (!fitsBudget)
            break;
        chain.rows = header + chain.rows;
    }
    if (chain.innermostRowCount > chain.rows.size()) {
        chain.innermostRowCount = 0;
        chain.innermostFoldStart = -1;
    }
    return chain;
}

int BraceScanner::braceScopeAnchor(const QTextDocument *doc, int foldStartBlockNumber,
                                   int lastVisibleBlockNumber)
{
    const QTextBlock start = doc->findBlockByNumber(foldStartBlockNumber);
    if (!start.isValid())
        return -1;

    const int startIndent = TextBlockUserData::foldingIndent(start);
    QTextBlock it = start.next();
    for (; it.isValid(); it = it.next()) {
        if (TextBlockUserData::foldingIndent(it) <= startIndent)
            break;
        if (it.blockNumber() > lastVisibleBlockNumber)
            return -1;
    }
    if (!it.isValid())
        return -1;

    const QTextBlock anchor = it.text().trimmed().startsWith(QLatin1Char('}')) ? it
                                                                               : it.previous();
    return anchor.isValid() ? anchor.blockNumber() : -1;
}

}
