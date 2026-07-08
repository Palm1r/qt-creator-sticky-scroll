// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#pragma once

#include "scopemodel.hpp"
#include "stickyscrolltypes.hpp"

#include <QList>
#include <QTextBlock>
#include <QTextDocument>

namespace StickyScroll {

struct MarkdownScopeModel
{
    static constexpr bool braceDynamics = false;

    ScopeChain enclosing(const QTextDocument *doc, int blockNumber, int maxLines) const;
    QList<int> headerRows(const QTextBlock &block) const { return {block.blockNumber()}; }
    int pushAnchorBlock(const QTextDocument *, int, int) const { return -1; }
    bool isFoldStart(const QTextBlock &block) const;
};

static_assert(ScopeModel<MarkdownScopeModel>);

}
