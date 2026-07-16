// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#pragma once

#include <utils/aspects.h>

namespace StickyScroll {

class StickyScrollSettings final : public Utils::AspectContainer
{
public:
    StickyScrollSettings();

    Utils::BoolAspect enabled = {this};
    Utils::IntegerAspect maxLines = {this};
    Utils::BoolAspect followHorizontalScroll = {this};
    Utils::BoolAspect deEmphasizeScopes = {this};
    Utils::IntegerAspect deEmphasisIntensity = {this};
    Utils::IntegerAspect panelTint = {this};
};

StickyScrollSettings &settings();

}
