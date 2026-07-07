// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#pragma once

#include "symbolindex.hpp"

#include <languageclient/languageclientutils.h>
#include <languageserverprotocol/languagefeatures.h>
#include <languageserverprotocol/lsptypes.h>

#include <QList>
#include <QObject>
#include <QPointer>

namespace TextEditor { class TextEditorWidget; }
namespace LanguageClient { class Client; }

namespace StickyScroll {

class StickyScrollWidget;

class SymbolSyncController final : public QObject
{
    Q_OBJECT

public:
    SymbolSyncController(TextEditor::TextEditorWidget *editorWidget, StickyScrollWidget *sticky);

private:
    void rewire();
    LanguageServerProtocol::DocumentUri documentUri() const;
    void request(LanguageClient::Schedule schedule);
    void applySymbols(const LanguageServerProtocol::DocumentUri &uri,
                      const LanguageServerProtocol::DocumentSymbolsResult &symbols);

    static void appendSpans(const QList<LanguageServerProtocol::DocumentSymbol> &symbols,
                            QList<SymbolSpan> &out);
    static void appendSpans(const QList<LanguageServerProtocol::SymbolInformation> &symbols,
                            QList<SymbolSpan> &out);

    TextEditor::TextEditorWidget *m_editorWidget;
    StickyScrollWidget *m_sticky;
    QPointer<LanguageClient::Client> m_client;
    int m_requestedRevision = -1;
};

}
