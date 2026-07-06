// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#include "stickyscrolltest.hpp"

#include "panelstateengine.hpp"
#include "symbolindex.hpp"

#include <texteditor/textdocumentlayout.h>

#include <QTest>
#include <QTextBlock>
#include <QTextDocument>

#include <algorithm>

using namespace TextEditor;

namespace StickyScroll {

struct Line
{
    QString text;
    int foldingIndent = 0;
};

static void fillDocument(QTextDocument &doc, const QList<Line> &lines)
{
    QStringList texts;
    for (const Line &line : lines)
        texts << line.text;
    doc.setPlainText(texts.join(QLatin1Char('\n')));

    QTextBlock block = doc.firstBlock();
    for (const Line &line : lines) {
        TextBlockUserData::setFoldingIndent(block, line.foldingIndent);
        block = block.next();
    }
}

static void fillText(QTextDocument &doc, const QStringList &lines)
{
    doc.setPlainText(lines.join(QLatin1Char('\n')));
}

void StickyScrollTest::testFlatDocument()
{
    QTextDocument doc;
    fillDocument(doc, {{"int a;", 0}, {"int b;", 0}, {"int c;", 0}});

    const ScopeChain chain = FoldingScanner::enclosingHeaders(&doc, 2, 5);
    QVERIFY(chain.rows.isEmpty());
    QCOMPARE(chain.innermostFoldStart, -1);
}

void StickyScrollTest::testNestedSameLineBraces()
{
    QTextDocument doc;
    fillDocument(doc,
                 {{"namespace X {", 0},
                  {"void foo() {", 1},
                  {"    body;", 2},
                  {"}", 1},
                  {"", 1}});

    const ScopeChain chain = FoldingScanner::enclosingHeaders(&doc, 2, 5);
    QCOMPARE(chain.rows, (QList<int>{0, 1}));
    QCOMPARE(chain.innermostFoldStart, 1);
    QCOMPARE(chain.innermostRowCount, 1);
}

void StickyScrollTest::testClosingBraceReportsOuterScopesOnly()
{
    QTextDocument doc;
    fillDocument(doc,
                 {{"namespace X {", 0},
                  {"void foo() {", 1},
                  {"    body;", 2},
                  {"}", 1},
                  {"", 1}});

    const ScopeChain chain = FoldingScanner::enclosingHeaders(&doc, 3, 5);
    QCOMPARE(chain.rows, (QList<int>{0}));
}

void StickyScrollTest::testBraceOnOwnLineShowsDeclaration()
{
    QTextDocument doc;
    fillDocument(doc,
                 {{"namespace X {", 0},
                  {"void foo()", 1},
                  {"{", 1},
                  {"    body;", 2},
                  {"}", 1}});

    const ScopeChain chain = FoldingScanner::enclosingHeaders(&doc, 3, 5);
    QCOMPARE(chain.rows, (QList<int>{0, 1}));
    QCOMPARE(chain.innermostFoldStart, 2);
    QCOMPARE(chain.innermostRowCount, 1);
}

void StickyScrollTest::testBraceOnOwnLineSkipsBlankLines()
{
    QTextDocument doc;
    fillDocument(doc,
                 {{"void foo()", 0},
                  {"", 0},
                  {"", 0},
                  {"{", 0},
                  {"    body;", 1}});

    const QTextBlock foldStart = doc.findBlockByNumber(3);
    QCOMPARE(FoldingScanner::headerRows(foldStart), (QList<int>{0}));
}

void StickyScrollTest::testMultilineSignature()
{
    QTextDocument doc;
    fillDocument(doc,
                 {{"void foo(int a,", 0},
                  {"         int b)", 0},
                  {"{", 0},
                  {"    body;", 1}});

    const ScopeChain chain = FoldingScanner::enclosingHeaders(&doc, 3, 5);
    QCOMPARE(chain.rows, (QList<int>{0, 1}));
    QCOMPARE(chain.innermostRowCount, 2);
    QCOMPARE(chain.innermostFoldStart, 2);
}

void StickyScrollTest::testConstructorInitializerShowsSignature()
{
    QTextDocument doc;
    fillDocument(doc,
                 {{"TestConstructor()", 0},
                  {"    : member()", 0},
                  {"{", 0},
                  {"    body;", 1},
                  {"}", 0}});

    const ScopeChain chain = FoldingScanner::enclosingHeaders(&doc, 3, 5);
    QCOMPARE(chain.rows, (QList<int>{0}));
    QCOMPARE(chain.innermostFoldStart, 2);
    QCOMPARE(chain.innermostRowCount, 1);
}

void StickyScrollTest::testConstructorCommaInitializersShowSignature()
{
    QTextDocument doc;
    fillDocument(doc,
                 {{"Foo::Foo()", 0},
                  {"    : a(1)", 0},
                  {"    , b(2)", 0},
                  {"{", 0},
                  {"    body;", 1},
                  {"}", 0}});

    const ScopeChain chain = FoldingScanner::enclosingHeaders(&doc, 4, 5);
    QCOMPARE(chain.rows, (QList<int>{0}));
}

void StickyScrollTest::testClassBaseClauseOnOwnLine()
{
    QTextDocument doc;
    fillDocument(doc,
                 {{"class Foo", 0},
                  {"    : public Bar", 0},
                  {"{", 0},
                  {"    int m;", 1},
                  {"};", 0}});

    const ScopeChain chain = FoldingScanner::enclosingHeaders(&doc, 3, 5);
    QCOMPARE(chain.rows, (QList<int>{0}));
}

void StickyScrollTest::testTrailingStyleInitializerList()
{
    QTextDocument doc;
    fillDocument(doc,
                 {{"Foo::Foo() :", 0},
                  {"    a(1),", 0},
                  {"    b(2)", 0},
                  {"{", 0},
                  {"    body;", 1},
                  {"}", 0}});

    const ScopeChain chain = FoldingScanner::enclosingHeaders(&doc, 4, 5);
    QCOMPARE(chain.rows, (QList<int>{0}));
}

void StickyScrollTest::testWrappedInitializerArguments()
{
    QTextDocument doc;
    fillDocument(doc,
                 {{"Foo::Foo()", 0},
                  {"    : base(a,", 0},
                  {"           b)", 0},
                  {"{", 0},
                  {"    body;", 1},
                  {"}", 0}});

    const ScopeChain chain = FoldingScanner::enclosingHeaders(&doc, 4, 5);
    QCOMPARE(chain.rows, (QList<int>{0}));
}

void StickyScrollTest::testMultilineSignatureWithInitializer()
{
    QTextDocument doc;
    fillDocument(doc,
                 {{"Foo::Foo(int x,", 0},
                  {"         int y)", 0},
                  {"    : a(x)", 0},
                  {"{", 0},
                  {"    body;", 1},
                  {"}", 0}});

    const ScopeChain chain = FoldingScanner::enclosingHeaders(&doc, 4, 5);
    QCOMPARE(chain.rows, (QList<int>{0, 1}));
    QCOMPARE(chain.innermostRowCount, 2);
}

static FoldingScanner::HeaderResolver resolverFor(const SymbolIndex &index)
{
    return [index](const QTextBlock &foldStart) { return index.headerRowsFor(foldStart); };
}

void StickyScrollTest::testSymbolResolverRefinesHeader()
{
    QTextDocument doc;
    fillDocument(doc,
                 {{"void test()", 0},
                  {"FANCY_MACRO", 0},
                  {"{", 0},
                  {"    body;", 1},
                  {"}", 0}});

    QCOMPARE(FoldingScanner::enclosingHeaders(&doc, 3, 5).rows, (QList<int>{1}));

    const SymbolIndex index{{{0, 0, 4}}};
    const ScopeChain chain = FoldingScanner::enclosingHeaders(&doc, 3, 5, resolverFor(index));
    QCOMPARE(chain.rows, (QList<int>{0}));
}

void StickyScrollTest::testSymbolResolverSkipsControlFlow()
{
    QTextDocument doc;
    fillDocument(doc,
                 {{"void test()", 0},
                  {"{", 0},
                  {"    if (x) {", 1},
                  {"        body;", 2},
                  {"    }", 1},
                  {"}", 0}});

    const SymbolIndex index{{{0, 0, 5}}};
    const ScopeChain chain = FoldingScanner::enclosingHeaders(&doc, 3, 5, resolverFor(index));
    QCOMPARE(chain.rows, (QList<int>{0, 2}));
    QCOMPARE(chain.innermostFoldStart, 2);
}

void StickyScrollTest::testSymbolResolverPicksNearestSymbol()
{
    QTextDocument doc;
    fillDocument(doc,
                 {{"class Foo", 0},
                  {"{", 0},
                  {"    void method() {", 1},
                  {"        body;", 2},
                  {"    }", 1},
                  {"};", 0}});

    const SymbolIndex index{{{0, 0, 5}, {2, 2, 4}}};
    const ScopeChain chain = FoldingScanner::enclosingHeaders(&doc, 3, 5, resolverFor(index));
    QCOMPARE(chain.rows, (QList<int>{0, 2}));
}

void StickyScrollTest::testSymbolResolverExtendsWrappedSignature()
{
    QTextDocument doc;
    fillDocument(doc,
                 {{"Foo::Foo(int x,", 0},
                  {"         int y)", 0},
                  {"    : member()", 0},
                  {"{", 0},
                  {"    body;", 1},
                  {"}", 0}});

    const SymbolIndex index{{{0, 0, 5}}};
    const ScopeChain chain = FoldingScanner::enclosingHeaders(&doc, 4, 5, resolverFor(index));
    QCOMPARE(chain.rows, (QList<int>{0, 1}));
    QCOMPARE(chain.innermostRowCount, 2);
}

void StickyScrollTest::testSymbolResolverFallsBackOutsideSpans()
{
    QTextDocument doc;
    fillDocument(doc,
                 {{"void test()", 0},
                  {"FANCY_MACRO", 0},
                  {"{", 0},
                  {"    body;", 1},
                  {"}", 0}});

    const SymbolIndex index{{{10, 10, 20}}};
    const ScopeChain chain = FoldingScanner::enclosingHeaders(&doc, 3, 5, resolverFor(index));
    QCOMPARE(chain.rows, (QList<int>{1}));
}

void StickyScrollTest::testBudgetDropsWholeScopes()
{
    QTextDocument doc;
    fillDocument(doc,
                 {{"void a() {", 0},
                  {"    void b() {", 1},
                  {"        void c() {", 2},
                  {"            body;", 3}});

    const ScopeChain chain = FoldingScanner::enclosingHeaders(&doc, 3, 2);
    QCOMPARE(chain.rows, (QList<int>{1, 2}));
    QCOMPARE(chain.innermostFoldStart, 2);

    const ScopeChain full = FoldingScanner::enclosingHeaders(&doc, 3, 5);
    QCOMPARE(full.rows, (QList<int>{0, 1, 2}));
}

void StickyScrollTest::testInnermostRowCountClampedToBudget()
{
    QTextDocument doc;
    fillDocument(doc,
                 {{"void foo(int a,", 0},
                  {"         int b)", 0},
                  {"{", 0},
                  {"    body;", 1}});

    const ScopeChain chain = FoldingScanner::enclosingHeaders(&doc, 3, 1);
    QVERIFY(chain.rows.isEmpty());
    QCOMPARE(chain.innermostRowCount, 0);
    QVERIFY(chain.firstInnermostRow() >= 0);
}

void StickyScrollTest::testSiblingScopes()
{
    QTextDocument doc;
    fillDocument(doc,
                 {{"namespace X {", 0},
                  {"void a() {", 1},
                  {"    body;", 2},
                  {"}", 1},
                  {"void b() {", 1},
                  {"    body;", 2},
                  {"}", 1}});

    const ScopeChain chain = FoldingScanner::enclosingHeaders(&doc, 5, 5);
    QCOMPARE(chain.rows, (QList<int>{0, 4}));
    QCOMPARE(chain.innermostFoldStart, 4);
}

void StickyScrollTest::testParenBalancePrefersHighlighterData()
{
    QTextDocument doc;
    fillDocument(doc, {{"foo(\"(\")", 0}});

    const QTextBlock block = doc.firstBlock();
    QCOMPARE(FoldingScanner::parenBalance(block), 1);

    Parentheses parens;
    parens << Parenthesis(Parenthesis::Opened, QLatin1Char('('), 3)
           << Parenthesis(Parenthesis::Closed, QLatin1Char(')'), 7);
    TextBlockUserData::setParentheses(block, parens);
    QCOMPARE(FoldingScanner::parenBalance(block), 0);
}

void StickyScrollTest::testInvalidBlockNumber()
{
    QTextDocument doc;
    fillDocument(doc, {{"int a;", 0}});

    QVERIFY(FoldingScanner::enclosingHeaders(&doc, -1, 5).rows.isEmpty());
    QVERIFY(FoldingScanner::enclosingHeaders(&doc, 100, 5).rows.isEmpty());
}

class FakeGeometry
{
public:
    FakeGeometry(int blockCount, qreal rowHeight, qreal viewportHeight)
        : m_blockCount(blockCount)
        , m_rowHeight(rowHeight)
        , m_viewportHeight(viewportHeight)
    {}

