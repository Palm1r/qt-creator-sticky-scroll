// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#pragma once

#include "stickyscrolltypes.hpp"

#include <QList>
#include <QTextBlock>
#include <QTextDocument>

#include <concepts>

namespace StickyScroll {

template <typename T>
concept ScopeModel = requires(const T model, const QTextDocument *doc, int blockNumber,
                              const QTextBlock &block) {
    { model.enclosing(doc, blockNumber, blockNumber) } -> std::same_as<ScopeChain>;
    { model.headerRows(block) } -> std::same_as<QList<int>>;
    { model.pushAnchorBlock(doc, blockNumber, blockNumber) } -> std::convertible_to<int>;
    { model.isFoldStart(block) } -> std::convertible_to<bool>;
    { T::braceDynamics } -> std::convertible_to<bool>;
};

}
