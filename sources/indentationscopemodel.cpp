// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#include "indentationscopemodel.hpp"

#include <QString>
#include <QTextBlock>
#include <QTextDocument>

#include <utility>

namespace StickyScroll {

static int leadingIndentColumns(const QTextBlock &block)
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

static bool isIndentationFoldStart(const QTextBlock &block)
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

static ScopeChain indentationHeaders(const QTextDocument *doc, int blockNumber, int maxLines)
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

ScopeChain IndentationScopeModel::enclosing(const QTextDocument *doc, int blockNumber,
                                            int maxLines) const
{
    return indentationHeaders(doc, blockNumber, maxLines);
}

bool IndentationScopeModel::isFoldStart(const QTextBlock &block) const
{
    return isIndentationFoldStart(block);
}

}