    qreal top = 0;

    int blockForVisibleRow(int row) const
    {
        const int block = int(top / m_rowHeight) + row;
        return block < m_blockCount ? block : -1;
    }

    int lastVisibleBlock() const
    {
        return (std::min)(int((top + m_viewportHeight) / m_rowHeight), m_blockCount - 1);
    }

    std::optional<qreal> blockTop(int blockNumber) const
    {
        if (blockNumber < 0 || blockNumber >= m_blockCount)
            return std::nullopt;
        return blockNumber * m_rowHeight - top;
    }

    qreal rowHeight() const { return m_rowHeight; }

private:
    const int m_blockCount;
    const qreal m_rowHeight;
    const qreal m_viewportHeight;
};

static QList<Line> scrollScenarioDocument()
{
    return {{"namespace X {", 0},
            {"void foo(int a,", 1},
            {"         int b)", 1},
            {"{", 1},
            {"    if (c) {", 2},
            {"        s1;", 3},
            {"        s2;", 3},
            {"    }", 2},
            {"    after;", 2},
            {"}", 1},
            {"tail1;", 1},
            {"tail2;", 1},
            {"tail3;", 1},
            {"tail4;", 1},
            {"tail5;", 1},
            {"}", 0}};
}

void StickyScrollTest::testPanelStateCheckpoints()
{
    QTextDocument doc;
    const QList<Line> lines = scrollScenarioDocument();
    fillDocument(doc, lines);
    FakeGeometry geometry(lines.size(), 10, 60);

    const auto stateAt = [&](qreal top) {
        geometry.top = top;
        return computePanelState(&doc, geometry, 5);
    };

    PanelState state = stateAt(0);
    QCOMPARE(state.chain.rows, (QList<int>{0, 1, 2}));
    QCOMPARE(state.pushOffset, 0.0);

    state = stateAt(10);
    QCOMPARE(state.chain.rows, (QList<int>{0, 1, 2, 4}));
    QCOMPARE(state.pushOffset, 0.0);

    state = stateAt(30);
    QCOMPARE(state.chain.rows, (QList<int>{0, 1, 2, 4}));
    QCOMPARE(state.pushOffset, 0.0);

    state = stateAt(35);
    QCOMPARE(state.chain.rows, (QList<int>{0, 1, 2, 4}));
    QCOMPARE(state.pushOffset, 0.0);

    state = stateAt(40);
    QCOMPARE(state.chain.rows, (QList<int>{0, 1, 2}));
    QCOMPARE(state.pushOffset, 0.0);

    state = stateAt(50);
    QCOMPARE(state.chain.rows, (QList<int>{0, 1, 2, 4}));
    QCOMPARE(state.pushOffset, 10.0);

    state = stateAt(70);
    QCOMPARE(state.chain.rows, (QList<int>{0, 1, 2}));
    QCOMPARE(state.pushOffset, 0.0);

    state = stateAt(90);
    QCOMPARE(state.chain.rows, (QList<int>{0}));
    QCOMPARE(state.pushOffset, 0.0);
}

void StickyScrollTest::testPanelStateNoFlickerOnLinearScroll()
{
    QTextDocument doc;
    const QList<Line> lines = scrollScenarioDocument();
    fillDocument(doc, lines);
    FakeGeometry geometry(lines.size(), 10, 60);

    QHash<int, QList<bool>> shownByBlock;
    const int steps = 100;
    for (int step = 0; step <= steps; ++step) {
        geometry.top = step;
        const PanelState state = computePanelState(&doc, geometry, 5);
        const int hiddenRows = int(state.pushOffset / geometry.rowHeight());
        const QList<int> &rows = state.chain.rows;
        QSet<int> visible;
        for (int i = 0; i < rows.size(); ++i) {
            const bool slidOut = i >= rows.size() - hiddenRows
                                 && state.pushOffset >= geometry.rowHeight();
            if (!slidOut)
                visible.insert(rows.at(i));
        }
        for (int block = 0; block < lines.size(); ++block)
            shownByBlock[block].append(visible.contains(block));
    }

    for (auto it = shownByBlock.constBegin(); it != shownByBlock.constEnd(); ++it) {
        const QList<bool> &timeline = it.value();
        int transitionsToShown = 0;
        for (int i = 1; i < timeline.size(); ++i) {
            if (timeline.at(i) && !timeline.at(i - 1))
                ++transitionsToShown;
        }
        if (timeline.first())
            ++transitionsToShown;
        QVERIFY2(transitionsToShown <= 1,
                 qPrintable(QString("block %1 flickered: shown %2 times")
                                .arg(it.key())
                                .arg(transitionsToShown)));
    }
}

void StickyScrollTest::testChainProviderDecorator()
{
    QTextDocument doc;
    const QList<Line> lines = scrollScenarioDocument();
    fillDocument(doc, lines);
    FakeGeometry geometry(lines.size(), 10, 60);
    geometry.top = 50;

    const FoldingScanner::ChainProvider onlyOutermost = [&doc](int blockNumber, int maxLines) {
        ScopeChain chain = FoldingScanner::enclosingHeaders(&doc, blockNumber, maxLines);
        if (chain.rows.size() > 1) {
            chain.rows = {chain.rows.first()};
            chain.innermostRowCount = 1;
            chain.innermostFoldStart = chain.rows.first();
        }
        return chain;
    };

    const PanelState state = computePanelState(&doc, geometry, 5,
                                               ProviderScopeModel{onlyOutermost});
    QCOMPARE(state.chain.rows, (QList<int>{0}));
}

void StickyScrollTest::testRefinedModelPanelState()
{
    QTextDocument doc;
    const QList<Line> lines = {{"void test()", 0},
                               {"FANCY_MACRO", 0},
                               {"{", 0},
                               {"    body1;", 1},
                               {"    body2;", 1},
                               {"    body3;", 1},
                               {"    body4;", 1},
                               {"}", 0}};
    fillDocument(doc, lines);
    FakeGeometry geometry(lines.size(), 10, 40);
    geometry.top = 30;

    const SymbolIndex index{{{0, 0, 7}}};
    const PanelState state
        = computePanelState(&doc, geometry, 5, RefinedBraceScopeModel{index});
    QCOMPARE(state.chain.rows, (QList<int>{0}));
}

void StickyScrollTest::testSymbolScopePinsThroughInitializerList()
{
    QTextDocument doc;
    const QList<Line> lines = {{"LLMClientInterface::LLMClientInterface(", 0},
                               {"    const A &a,", 1},
                               {"    const B &b,", 1},
                               {"    C &c,", 1},
                               {"    D &d,", 1},
                               {"    E &e,", 1},
                               {"    F &f)", 1},
                               {"    : m_a(a)", 0},
                               {"    , m_b(b)", 0},
                               {"    , m_c(c)", 0},
                               {"{}", 0},
                               {"", 0},
                               {"void after() {", 0},
                               {"    body;", 1},
                               {"}", 0}};
    fillDocument(doc, lines);
    FakeGeometry geometry(lines.size(), 10, 40);
    const SymbolIndex index{{{0, 0, 10}, {12, 12, 14}}};

    const auto stateAt = [&](qreal top) {
        geometry.top = top;
        return computePanelState(&doc, geometry, 5, RefinedBraceScopeModel{index});
    };

    PanelState state = stateAt(30);
    QCOMPARE(state.chain.rows, (QList<int>{0}));
    QCOMPARE(state.pushOffset, 0.0);

    state = stateAt(60);
    QCOMPARE(state.chain.rows, (QList<int>{0}));
    QCOMPARE(state.pushOffset, 0.0);

    state = stateAt(80);
    QCOMPARE(state.chain.rows, (QList<int>{0}));
    QCOMPARE(state.pushOffset, 0.0);

    state = stateAt(100);
    QCOMPARE(state.pushOffset, 10.0);

    state = stateAt(110);
    QVERIFY(state.chain.rows.isEmpty());
}

static QStringList yamlDocument()
{
    return {"jobs:",
            "  build:",
            "    steps:",
            "      - name: Checkout",
            "        uses: actions/checkout",
            "      - name: Build",
            "        run: make",
            "  deploy:",
            "    needs: build"};
}

void StickyScrollTest::testIndentationDeepNestPinsOnReach()
{
    QTextDocument doc;
    fillText(doc,
             {"jobs:",
              "  build:",
              "    strategy:",
              "      matrix:",
              "        config:",
              "        - {",
              "            os: ubuntu,",
              "            name: Linux,",
              "          }",
              "    steps:",
              "      - run: x",
              "tail: done"});
    const int blocks = doc.blockCount();
    FakeGeometry geometry(blocks, 10, 100);

    QList<bool> configShown;
    QList<bool> entryShown;
    for (int top = 0; top <= blocks * 10; ++top) {
        geometry.top = top;
        const PanelState state = computePanelState(&doc, geometry, 5, IndentationScopeModel{});
        QCOMPARE(state.pushOffset, 0.0);
        if (int(top / 10) == 5)
            QVERIFY2(state.chain.rows.contains(5), "- { must pin the moment it is reached");
        configShown.append(state.chain.rows.contains(4));
        entryShown.append(state.chain.rows.contains(5));
    }

    const auto intervals = [](const QList<bool> &timeline) {
        int transitions = timeline.first() ? 1 : 0;
        for (int i = 1; i < timeline.size(); ++i)
            transitions += timeline.at(i) && !timeline.at(i - 1) ? 1 : 0;
        return transitions;
    };
    QVERIFY2(intervals(configShown) <= 1, "config: flickered");
    QVERIFY2(intervals(entryShown) <= 1, "- { flickered");
}

static QStringList markdownDocument()
{
    return {"# Title",
            "intro",
            "## A",
            "a-text",
            "### A1",
            "a1-text",
            "#### A1a",
            "deep-1",
            "deep-2",
            "## B",
            "b-text",
            "### B1",
            "b1-text"};
}

void StickyScrollTest::testHeadingHeadersMarkdown()
{
    QTextDocument doc;
    fillText(doc, markdownDocument());

    ScopeChain chain = FoldingScanner::headingHeaders(&doc, 7, 5);
    QCOMPARE(chain.rows, (QList<int>{0, 2, 4, 6}));
    QCOMPARE(chain.innermostFoldStart, 6);
    QCOMPARE(chain.innermostRowCount, 1);

    chain = FoldingScanner::headingHeaders(&doc, 10, 5);
    QCOMPARE(chain.rows, (QList<int>{0, 9}));
    QCOMPARE(chain.innermostFoldStart, 9);

    chain = FoldingScanner::headingHeaders(&doc, 6, 5);
    QCOMPARE(chain.rows, (QList<int>{0, 2, 4}));
}

void StickyScrollTest::testHeadingHeadersBudgetDropsOuterSections()
{
    QTextDocument doc;
    fillText(doc, markdownDocument());

    const ScopeChain chain = FoldingScanner::headingHeaders(&doc, 7, 2);
    QCOMPARE(chain.rows, (QList<int>{4, 6}));
    QCOMPARE(chain.innermostFoldStart, 6);
}

void StickyScrollTest::testHeadingHeadersSkipsFencedCode()
{
    QTextDocument doc;
    fillText(doc,
             {"# Title",
              "",
              "```sh",
              "# not a heading",
              "echo hi",
              "```",
              "text after"});

    QCOMPARE(FoldingScanner::headingHeaders(&doc, 6, 5).rows, (QList<int>{0}));
    QCOMPARE(FoldingScanner::headingHeaders(&doc, 4, 5).rows, (QList<int>{0}));
    QVERIFY(!FoldingScanner::isHeadingFoldStart(doc.findBlockByNumber(3)));
}

void StickyScrollTest::testHeadingHeadersTopLevelHasNoChain()
{
    QTextDocument doc;
    fillText(doc, {"preamble", "# Title", "body", "## Sub", "more"});

    QVERIFY(FoldingScanner::headingHeaders(&doc, 0, 5).rows.isEmpty());
    QVERIFY(FoldingScanner::headingHeaders(&doc, 1, 5).rows.isEmpty());
    QVERIFY(FoldingScanner::headingHeaders(&doc, -1, 5).rows.isEmpty());
    QVERIFY(FoldingScanner::headingHeaders(&doc, 100, 5).rows.isEmpty());
}

void StickyScrollTest::testMarkdownPanelStatePinsOnReach()
{
    QTextDocument doc;
    const QStringList lines = markdownDocument();
    fillText(doc, lines);
    FakeGeometry geometry(doc.blockCount(), 10, 100);

    QHash<int, QList<bool>> shownByBlock;
    for (int top = 0; top <= lines.size() * 10; ++top) {
        geometry.top = top;
        const PanelState state = computePanelState(&doc, geometry, 5, MarkdownScopeModel{});
        QCOMPARE(state.pushOffset, 0.0);
        if (int(top / 10) == 6)
            QVERIFY2(state.chain.rows.contains(6), "#### A1a must pin the moment it is reached");
        QSet<int> visible(state.chain.rows.constBegin(), state.chain.rows.constEnd());
        for (int block = 0; block < lines.size(); ++block)
            shownByBlock[block].append(visible.contains(block));
    }

    for (auto it = shownByBlock.constBegin(); it != shownByBlock.constEnd(); ++it) {
        const QList<bool> &timeline = it.value();
        int transitions = timeline.first() ? 1 : 0;
        for (int i = 1; i < timeline.size(); ++i)
            transitions += timeline.at(i) && !timeline.at(i - 1) ? 1 : 0;
        QVERIFY2(transitions <= 1,
                 qPrintable(QString("block %1 flickered: shown %2 times")
                                .arg(it.key()).arg(transitions)));
    }
}

void StickyScrollTest::testIndentationHeadersYaml()
{
    QTextDocument doc;
    fillText(doc, yamlDocument());

    ScopeChain chain = FoldingScanner::indentationHeaders(&doc, 4, 5);
    QCOMPARE(chain.rows, (QList<int>{0, 1, 2, 3}));
    QCOMPARE(chain.innermostFoldStart, 3);
    QCOMPARE(chain.innermostRowCount, 1);

    chain = FoldingScanner::indentationHeaders(&doc, 8, 5);
    QCOMPARE(chain.rows, (QList<int>{0, 7}));
    QCOMPARE(chain.innermostFoldStart, 7);

    chain = FoldingScanner::indentationHeaders(&doc, 1, 5);
    QCOMPARE(chain.rows, (QList<int>{0}));
}

void StickyScrollTest::testIndentationHeadersFlushSequence()
{
    QTextDocument doc;
    fillText(doc,
             {"jobs:",
              "  build:",
              "    name: xxx",
              "    runs-on: yyy",
              "    outputs:",
              "        tag: zzz",
              "    strategy:",
              "      matrix:",
              "        config:",
              "        - {",
              "            os: ubuntu,",
              "          }"});

    const ScopeChain onEntry = FoldingScanner::indentationHeaders(&doc, 9, 5);
    QCOMPARE(onEntry.rows, (QList<int>{0, 1, 6, 7, 8}));
    QCOMPARE(onEntry.innermostFoldStart, 8);

    const ScopeChain insideEntry = FoldingScanner::indentationHeaders(&doc, 10, 5);
    QCOMPARE(insideEntry.rows, (QList<int>{1, 6, 7, 8, 9}));
    QCOMPARE(insideEntry.innermostFoldStart, 9);

    FakeGeometry geometry(doc.blockCount(), 10, 60);
    geometry.top = 100;
    const PanelState live = computePanelState(&doc, geometry, 5, IndentationScopeModel{});
    QVERIFY2(live.chain.rows.contains(8), "config: must stay pinned inside the flow mapping");
}

void StickyScrollTest::testIndentationHeadersBudgetDropsOuterScopes()
{
    QTextDocument doc;
    fillText(doc, {"a:", "  b:", "    c:", "      d:", "        e: v"});

    const ScopeChain narrow = FoldingScanner::indentationHeaders(&doc, 4, 2);
    QCOMPARE(narrow.rows, (QList<int>{2, 3}));
    QCOMPARE(narrow.innermostFoldStart, 3);

    const ScopeChain wide = FoldingScanner::indentationHeaders(&doc, 4, 5);
    QCOMPARE(wide.rows, (QList<int>{0, 1, 2, 3}));
}

void StickyScrollTest::testIndentationHeadersSkipsBlankLines()
{
    QTextDocument doc;
    fillText(doc, {"parent:", "  child:", "", "    value: x"});

    const ScopeChain chain = FoldingScanner::indentationHeaders(&doc, 3, 5);
    QCOMPARE(chain.rows, (QList<int>{0, 1}));
    QCOMPARE(chain.innermostFoldStart, 1);
}

void StickyScrollTest::testIndentationHeadersTopLevelHasNoChain()
{
    QTextDocument doc;
    fillText(doc, yamlDocument());

    QVERIFY(FoldingScanner::indentationHeaders(&doc, 0, 5).rows.isEmpty());
    QVERIFY(FoldingScanner::indentationHeaders(&doc, -1, 5).rows.isEmpty());
    QVERIFY(FoldingScanner::indentationHeaders(&doc, 100, 5).rows.isEmpty());
}

void StickyScrollTest::testIndentationPanelStateNoFlicker()
{
    QTextDocument doc;
    const QList<Line> lines = {{"jobs:", 0},
                               {"  build:", 0},
                               {"    steps:", 0},
                               {"      - name: A", 0},
                               {"        run: |", 0},
                               {"          echo one", 1},
                               {"          echo two", 1},
                               {"      - name: B", 0},
                               {"        run: cmd", 0},
                               {"    post: done", 0},
                               {"  deploy:", 0},
                               {"    needs: build", 0},
                               {"tail: end", 0}};
    fillDocument(doc, lines);
    QVERIFY(TextBlockUserData::canFold(doc.findBlockByNumber(4)));
    FakeGeometry geometry(lines.size(), 10, 60);

    QHash<int, QList<bool>> shownByBlock;
    const int steps = 130;
    for (int step = 0; step <= steps; ++step) {
        geometry.top = step;
        const PanelState state = computePanelState(&doc, geometry, 5, IndentationScopeModel{});
        QCOMPARE(state.pushOffset, 0.0);
        QSet<int> visible(state.chain.rows.constBegin(), state.chain.rows.constEnd());
        for (int block = 0; block < lines.size(); ++block)
            shownByBlock[block].append(visible.contains(block));
    }

    for (auto it = shownByBlock.constBegin(); it != shownByBlock.constEnd(); ++it) {
        const QList<bool> &timeline = it.value();
        int transitionsToShown = 0;
        for (int i = 1; i < timeline.size(); ++i) {
            if (timeline.at(i) && !timeline.at(i - 1))
                ++transitionsToShown;
        }
        if (timeline.first())
            ++transitionsToShown;
        QVERIFY2(transitionsToShown <= 1,
                 qPrintable(QString("block %1 flickered: shown %2 times")
                                .arg(it.key())
                                .arg(transitionsToShown)));
    }
}

}
