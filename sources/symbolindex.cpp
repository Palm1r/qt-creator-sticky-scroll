// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#include "symbolindex.hpp"

#include "scanutil.hpp"

#include <texteditor/textdocumentlayout.h>

#include <algorithm>
#include <utility>

namespace StickyScroll {

const SymbolSpan *SymbolIndex::ownerOf(const QTextBlock &foldStart) const
{
    const int foldLine = foldStart.blockNumber();
    const auto cached = m_ownerCache.constFind(foldLine);
    if (cached != m_ownerCache.constEnd())
        return *cached >= 0 ? &m_spans.at(*cached) : nullptr;

    const auto remember = [&](const SymbolSpan *result) -> const SymbolSpan * {
        m_ownerCache.insert(foldLine, result ? int(result - m_spans.constData()) : -1);
        return result;
    };

    const SymbolSpan *owner = nullptr;
    for (const SymbolSpan &span : m_spans) {
        if (foldLine < span.firstLine || foldLine > span.lastLine)
            continue;
        const int nameDistance = foldLine - span.nameLine;
        if (nameDistance < 0 || nameDistance > kInitializerLookback)
            continue;
        if (!owner || span.nameLine > owner->nameLine)
            owner = &span;
    }
    if (!owner)
        return remember(nullptr);

    const QTextDocument *doc = foldStart.document();
    const QTextBlock nameBlock = doc->findBlockByNumber(owner->nameLine);
    if (!nameBlock.isValid())
        return remember(nullptr);
    for (QTextBlock between = nameBlock.next();
         between.isValid() && between.blockNumber() < foldLine;
         between = between.next()) {
        if (TextEditor::TextBlockUserData::canFold(between))
            return remember(nullptr);
    }
    return remember(owner);
}

QList<int> SymbolIndex::headerRowsFor(const QTextBlock &foldStart) const
{
    const SymbolSpan *owner = ownerOf(foldStart);
    if (!owner)
        return {};
    return signatureRows(foldStart.document(), *owner);
}

int SymbolIndex::scopeEndFor(const QTextBlock &foldStart) const
{
    const SymbolSpan *owner = ownerOf(foldStart);
    return owner ? owner->lastLine : -1;
}

ScopeChain SymbolIndex::mergeEnclosingScopes(const QTextDocument *doc, ScopeChain chain,
                                             int blockNumber, int maxLines) const
{
    QList<const SymbolSpan *> enclosing;
    for (const SymbolSpan &span : m_spans) {
        if (span.nameLine < blockNumber && blockNumber <= span.lastLine)
            enclosing.append(&span);
    }
    std::sort(enclosing.begin(), enclosing.end(),
              [](const SymbolSpan *a, const SymbolSpan *b) { return a->nameLine > b->nameLine; });

    for (const SymbolSpan *span : std::as_const(enclosing)) {
        if (chain.rows.contains(span->nameLine))
            continue;
        const QList<int> rows = signatureRows(doc, *span);
        if (rows.isEmpty())
            continue;
        if (chain.rows.size() + rows.size() > maxLines)
            continue;
        int position = 0;
        while (position < chain.rows.size() && chain.rows.at(position) < rows.first())
            ++position;
        for (int i = 0; i < rows.size(); ++i)
            chain.rows.insert(position + i, rows.at(i));
        if (span->nameLine > chain.innermostFoldStart) {
            chain.innermostFoldStart = span->nameLine;
            chain.innermostRowCount = rows.size();
        }
    }
    return chain;
}

QList<int> SymbolIndex::signatureRows(const QTextDocument *doc, const SymbolSpan &span)
{
    const QTextBlock nameBlock = doc->findBlockByNumber(span.nameLine);
    if (!nameBlock.isValid())
        return {};

    QList<int> rows{span.nameLine};
    int openParens = parenBalance(nameBlock);
    QTextBlock line = nameBlock;
    while (openParens > 0 && rows.size() < kMaxHeaderRows) {
        line = line.next();
        if (!line.isValid() || line.blockNumber() > span.lastLine)
            break;
        rows.append(line.blockNumber());
        openParens += parenBalance(line);
    }
    if (openParens > 0)
        return {span.nameLine};
    return rows;
}

}
