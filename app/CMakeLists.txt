qt_add_executable(lvdExplorer
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
)

# Generates a Version.h carrying LVDEXPLORER_VERSION_STRING from
# project(lvdExplorer VERSION ...) in the root CMakeLists.txt, so that's
# the single place a release bump needs to happen -- main.cpp and
# AboutDialog both read it live via QApplication::applicationVersion()
# rather than a second hardcoded literal.
configure_file(Version.h.in ${CMAKE_CURRENT_BINARY_DIR}/generated/Version.h @ONLY)
target_include_directories(lvdExplorer PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)

if(WIN32)
    # Embeds resources/branding/appicon.ico into the .exe itself, plus a
    # VERSIONINFO block (Explorer's Properties -> Details tab) generated
    # from the same project version -- distinct from the runtime
    # QApplication::setWindowIcon() call in main.cpp.
    set(LVDEXPLORER_APP_ICON_PATH "${CMAKE_SOURCE_DIR}/resources/branding/appicon.ico")
    configure_file(resources/app.rc.in ${CMAKE_CURRENT_BINARY_DIR}/app.rc @ONLY)
    target_sources(lvdExplorer PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/app.rc)
endif()

target_link_libraries(lvdExplorer PRIVATE lvdUi lvdCore Qt6::Widgets)

set_target_properties(lvdExplorer PROPERTIES
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE OFF
)

# Translations: .ts sources live under resources/translations/ (Italian
# and French, both fully translated -- not scaffolding). This compiles
# each .ts to .qm and embeds them into the binary at ":/i18n/", loaded by
# locale in main.cpp. Add more languages by adding more .ts files here and
# to TS_FILES; keep them in sync with source via lupdate.
qt_add_translations(lvdExplorer
    TS_FILES
        ${CMAKE_SOURCE_DIR}/resources/translations/lvdExplorer_it.ts
        ${CMAKE_SOURCE_DIR}/resources/translations/lvdExplorer_fr.ts
    RESOURCE_PREFIX "/i18n"
)
