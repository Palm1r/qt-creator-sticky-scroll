cmake_minimum_required(VERSION 3.16)

project(StickyScroll)

set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(QtCreator REQUIRED COMPONENTS Core)
find_package(Qt6 6.9 REQUIRED COMPONENTS Widgets LinguistTools)

qt_standard_project_setup(I18N_TRANSLATED_LANGUAGES
  en cs zh_CN zh_TW da de fr hr ja pl ru sl sv uk
)

option(WITH_TESTS "Builds with tests" NO)

if(WITH_TESTS)
  find_package(Qt6 REQUIRED COMPONENTS Test)
  enable_testing()
endif()

add_qtc_plugin(StickyScroll
  PLUGIN_DEPENDS
    QtCreator::Core
    QtCreator::TextEditor
    QtCreator::LanguageClient
  DEPENDS
    Qt::Widgets
    QtCreator::ExtensionSystem
    QtCreator::Utils
    QtCreator::LanguageServerProtocol
  SOURCES
    .github/workflows/build_cmake.yml
    .github/workflows/README.md
    README.md
    sources/bracescopemodel.cpp
    sources/bracescopemodel.hpp
    sources/indentationscopemodel.cpp
    sources/indentationscopemodel.hpp
    sources/markdownscopemodel.cpp
    sources/markdownscopemodel.hpp
    sources/panelstateengine.hpp
    sources/scanutil.cpp
    sources/scanutil.hpp
    sources/scopemodel.hpp
    sources/scopemodels.hpp
    sources/scoperesolver.hpp
    sources/stickyscroll.cpp
    sources/stickyscrolltypes.hpp
    sources/stickyscrollsettings.cpp
    sources/stickyscrollsettings.hpp
    sources/stickyscrolltr.hpp
    sources/stickyscrollwidget.cpp
    sources/stickyscrollwidget.hpp
    sources/symbolindex.cpp
    sources/symbolindex.hpp
    sources/symbolsynccontroller.cpp
    sources/symbolsynccontroller.hpp
)

target_include_directories(StickyScroll PRIVATE sources)

extend_qtc_plugin(StickyScroll
  CONDITION WITH_TESTS
  DEPENDS Qt::Test
  SOURCES
    tests/stickyscrolltest.cpp
    tests/stickyscrolltest.hpp
)

if(WITH_TESTS)
  target_include_directories(StickyScroll PRIVATE tests)
endif()

get_target_property(QtCreatorCorePath QtCreator::Core LOCATION)
find_program(QtCreatorExecutable
  NAMES
    qtcreator "Qt Creator"
  PATHS
    "${QtCreatorCorePath}/../../../bin"
    "${QtCreatorCorePath}/../../../MacOS"
  NO_DEFAULT_PATH
)
if (QtCreatorExecutable)
  add_custom_target(RunQtCreator
    COMMAND ${QtCreatorExecutable} -pluginpath $<TARGET_FILE_DIR:StickyScroll>
    DEPENDS StickyScroll
  )
  set_target_properties(RunQtCreator PROPERTIES FOLDER "qtc_runnable")
endif()

qt_add_translations(TARGETS StickyScroll
          TS_OUTPUT_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/resources/translations
          RESOURCE_PREFIX "/translations"
          LUPDATE_OPTIONS -no-obsolete -locations none
)