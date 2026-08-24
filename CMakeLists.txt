cmake_minimum_required(VERSION 3.16)
project(RedR VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable Qt automated tools
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Optimization flags for high rendering performance
if(MSVC)
    add_compile_options(/O2 /MP)
else()
    add_compile_options(-O3 -pthread -Wall -Wextra -Wno-unused-parameter)
endif()

# Find Qt6 Packages
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Network
    Concurrent
    OpenGL
    OpenGLWidgets
)

# Find zlib (used by PBF loader)
find_package(ZLIB REQUIRED)

# Source files
set(SOURCES
    src/main.cpp
    src/core/OsmPbfLoader.cpp
    src/core/MapDataCache.cpp
    src/core/DamManager.cpp
    src/core/DamFloodSimulation.cpp
    src/renderer/MapStyle.cpp
    src/renderer/MapRenderer.cpp
    src/ui/NavigationControls.cpp
    src/ui/SearchBar.cpp
    src/ui/PlaceCard.cpp
    src/ui/LayerPanel.cpp
    src/ui/MapWidget.cpp
    src/ui/MainWindow.cpp
    src/ui/OnlineTileWidget.cpp
    src/ui/MiniMap.cpp
    src/ui/SettingsDialog.cpp
    src/ui/TimelineWidget.cpp
    src/ui/PropertiesPanel.cpp
    src/ui/SidebarTerminal.cpp
    src/ui/PieMenu.cpp
    src/assets/resources.qrc
)

# Application Executable Target
add_executable(RedR ${SOURCES})

target_include_directories(RedR PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/third_party/include
)

target_link_libraries(RedR PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::Concurrent
    Qt6::OpenGL
    Qt6::OpenGLWidgets
    ZLIB::ZLIB
)

set_target_properties(RedR PROPERTIES
    OUTPUT_NAME "RedR"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

# Compatibility alias for build scripts
add_custom_target(assam_map ALL DEPENDS RedR
    COMMAND ${CMAKE_COMMAND} -E create_symlink $<TARGET_FILE:RedR> ${CMAKE_BINARY_DIR}/assam_map
)
