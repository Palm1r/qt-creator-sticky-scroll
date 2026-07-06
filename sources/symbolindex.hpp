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
        : m_spans(std::move(initialSpans))
    {}

    const QList<SymbolSpan> &spans() const { return m_spans; }
    void setSpans(QList<SymbolSpan> spans)
    {
        m_spans = std::move(spans);
        m_ownerCache.clear();
    }

    bool isEmpty() const { return m_spans.isEmpty(); }
    const SymbolSpan *ownerOf(const QTextBlock &foldStart) const;
    QList<int> headerRowsFor(const QTextBlock &foldStart) const;
    int scopeEndFor(const QTextBlock &foldStart) const;
    ScopeChain mergeEnclosingScopes(const QTextDocument *doc, ScopeChain chain, int blockNumber,
                                    int maxLines) const;

    bool operator==(const SymbolIndex &other) const { return m_spans == other.m_spans; }

private:
    static QList<int> signatureRows(const QTextDocument *doc, const SymbolSpan &span);
    QList<SymbolSpan> m_spans;
    mutable QHash<int, int> m_ownerCache;
};

}
