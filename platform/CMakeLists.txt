if(WIN32)
    set(PLATFORM_SOURCES
        win/windowsplatformshell.h
        win/windowsplatformshell.cpp
    )
elseif(UNIX)
    find_package(Qt6 REQUIRED COMPONENTS DBus)
    set(PLATFORM_SOURCES
        linux/linuxplatformshell.h
        linux/linuxplatformshell.cpp
    )
endif()

add_library(lvdPlatform STATIC
    iplatformshell.h
    platformshell.h
    platformshell.cpp
    ${PLATFORM_SOURCES}
)

target_include_directories(lvdPlatform PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(lvdPlatform PUBLIC Qt6::Widgets)

if(WIN32)
    # Shell32/Ole32 for the IShellFolder/IContextMenu/ShellExecuteEx COM
    # and PIDL machinery in windowsplatformshell.cpp.
    target_link_libraries(lvdPlatform PRIVATE Shell32 Ole32)
elseif(UNIX)
    target_link_libraries(lvdPlatform PRIVATE Qt6::DBus)
endif()
