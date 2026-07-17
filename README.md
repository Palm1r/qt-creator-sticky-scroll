# Qt Creator Sticky Scroll

[![Build plugin](https://github.com/Palm1r/qt-creator-sticky-scroll/actions/workflows/build_cmake.yml/badge.svg)](https://github.com/Palm1r/qt-creator-sticky-scroll/actions/workflows/build_cmake.yml)
![GitHub Downloads (all assets, all releases)](https://img.shields.io/github/downloads/Palm1r/qt-creator-sticky-scroll/total?color=41%2C173%2C71&label=downloads)

Pins the enclosing scope headers to the top of the editor while you scroll, so
the context of the code on screen — the function, class, block, key, or heading
you are inside — always stays visible. Click a pinned line to jump to it. Toggle
the feature and set the maximum number of pinned lines on the **Sticky Scroll**
page in the text editor settings.

The panel is visually separated from the code: it gets a subtle background tint
and a separation line, and the pinned lines themselves are dimmed on two levels
— the innermost scope slightly, the outer scopes more — so they read as context
rather than code. Hovering a pinned line restores its full contrast. Both the
tint and the dimming are configurable (see [Settings](#settings)).

<img width="600" height="305" alt="sticky-scroll-demo-600" src="https://github.com/user-attachments/assets/c7d105aa-b75f-493d-8bd7-10f8480c5b29" />

## Supported formats

Which lines get pinned is derived from the document structure; the strategy is
chosen automatically per file type:

- **Brace / fold-based** — C, C++, JavaScript/TypeScript, JSON, CMake, Python,
  and any other language whose Qt Creator highlighter provides code folding.
  Uses the editor's folding data; handles nested scopes, multi-line signatures,
  and brace-on-own-line.
- **YAML** — derived from indentation, since Qt Creator's YAML highlighter only
  folds block scalars, e.g. `jobs → build → steps → - name → run:`.
  Block-sequence entries flush with their parent key are handled.
- **Markdown** — derived from ATX heading levels (`#`, `##`, …), since the
  highlighter provides no folding, e.g. `# Title → ## Usage → ### Configuration`.
  `#` inside fenced code blocks is ignored.

## Settings

Configure the plugin under **Settings → Text Editor → Sticky Scroll**:

- **Enable sticky scroll** — turn the pinned headers on or off (enabled by
  default).
- **Maximum number of pinned lines** — how many scope headers may stay pinned at
  once (1–10, default 5).
- **Horizontal pinning** — should pinned follow to horizontal scroll
- **De-emphasize pinned scope lines** — dim the pinned lines so they read as
  context, not code: the innermost scope lightly, all outer scopes more
  (enabled by default). The percentage field scales the dimming strength
  (0–100%, default 100%); hovering a line always shows it at full contrast.
- **Panel background tint** — how strongly the panel background is tinted
  relative to the editor background (0–25%, default 6%; 0 disables the tint).
  The separation line and shadow are always drawn.

## Installation

Extension repository

From Qt Creator 20 you can find Sticky Scroll plugin in extension list in Qt Creator and install from there

Prebuilt plugins are attached to every
[release](https://github.com/Palm1r/qt-creator-sticky-scroll/releases).

1. Download the archive for your platform. It is named
   `StickyScroll-<tag>-QtC<version>-<platform>.7z` and must match your **exact**
   Qt Creator version (releases currently target Qt Creator 20.0.0).
2. In Qt Creator open the **About Plugins…** dialog (under **Help** on
   Windows/Linux, under the **Qt Creator** menu on macOS) and click
   **Install Plugin…**.
3. Select the downloaded `.7z` archive, confirm, and restart Qt Creator.

To build from source instead, follow the steps below.

## How to Build

Create a build directory and run

    cmake -DCMAKE_PREFIX_PATH=<path_to_qtcreator> -DCMAKE_BUILD_TYPE=RelWithDebInfo <path_to_plugin_source>
    cmake --build .

where `<path_to_qtcreator>` is the relative or absolute path to a Qt Creator build directory, or to a
combined binary and development package (Windows / Linux), or to the `Qt Creator.app/Contents/Resources/`
directory of a combined binary and development package (macOS), and `<path_to_plugin_source>` is the
relative or absolute path to this plugin directory.

## How to Run

From the command line run

    cmake --build . --target RunQtCreator

`RunQtCreator` is a custom CMake target that will use the <path to qtcreator> referenced above to
start the Qt Creator executable with the following parameters

    -pluginpath <path_to_plugin>

where `<path_to_plugin>` is the path to the resulting plugin library in the build directory
(`<plugin_build>/lib/qtcreator/plugins` on Windows and Linux,
`<plugin_build>/Qt Creator.app/Contents/PlugIns` on macOS).

You might want to add `-temporarycleansettings` (or `-tcs`) to ensure that the opened Qt Creator
instance cannot mess with your user-global Qt Creator settings.

## How to Test

The plugin ships unit tests for the scope-chain logic. They are compiled in only when the
`WITH_TESTS` option is enabled, so release binaries stay test-free. Configure a separate build
directory and run the tests through Qt Creator's plugin test runner:

    cmake -B build-tests -DWITH_TESTS=ON -DCMAKE_PREFIX_PATH=<path_to_qtcreator> <path_to_plugin_source>
    cmake --build build-tests
    "Qt Creator" -pluginpath <path_to_plugin> -test StickyScroll -tcs

where `<path_to_plugin>` is the plugin directory inside `build-tests` (see "How to Run" above).
The run prints QtTest output and exits; expect `Totals: ... 0 failed`.

The panel logs its state (scope chain, push offset, and capture decisions) under the
`qtc.stickyscroll` category. Enable that logging rule to debug the panel interactively:

    QT_LOGGING_RULES='qtc.stickyscroll.debug=true' \
        "Qt Creator" -pluginpath <path_to_plugin> -tcs <file>

## Support the development

If you find this project helpful, there are several ways you can support it:

- **Report issues** — if you encounter any bugs or have suggestions for
  improvements, please
  [open an issue](https://github.com/Palm1r/qt-creator-sticky-scroll/issues).
- **Contribute** — feel free to submit pull requests with bug fixes or new
  features.
- **Spread the word** — star the
  [GitHub repository](https://github.com/Palm1r/qt-creator-sticky-scroll) and
  share the plugin with your fellow developers.
- **Financial support** — if you'd like to support the development financially,
  you can make a donation using one of the following:
  - PayPal: [paypal.me/palm1r](https://www.paypal.com/paypalme/palm1r)
  - Bitcoin (BTC): `bc1qndq7f0mpnlya48vk7kugvyqj5w89xrg4wzg68t`
  - Ethereum (ETH): `0xA5e8c37c94b24e25F9f1f292a01AF55F03099D8D`
  - Litecoin (LTC): `ltc1qlrxnk30s2pcjchzx4qrxvdjt5gzuervy5mv0vy`
  - USDT (TRC20): `THdZrE7d6epW6ry98GA3MLXRjha1DjKtUx`

## License

[MIT](LICENSE) © 2026 Petr Mironychev

## Qt Creator components and attributions

Sticky Scroll is a plugin for Qt Creator and incorporates certain
components (plugin templates, API headers, and related boilerplate) originating
from Qt Creator, which are copyright (C) The Qt Company Ltd.

These components are provided by The Qt Company under the GNU General Public
License version 3, annotated with The Qt Company GPL Exception 1.0. This
exception permits the development and distribution of Qt Creator plugins under
licenses of the plugin author's own choosing, notwithstanding the GPL's general
linking requirements. It is this exception that allows Sticky Scroll
to be distributed under the MIT license.

The original copyright and license notices of The Qt Company are preserved in
the relevant source files and must not be removed.
