// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#include "stickyscrollsettings.hpp"
#include "stickyscrollwidget.hpp"
#include "symbolindex.hpp"

#ifdef WITH_TESTS
#include "stickyscrolltest.hpp"
#endif

#include <coreplugin/editormanager/documentmodel.h>
#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/icore.h>

#include <extensionsystem/iplugin.h>

#include <languageclient/client.h>
#include <languageclient/documentsymbolcache.h>
#include <languageclient/languageclientmanager.h>
#include <languageserverprotocol/languagefeatures.h>

#include <texteditor/textdocument.h>
#include <texteditor/texteditor.h>

#include <QPointer>
#include <QTranslator>

#include <variant>

using namespace Core;

namespace StickyScroll {

class SymbolSyncController final : public QObject
{
public:
    SymbolSyncController(TextEditor::TextEditorWidget *editorWidget, StickyScrollWidget *sticky)
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

private:
    void rewire()
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

    LanguageServerProtocol::DocumentUri documentUri() const
    {
        return m_client->hostPathToServerUri(m_editorWidget->textDocument()->filePath());
    }

    void request(LanguageClient::Schedule schedule)
    {
        if (!m_client)
            return;

        m_requestedRevision = m_editorWidget->document()->revision();
        m_client->documentSymbolCache()->requestSymbols(documentUri(), schedule);
    }

    void applySymbols(const LanguageServerProtocol::DocumentUri &uri,
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

    static void appendSpans(const QList<LanguageServerProtocol::DocumentSymbol> &symbols,
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

    static void appendSpans(const QList<LanguageServerProtocol::SymbolInformation> &symbols,
                            QList<SymbolSpan> &out)
    {
        for (const LanguageServerProtocol::SymbolInformation &symbol : symbols) {
            const LanguageServerProtocol::Range range = symbol.location().range();
            out.append({range.start().line(), range.start().line(), range.end().line()});
        }
    }

    TextEditor::TextEditorWidget *m_editorWidget;
    StickyScrollWidget *m_sticky;
    QPointer<LanguageClient::Client> m_client;
    int m_requestedRevision = -1;
};

class StickyScrollPlugin final : public ExtensionSystem::IPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QtCreatorPlugin" FILE "StickyScroll.json")

public:
    void initialize() final
    {
        installTranslator();

        connect(&settings().enabled, &Utils::BaseAspect::changed,
                this, &StickyScrollPlugin::applySettings);
        connect(&settings().maxLines, &Utils::BaseAspect::changed,
                this, &StickyScrollPlugin::applySettings);

        connect(EditorManager::instance(), &EditorManager::editorOpened,
                this, &StickyScrollPlugin::attachToEditor);

#ifdef WITH_TESTS
        addTest<StickyScrollTest>();
#endif
    }

    ShutdownFlag aboutToShutdown() final
    {
        if (m_translator)
            QCoreApplication::removeTranslator(m_translator);
        return SynchronousShutdown;
    }

private:
    void attachToEditor(IEditor *editor)
    {
        auto *editorWidget = TextEditor::TextEditorWidget::fromEditor(editor);
        if (!editorWidget)
            return;
        if (editorWidget->findChild<StickyScrollWidget *>(QString(), Qt::FindDirectChildrenOnly))
            return;
        auto *sticky = new StickyScrollWidget(editorWidget);
        connect(sticky, &StickyScrollWidget::navigateToLine, editorWidget, [editorWidget](int line) {
            editorWidget->gotoLine(line, 0, true, true);
            editorWidget->setFocus();
        });
        new SymbolSyncController(editorWidget, sticky);
        sticky->setStickyEnabled(settings().enabled());
    }

    void applySettings()
    {
        const bool enabled = settings().enabled();
        const QList<IEditor *> editors = DocumentModel::editorsForOpenedDocuments();
        for (IEditor *editor : editors) {
            auto *editorWidget = TextEditor::TextEditorWidget::fromEditor(editor);
            if (!editorWidget)
                continue;
            auto *sticky = editorWidget->findChild<StickyScrollWidget *>(
                QString(), Qt::FindDirectChildrenOnly);
            if (sticky) {
                sticky->setStickyEnabled(enabled);
                sticky->refresh();
            }
        }
    }

    void installTranslator()
    {
        auto translator = new QTranslator(this);
        if (translator->load(
                QLocale(Core::ICore::userInterfaceLanguage()),
                "StickyScroll",
                "_",
                ":/translations")) {
            QCoreApplication::installTranslator(translator);
            m_translator = translator;
        } else {
            delete translator;
        }
    }

    QTranslator *m_translator = nullptr;
};

}

#include <stickyscroll.moc>
