// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#include "markdownscopemodel.hpp"

#include <QString>
#include <QTextBlock>
#include <QTextDocument>

#include <algorithm>
#include <utility>

namespace StickyScroll {

static int atxHeadingLevel(const QTextBlock &block)
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

static bool isFenceMarker(const QTextBlock &block)
{
    const QString trimmed = block.text().trimmed();
    return trimmed.startsWith(QLatin1String("```")) || trimmed.startsWith(QLatin1String("~~~"));
}

static bool insideFence(const QTextBlock &block)
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

static ScopeChain headingHeaders(const QTextDocument *doc, int blockNumber, int maxLines)
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

static bool isHeadingFoldStart(const QTextBlock &block)
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

ScopeChain MarkdownScopeModel::enclosing(const QTextDocument *doc, int blockNumber,
                                         int maxLines) const
{
    return headingHeaders(doc, blockNumber, maxLines);
}

bool MarkdownScopeModel::isFoldStart(const QTextBlock &block) const
{
    return isHeadingFoldStart(block);
}

}
