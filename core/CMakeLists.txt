add_library(lvdCore STATIC
    fsmodel/directoryscantask.h
    fsmodel/directoryscantask.cpp
    fsmodel/drivelisttask.h
    fsmodel/drivelisttask.cpp
    fsmodel/filesystemmodel.h
    fsmodel/filesystemmodel.cpp
    ops/fileoprequest.h
    ops/fileophandle.h
    ops/fileoptask.h
    ops/fileoptask.cpp
    ops/opsengine.h
    ops/opsengine.cpp
    ops/checksumtask.h
    ops/checksumtask.cpp
    ops/foldercompare.h
    ops/foldercompare.cpp
    session/sessiondata.h
    session/sessionmanager.h
    session/sessionmanager.cpp
    search/searchoptions.h
    search/searchtask.h
    search/searchtask.cpp
    search/searchengine.h
    search/searchengine.cpp
    search/contentmatch.h
    search/contentsearchoptions.h
    search/contentsearchtask.h
    search/contentsearchtask.cpp
    search/contentsearchengine.h
    search/contentsearchengine.cpp
    archive/archivereader.h
    archive/archivereader.cpp
    archive/archiveextracttask.h
    archive/archiveextracttask.cpp
    fsmodel/archivescantask.h
    fsmodel/archivescantask.cpp
)

target_include_directories(lvdCore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

# QFileIconProvider (used by FileSystemModel for the Name column's icon)
# lives in Qt Widgets, not Core.
target_link_libraries(lvdCore PUBLIC Qt6::Core Qt6::Widgets)

# libarchive (BSD-2-Clause) reads ZIP, 7z and RAR. Windows: point
# CMAKE_PREFIX_PATH at a vcpkg install (vcpkg install libarchive). Linux:
# libarchive-dev.
find_package(LibArchive 3.4 REQUIRED)
target_link_libraries(lvdCore PRIVATE LibArchive::LibArchive)
