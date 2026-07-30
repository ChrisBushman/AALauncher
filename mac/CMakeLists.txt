cmake_minimum_required(VERSION 3.20)
project(AALauncherMac VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt6 must be found via CMAKE_PREFIX_PATH, e.g.:
#   cmake -S . -B build -DCMAKE_PREFIX_PATH=$(brew --prefix qt)
find_package(Qt6 REQUIRED COMPONENTS Widgets WebEngineWidgets Network)

qt_standard_project_setup()

qt_add_executable(AALauncher
    main.cpp
    MainWindow.h
    MainWindow.cpp
    DisplaySettingsDialog.h
    DisplaySettingsDialog.cpp
    NetworkIPDialog.h
    NetworkIPDialog.cpp
    ScriptCompilerWindow.h
    ScriptCompilerWindow.cpp
    ServerList.h
    ServerList.cpp
    ServerDiscovery.h
    ServerDiscovery.cpp
)

target_link_libraries(AALauncher
    PRIVATE
        Qt6::Widgets
        Qt6::WebEngineWidgets
        Qt6::Network
)

# Keep a flat binary (not a .app bundle) for easier development and deployment
# alongside the game binaries (AA, AAServer).
set_target_properties(AALauncher PROPERTIES
    MACOSX_BUNDLE FALSE
)
