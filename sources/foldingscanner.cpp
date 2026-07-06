// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#include "foldingscanner.hpp"

#include <texteditor/textdocumentlayout.h>

#include <algorithm>
#include <utility>

using namespace TextEditor;

namespace StickyScroll {

Q_LOGGING_CATEGORY(stickyLog, "qtc.stickyscroll", QtWarningMsg)

int FoldingScanner::parenBalance(const QTextBlock &block)
{
    const Parentheses parens = TextBlockUserData::parentheses(block);
    if (!parens.isEmpty()) {
        int balance = 0;
        for (const Parenthesis &paren : parens) {
            if (paren.chr == QLatin1Char('('))
                ++balance;
            else if (paren.chr == QLatin1Char(')'))
                --balance;
        }
        return balance;
    }
    const QString text = block.text();
    return static_cast<int>(text.count(QLatin1Char('(')) - text.count(QLatin1Char(')')));
}

QTextBlock FoldingScanner::declarationAbove(const QTextBlock &brace)
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

bool FoldingScanner::startsInitializerToken(const QString &trimmed)
{
    if (trimmed.startsWith(QLatin1Char(',')))
        return true;
    return trimmed.startsWith(QLatin1Char(':')) && !trimmed.startsWith(QLatin1String("::"));
}

QTextBlock FoldingScanner::skipInitializerClause(const QTextBlock &headerBottom,
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

QList<int> FoldingScanner::headerRows(const QTextBlock &foldStart)
{
    const bool braceOnOwnLine = foldStart.text().trimmed() == QLatin1String("{");
    const QTextBlock candidate = braceOnOwnLine ? declarationAbove(foldStart) : foldStart;
    const QTextBlock headerBottom = skipInitializerClause(candidate, braceOnOwnLine);

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

ScopeChain FoldingScanner::enclosingHeaders(const QTextDocument *doc, int blockNumber, int maxLines,
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

int FoldingScanner::braceScopeAnchor(const QTextDocument *doc, int foldStartBlockNumber,
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

int FoldingScanner::leadingIndentColumns(const QTextBlock &block)
{
    const QString text = block.text();
    int spaces = 0;
    while (spaces < text.size()
           && (text.at(spaces) == QLatin1Char(' ') || text.at(spaces) == QLatin1Char('\t')))
        ++spaces;
    if (spaces == text.size())
        return -1;
    const bool sequenceEntry = text.at(spaces) == QLatin1Char('-')
                               && (spaces + 1 == text.size()
                                   || text.at(spaces + 1) == QLatin1Char(' '));
    return 2 * spaces + (sequenceEntry ? 1 : 0);
}

bool FoldingScanner::isIndentationFoldStart(const QTextBlock &block)
{
    const int indent = leadingIndentColumns(block);
    if (indent < 0)
        return false;
    for (QTextBlock line = block.next(); line.isValid(); line = line.next()) {
        const int lineIndent = leadingIndentColumns(line);
        if (lineIndent < 0)
            continue;
        return lineIndent > indent;
    }
    return false;
}

ScopeChain FoldingScanner::indentationHeaders(const QTextDocument *doc, int blockNumber, int maxLines)
{
    const QTextBlock block = doc->findBlockByNumber(blockNumber);
    if (!block.isValid())
        return {};

    int targetIndent = leadingIndentColumns(block);
    for (QTextBlock line = block.next(); targetIndent < 0 && line.isValid(); line = line.next())
        targetIndent = leadingIndentColumns(line);
    if (targetIndent <= 0)
        return {};

    QList<int> headersInnermostFirst;
    ScopeChain chain;
    int enclosingIndent = targetIndent;
    for (QTextBlock line = block.previous(); line.isValid() && enclosingIndent > 0;
         line = line.previous()) {
        const int lineIndent = leadingIndentColumns(line);
        if (lineIndent < 0 || lineIndent >= enclosingIndent)
            continue;
        if (headersInnermostFirst.isEmpty())
            chain.innermostFoldStart = line.blockNumber();
        headersInnermostFirst.append(line.blockNumber());
        enclosingIndent = lineIndent;
    }
    if (headersInnermostFirst.isEmpty())
        return chain;

    chain.innermostRowCount = 1;
    for (const int header : std::as_const(headersInnermostFirst)) {
        if (chain.rows.size() + 1 > maxLines)
            break;
        chain.rows.prepend(header);
    }
    return chain;
}

int FoldingScanner::atxHeadingLevel(const QTextBlock &block)
{
    const QString text = block.text();
    int i = 0;
    while (i < text.size() && i < 3 && text.at(i) == QLatin1Char(' '))
        ++i;
    int hashes = 0;
    while (i + hashes < text.size() && text.at(i + hashes) == QLatin1Char('#'))
        ++hashes;
    if (hashes == 0 || hashes > 6)
        return 0;
    const int after = i + hashes;
    if (after == text.size() || text.at(after) == QLatin1Char(' ')
        || text.at(after) == QLatin1Char('\t'))
        return hashes;
    return 0;
}

bool FoldingScanner::isFenceMarker(const QTextBlock &block)
{
    const QString trimmed = block.text().trimmed();
    return trimmed.startsWith(QLatin1String("```")) || trimmed.startsWith(QLatin1String("~~~"));
}

bool FoldingScanner::insideFence(const QTextBlock &block)
{
    const QTextDocument *doc = block.document();
    if (!doc)
        return false;
    bool inFence = false;
    for (QTextBlock line = doc->firstBlock();
         line.isValid() && line.blockNumber() < block.blockNumber();
         line = line.next()) {
        if (isFenceMarker(line))
            inFence = !inFence;
    }
    return inFence;
}

ScopeChain FoldingScanner::headingHeaders(const QTextDocument *doc, int blockNumber, int maxLines)
{
    const QTextBlock target = doc->findBlockByNumber(blockNumber);
    if (!target.isValid())
        return {};

    QList<std::pair<int, int>> stack;
    bool inFence = false;
    for (QTextBlock line = doc->firstBlock();
         line.isValid() && line.blockNumber() < blockNumber;
         line = line.next()) {
        if (isFenceMarker(line)) {
            inFence = !inFence;
            continue;
        }
        if (inFence)
            continue;
        const int level = atxHeadingLevel(line);
        if (level == 0)
            continue;
        while (!stack.isEmpty() && stack.last().first >= level)
            stack.removeLast();
        stack.append({level, line.blockNumber()});
    }

    const int targetLevel = inFence ? 0 : atxHeadingLevel(target);
    if (targetLevel > 0) {
        while (!stack.isEmpty() && stack.last().first >= targetLevel)
            stack.removeLast();
    }
    if (stack.isEmpty())
        return {};

    ScopeChain chain;
    chain.innermostFoldStart = stack.last().second;
    chain.innermostRowCount = 1;
    for (int i = (std::max)(0, int(stack.size()) - maxLines); i < stack.size(); ++i)
        chain.rows.append(stack.at(i).second);
    return chain;
}

bool FoldingScanner::isHeadingFoldStart(const QTextBlock &block)
{
    if (insideFence(block))
        return false;
    const int level = atxHeadingLevel(block);
    if (level == 0)
        return false;
    for (QTextBlock line = block.next(); line.isValid(); line = line.next()) {
        if (line.text().trimmed().isEmpty())
            continue;
        const int lineLevel = atxHeadingLevel(line);
        if (lineLevel == 0)
            return true;
        return lineLevel > level;
    }
    return false;
}

QStringList FoldingScanner::logLines(const QList<int> &blockNumbers)
{
    QStringList result;
    for (const int number : blockNumbers)
        result << QString::number(number + 1);
    return result;
}

}
