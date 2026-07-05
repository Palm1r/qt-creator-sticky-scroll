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
};

StickyScrollSettings &settings();

}
