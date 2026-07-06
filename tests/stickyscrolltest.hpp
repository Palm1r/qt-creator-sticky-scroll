// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#pragma once

#include <QObject>

namespace StickyScroll {

class StickyScrollTest final : public QObject
{
    Q_OBJECT

private slots:
    void testFlatDocument();
    void testNestedSameLineBraces();
    void testClosingBraceReportsOuterScopesOnly();
    void testBraceOnOwnLineShowsDeclaration();
    void testBraceOnOwnLineSkipsBlankLines();
    void testMultilineSignature();
    void testConstructorInitializerShowsSignature();
    void testConstructorCommaInitializersShowSignature();
    void testClassBaseClauseOnOwnLine();
    void testTrailingStyleInitializerList();
    void testWrappedInitializerArguments();
    void testMultilineSignatureWithInitializer();
    void testSymbolResolverRefinesHeader();
    void testSymbolResolverSkipsControlFlow();
    void testSymbolResolverPicksNearestSymbol();
    void testSymbolResolverExtendsWrappedSignature();
    void testSymbolResolverFallsBackOutsideSpans();
    void testRefinedModelPanelState();
    void testSymbolScopePinsThroughInitializerList();
    void testBudgetDropsWholeScopes();
    void testInnermostRowCountClampedToBudget();
    void testSiblingScopes();
    void testParenBalancePrefersHighlighterData();
    void testInvalidBlockNumber();
    void testPanelStateCheckpoints();
    void testPanelStateNoFlickerOnLinearScroll();
    void testChainProviderDecorator();
    void testIndentationDeepNestPinsOnReach();
    void testHeadingHeadersMarkdown();
    void testHeadingHeadersBudgetDropsOuterSections();
    void testHeadingHeadersSkipsFencedCode();
    void testHeadingHeadersTopLevelHasNoChain();
    void testMarkdownPanelStatePinsOnReach();
    void testIndentationHeadersYaml();
    void testIndentationHeadersFlushSequence();
    void testIndentationHeadersBudgetDropsOuterScopes();
    void testIndentationHeadersSkipsBlankLines();
    void testIndentationHeadersTopLevelHasNoChain();
    void testIndentationPanelStateNoFlicker();
};

}
