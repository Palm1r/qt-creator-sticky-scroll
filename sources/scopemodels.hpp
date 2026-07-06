// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#pragma once

#include "foldingscanner.hpp"
#include "stickyscrolltypes.hpp"
#include "symbolindex.hpp"

#include <texteditor/textdocumentlayout.h>

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

struct BraceScopeModel
{
    static constexpr bool braceDynamics = true;

    ScopeChain enclosing(const QTextDocument *doc, int blockNumber, int maxLines) const
    {
        return FoldingScanner::enclosingHeaders(doc, blockNumber, maxLines);
    }
    QList<int> headerRows(const QTextBlock &block) const
    {
        return FoldingScanner::headerRows(block);
    }
    int pushAnchorBlock(const QTextDocument *doc, int foldStartBlockNumber,
                        int lastVisibleBlockNumber) const
    {
        return FoldingScanner::braceScopeAnchor(doc, foldStartBlockNumber, lastVisibleBlockNumber);
    }
    bool isFoldStart(const QTextBlock &block) const
    {
        return TextEditor::TextBlockUserData::canFold(block);
    }
};

struct RefinedBraceScopeModel
{
    static constexpr bool braceDynamics = true;

    SymbolIndex symbols;

    ScopeChain enclosing(const QTextDocument *doc, int blockNumber, int maxLines) const
    {
        ScopeChain chain = FoldingScanner::enclosingHeaders(
            doc, blockNumber, maxLines,
            [this](const QTextBlock &foldStart) { return symbols.headerRowsFor(foldStart); });
        return symbols.mergeEnclosingScopes(doc, std::move(chain), blockNumber, maxLines);
    }
    QList<int> headerRows(const QTextBlock &block) const
    {
        QList<int> rows = symbols.headerRowsFor(block);
        if (!rows.isEmpty())
            return rows;
        return FoldingScanner::headerRows(block);
    }
    int pushAnchorBlock(const QTextDocument *doc, int foldStartBlockNumber,
                        int lastVisibleBlockNumber) const
    {
        const QTextBlock start = doc->findBlockByNumber(foldStartBlockNumber);
        const int symbolEnd = start.isValid() ? symbols.scopeEndFor(start) : -1;
        if (symbolEnd >= 0)
            return symbolEnd > lastVisibleBlockNumber ? -1 : symbolEnd;
        return FoldingScanner::braceScopeAnchor(doc, foldStartBlockNumber, lastVisibleBlockNumber);
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
    QList<int> headerRows(const QTextBlock &block) const
    {
        return {block.blockNumber()};
    }
    int pushAnchorBlock(const QTextDocument *, int, int) const { return -1; }
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
    QList<int> headerRows(const QTextBlock &block) const
    {
        return {block.blockNumber()};
    }
    int pushAnchorBlock(const QTextDocument *, int, int) const { return -1; }
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
    QList<int> headerRows(const QTextBlock &block) const
    {
        return FoldingScanner::headerRows(block);
    }
    int pushAnchorBlock(const QTextDocument *doc, int foldStartBlockNumber,
                        int lastVisibleBlockNumber) const
    {
        return FoldingScanner::braceScopeAnchor(doc, foldStartBlockNumber, lastVisibleBlockNumber);
    }
    bool isFoldStart(const QTextBlock &block) const
    {
        return TextEditor::TextBlockUserData::canFold(block);
    }
};

}
