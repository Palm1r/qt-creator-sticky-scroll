// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#pragma once

#include "foldingscanner.hpp"
#include "stickyscrolltypes.hpp"

#include <texteditor/textdocumentlayout.h>

#include <QTextBlock>
#include <QTextDocument>

#include <concepts>

namespace StickyScroll {

template <typename T>
concept ScopeModel = requires(const T model, const QTextDocument *doc, int blockNumber,
                              const QTextBlock &block) {
    { model.enclosing(doc, blockNumber, blockNumber) } -> std::same_as<ScopeChain>;
    { model.isFoldStart(block) } -> std::convertible_to<bool>;
    { T::braceDynamics } -> std::convertible_to<bool>;
};

struct BraceScopeModel
{
    static constexpr bool braceDynamics = true;

    ScopeChain enclosing(const QTextDocument *doc, int blockNumber, int maxLines) const
    {
        return FoldingScanner::enclosingHeaders(doc, blockNumber, maxLines);
    }
    bool isFoldStart(const QTextBlock &block) const
    {
        return TextEditor::TextBlockUserData::canFold(block);
    }
};

struct IndentationScopeModel
{
    static constexpr bool braceDynamics = false;

    ScopeChain enclosing(const QTextDocument *doc, int blockNumber, int maxLines) const
    {
        return FoldingScanner::indentationHeaders(doc, blockNumber, maxLines);
    }
    bool isFoldStart(const QTextBlock &block) const
    {
        return FoldingScanner::isIndentationFoldStart(block);
    }
};

struct MarkdownScopeModel
{
    static constexpr bool braceDynamics = false;

    ScopeChain enclosing(const QTextDocument *doc, int blockNumber, int maxLines) const
    {
        return FoldingScanner::headingHeaders(doc, blockNumber, maxLines);
    }
    bool isFoldStart(const QTextBlock &block) const
    {
        return FoldingScanner::isHeadingFoldStart(block);
    }
};

struct ProviderScopeModel
{
    static constexpr bool braceDynamics = true;

    FoldingScanner::ChainProvider provider;

    ScopeChain enclosing(const QTextDocument *, int blockNumber, int maxLines) const
    {
        return provider(blockNumber, maxLines);
    }
    bool isFoldStart(const QTextBlock &block) const
    {
        return TextEditor::TextBlockUserData::canFold(block);
    }
};

}
