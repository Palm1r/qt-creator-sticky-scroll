// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#include "symbolsynccontroller.hpp"

#include "stickyscrollwidget.hpp"

#include <languageclient/client.h>
#include <languageclient/documentsymbolcache.h>
#include <languageclient/languageclientmanager.h>

#include <texteditor/textdocument.h>
#include <texteditor/texteditor.h>

#include <variant>

namespace StickyScroll {

SymbolSyncController::SymbolSyncController(TextEditor::TextEditorWidget *editorWidget,
                                          StickyScrollWidget *sticky)
    : QObject(sticky)
    , m_editorWidget(editorWidget)
    , m_sticky(sticky)
{
    auto *manager = LanguageClient::LanguageClientManager::instance();
    connect(manager, &LanguageClient::LanguageClientManager::clientInitialized,
            this, &SymbolSyncController::rewire);
    connect(manager, &LanguageClient::LanguageClientManager::clientRemoved,
            this, &SymbolSyncController::rewire);
    rewire();
}

void SymbolSyncController::rewire()
{
    LanguageClient::Client *client
        = LanguageClient::LanguageClientManager::clientForDocument(
            m_editorWidget->textDocument());
    if (client == m_client)
        return;
    if (m_client) {
        disconnect(m_client, nullptr, this, nullptr);
        disconnect(m_client->documentSymbolCache(), nullptr, this, nullptr);
    }
    m_client = client;
    if (!m_client) {
        m_sticky->setSymbolSpans({}, -1);
        return;
    }
    connect(m_client, &LanguageClient::Client::documentUpdated,
            this, [this](TextEditor::TextDocument *document) {
                if (document == m_editorWidget->textDocument())
                    request(LanguageClient::Schedule::Delayed);
            });
    connect(m_client->documentSymbolCache(),
            &LanguageClient::DocumentSymbolCache::gotSymbols,
            this, &SymbolSyncController::applySymbols);
    request(LanguageClient::Schedule::Now);
}

LanguageServerProtocol::DocumentUri SymbolSyncController::documentUri() const
{
    return m_client->hostPathToServerUri(m_editorWidget->textDocument()->filePath());
}

void SymbolSyncController::request(LanguageClient::Schedule schedule)
{
    if (!m_client)
        return;

    m_requestedRevision = m_editorWidget->document()->revision();
    m_client->documentSymbolCache()->requestSymbols(documentUri(), schedule);
}

void SymbolSyncController::applySymbols(const LanguageServerProtocol::DocumentUri &uri,
                                        const LanguageServerProtocol::DocumentSymbolsResult &symbols)
{
    if (!m_client || uri != documentUri())
        return;
    QList<SymbolSpan> spans;
    if (const auto *tree
        = std::get_if<QList<LanguageServerProtocol::DocumentSymbol>>(&symbols)) {
        appendSpans(*tree, spans);
    } else if (const auto *flat
               = std::get_if<QList<LanguageServerProtocol::SymbolInformation>>(&symbols)) {
        appendSpans(*flat, spans);
    }
    m_sticky->setSymbolSpans(spans, m_requestedRevision);
}

void SymbolSyncController::appendSpans(const QList<LanguageServerProtocol::DocumentSymbol> &symbols,
                                       QList<SymbolSpan> &out)
{
    for (const LanguageServerProtocol::DocumentSymbol &symbol : symbols) {
        const LanguageServerProtocol::Range range = symbol.range();
        const LanguageServerProtocol::Range selection = symbol.selectionRange();
        out.append({selection.start().line(), range.start().line(), range.end().line()});
        if (const auto children = symbol.children())
            appendSpans(*children, out);
    }
}

void SymbolSyncController::appendSpans(const QList<LanguageServerProtocol::SymbolInformation> &symbols,
                                       QList<SymbolSpan> &out)
{
    for (const LanguageServerProtocol::SymbolInformation &symbol : symbols) {
        const LanguageServerProtocol::Range range = symbol.location().range();
        out.append({range.start().line(), range.start().line(), range.end().line()});
    }
}

}
