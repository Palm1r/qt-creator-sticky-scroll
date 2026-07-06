// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#include "stickyscrollsettings.hpp"
#include "stickyscrollwidget.hpp"

#ifdef WITH_TESTS
#include "stickyscrolltest.hpp"
#endif

#include <coreplugin/editormanager/documentmodel.h>
#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/icore.h>

#include <extensionsystem/iplugin.h>

#include <texteditor/texteditor.h>

#include <QTranslator>

using namespace Core;

namespace StickyScroll {

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
