// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#include "stickyscrollsettings.hpp"

#include "stickyscrolltr.hpp"

#include <coreplugin/dialogs/ioptionspage.h>
#include <texteditor/texteditorconstants.h>
#include <utils/layoutbuilder.h>

namespace StickyScroll {

StickyScrollSettings &settings()
{
    static StickyScrollSettings theSettings;
    return theSettings;
}

StickyScrollSettings::StickyScrollSettings()
{
    setAutoApply(false);

    setSettingsGroup("StickyScroll");

    enabled.setSettingsKey("Enabled");
    enabled.setDefaultValue(true);
    enabled.setLabelText(Tr::tr("Enable sticky scroll"));

    maxLines.setSettingsKey("MaxLines");
    maxLines.setDefaultValue(5);
    maxLines.setRange(1, 10);
    maxLines.setLabelText(Tr::tr("Maximum number of pinned lines:"));

    followHorizontalScroll.setSettingsKey("FollowHorizontalScroll");
    followHorizontalScroll.setDefaultValue(false);
    followHorizontalScroll.setLabelText(
        Tr::tr("Scroll pinned lines horizontally with the editor"));

    setLayouter([this] {
        using namespace Layouting;
        return Column {
            enabled,
            Row { maxLines, st },
            followHorizontalScroll,
            st,
        };
    });

    readSettings();
}

class StickyScrollSettingsPage final : public Core::IOptionsPage
{
public:
    StickyScrollSettingsPage()
    {
        setId("StickyScroll.Settings");
        setDisplayName(Tr::tr("Sticky Scroll"));
        setCategory(TextEditor::Constants::TEXT_EDITOR_SETTINGS_CATEGORY);
        setSettingsProvider([] { return &settings(); });
    }
};

const StickyScrollSettingsPage settingsPage;

}
