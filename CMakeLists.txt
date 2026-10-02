cmake_minimum_required(VERSION 3.23)

project(lvdExplorer
    VERSION 0.2.0
    DESCRIPTION "Open-source Qt6 multi-pane file manager"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets LinguistTools)

qt_standard_project_setup()

enable_testing()

add_subdirectory(core)
add_subdirectory(platform)
add_subdirectory(ui)
add_subdirectory(app)
add_subdirectory(tests)
