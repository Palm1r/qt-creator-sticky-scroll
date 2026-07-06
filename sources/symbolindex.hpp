// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#pragma once

#include "stickyscrolltypes.hpp"

#include <QHash>
#include <QList>
#include <QTextBlock>
#include <QTextDocument>

#include <utility>

namespace StickyScroll {

struct SymbolSpan
{
    int nameLine = -1;
    int firstLine = -1;
    int lastLine = -1;

    bool operator==(const SymbolSpan &other) const = default;
};

class SymbolIndex
{
public:
    SymbolIndex() = default;
    SymbolIndex(QList<SymbolSpan> initialSpans)
        : spans(std::move(initialSpans))
    {}

    QList<SymbolSpan> spans;

    bool isEmpty() const { return spans.isEmpty(); }
    const SymbolSpan *ownerOf(const QTextBlock &foldStart) const;
    QList<int> headerRowsFor(const QTextBlock &foldStart) const;
    int scopeEndFor(const QTextBlock &foldStart) const;
    ScopeChain mergeEnclosingScopes(const QTextDocument *doc, ScopeChain chain, int blockNumber,
                                    int maxLines) const;

    bool operator==(const SymbolIndex &other) const { return spans == other.spans; }

private:
    static QList<int> signatureRows(const QTextDocument *doc, const SymbolSpan &span);
    mutable QHash<int, int> m_ownerCache;
};

}
