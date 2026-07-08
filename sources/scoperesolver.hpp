// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#pragma once

#include "panelstateengine.hpp"
#include "scopemodels.hpp"
#include "symbolindex.hpp"

#include <QHash>
#include <QString>
#include <QTextDocument>

namespace StickyScroll {

enum class ScopeFormat { Brace, Indentation, Markdown };

inline constexpr ScopeFormat kDefaultScopeFormat = ScopeFormat::Brace;

inline ScopeFormat scopeFormatFor(const QString &mimeType)
{
    static const QHash<QString, ScopeFormat> byMime = {
        {"text/x-c++src", kDefaultScopeFormat},
        {"text/x-c++hdr", kDefaultScopeFormat},
        {"text/x-csrc", kDefaultScopeFormat},
        {"text/x-chdr", kDefaultScopeFormat},
        {"text/x-python", kDefaultScopeFormat},
        {"application/json", kDefaultScopeFormat},
        {"text/javascript", kDefaultScopeFormat},
        {"application/javascript", kDefaultScopeFormat},
        {"text/x-cmake", kDefaultScopeFormat},
        {"text/x-qml", kDefaultScopeFormat},
        {"text/x-yaml", ScopeFormat::Indentation},
        {"text/yaml", ScopeFormat::Indentation},
        {"application/x-yaml", ScopeFormat::Indentation},
        {"application/yaml", ScopeFormat::Indentation},
        {"text/markdown", ScopeFormat::Markdown},
        {"text/x-markdown", ScopeFormat::Markdown},
    };
    return byMime.value(mimeType, kDefaultScopeFormat);
}

struct ScopeContext
{
    QString mimeType;
    const SymbolIndex &symbols;
    int symbolsRevision = -1;
};

template <ViewportGeometry Geometry>
PanelState computePanelState(const QTextDocument *doc, const Geometry &geometry, int maxLines,
                             const ScopeContext &context)
{
    const ScopeFormat format = scopeFormatFor(context.mimeType);
    const bool symbolsFresh = format == ScopeFormat::Brace && !context.symbols.isEmpty()
                              && context.symbolsRevision == doc->revision();
    if (symbolsFresh)
        return computePanelState(doc, geometry, maxLines, RefinedBraceScopeModel{context.symbols});

    switch (format) {
    case ScopeFormat::Indentation:
        return computePanelState(doc, geometry, maxLines, IndentationScopeModel{});
    case ScopeFormat::Markdown:
        return computePanelState(doc, geometry, maxLines, MarkdownScopeModel{});
    case ScopeFormat::Brace:
        break;
    }
    return computePanelState(doc, geometry, maxLines);
}

}
