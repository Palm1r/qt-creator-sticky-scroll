# Sticky Scroll

[![Build plugin](https://github.com/Palm1r/qt-creator-sticky-scroll/actions/workflows/build_cmake.yml/badge.svg)](https://github.com/Palm1r/qt-creator-sticky-scroll/actions/workflows/build_cmake.yml)

Pins the enclosing scope headers to the top of the editor while you scroll, so
the context of the code on screen — the function, class, block, key, or heading
you are inside — always stays visible. Click a pinned line to jump to it. Toggle
the feature and set the maximum number of pinned lines on the **Sticky Scroll**
page in the text editor settings.

<!-- TODO: add a screenshot or GIF at docs/screenshot.png, then uncomment: -->
<!-- ![StickyScroll in action](docs/screenshot.png) -->

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

## Installation

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

## License

[MIT](LICENSE) © 2026 Petr Mironychev
