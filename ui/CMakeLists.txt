add_library(lvdUi STATIC
    resources/aboutassets.qrc
    bookmarks/bookmarkmanager.h
    bookmarks/bookmarkmanager.cpp
    bookmarks/bookmarkspanel.h
    bookmarks/bookmarkspanel.cpp
    clipboard/fileclipboard.h
    clipboard/fileclipboard.cpp
    dialogs/aboutdialog.h
    dialogs/aboutdialog.cpp
    dialogs/sessionmanagerdialog.h
    dialogs/sessionmanagerdialog.cpp
    dialogs/progressdialog.h
    dialogs/progressdialog.cpp
    dialogs/batchrenamedialog.h
    dialogs/batchrenamedialog.cpp
    dialogs/checksumdialog.h
    dialogs/checksumdialog.cpp
    dialogs/foldercomparedialog.h
    dialogs/foldercomparedialog.cpp
    dialogs/shortcuteditordialog.h
    dialogs/shortcuteditordialog.cpp
    finder/contentresultsmodel.h
    finder/contentresultsmodel.cpp
    finder/finderpanel.h
    finder/finderpanel.cpp
    finder/searchresultsmodel.h
    finder/searchresultsmodel.cpp
    pane/filepane.h
    pane/filepane.cpp
    pane/panecontainer.h
    pane/panecontainer.cpp
    pane/panetabbar.h
    pane/panetabbar.cpp
    pane/panegridwidget.h
    pane/panegridwidget.cpp
    resources/iconfactory.h
    resources/iconfactory.cpp
    shortcuts/shortcutmanager.h
    shortcuts/shortcutmanager.cpp
    theme/thememanager.h
    theme/thememanager.cpp
    view/filetreeview.h
    view/filetreeview.cpp
    widgets/breadcrumbbar.h
    widgets/breadcrumbbar.cpp
)

target_include_directories(lvdUi PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_link_libraries(lvdUi PUBLIC Qt6::Widgets lvdCore lvdPlatform)
