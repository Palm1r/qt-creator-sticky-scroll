// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#pragma once

#include "stickyscrolltypes.hpp"

#include <QRect>

namespace StickyScroll {

enum class PanelAction { Hide, Keep, Repaint };

struct PaintedSnapshot
{
    QRect geometry;
    PanelState state;
    int revision = -1;
    bool visible = false;
};

struct PanelLayout
{
    PanelAction action = PanelAction::Hide;
    QRect panel;
    QRect shadow;
    bool resetHover = false;
};

inline PanelLayout reconcilePanel(const PaintedSnapshot &painted, const PanelState &state,
                                  const QRect &viewportRect, qreal rowHeight, int revision,
                                  int shadowHeight)
{
    if (state.chain.rows.isEmpty())
        return {PanelAction::Hide, {}, {}, true};

    const qreal fullHeight = state.fullHeight(rowHeight);
    const int panelHeight = qRound(fullHeight - state.pushOffset);
    const int panelWidth = viewportRect.x() + viewportRect.width();
    const QRect panel(0, viewportRect.y(), panelWidth, panelHeight);
    const QRect shadow(0, viewportRect.y() + panelHeight, panelWidth, shadowHeight);

    const bool resetHover = state.chain.rows != painted.state.chain.rows;
    const bool unchanged = painted.visible && panel == painted.geometry
                           && state == painted.state && revision == painted.revision;
    return {unchanged ? PanelAction::Keep : PanelAction::Repaint, panel, shadow, resetHover};
}

}
