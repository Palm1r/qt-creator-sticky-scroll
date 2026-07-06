// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#pragma once

#include <QList>
#include <QLoggingCategory>

namespace StickyScroll {

Q_DECLARE_LOGGING_CATEGORY(stickyLog)

inline constexpr int kMaxHeaderRows = 4;
inline constexpr int kHeaderBlankLookback = 3;
inline constexpr int kInitializerLookback = 16;
inline constexpr qreal kPanelBorderPx = 1;

struct ScopeChain
{
    QList<int> rows;
    int innermostRowCount = 0;
    int innermostFoldStart = -1;

    int firstInnermostRow() const { return rows.size() - innermostRowCount; }

    bool operator==(const ScopeChain &other) const = default;
};

struct PanelState
{
    ScopeChain chain;
    qreal pushOffset = 0;

    qreal fullHeight(qreal rowHeight) const
    {
        return rowHeight * chain.rows.size() + kPanelBorderPx;
    }

    bool operator==(const PanelState &other) const = default;
};

}
