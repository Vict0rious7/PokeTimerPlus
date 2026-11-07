cmake_minimum_required(VERSION 3.25)

project(PokeTimerPlus VERSION 0.1.0 LANGUAGES C CXX)

option(TEST "Build tests" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Same layout as PokeFinder: Core (no Qt), Form (Qt Widgets), Test, main.cpp.
add_library(PokeTimerPlusIncludes INTERFACE)
target_include_directories(PokeTimerPlusIncludes INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets)
qt_standard_project_setup()

add_subdirectory(Core)
if (TEST)
    enable_testing()
    add_subdirectory(Test)
endif ()
add_subdirectory(Form)

qt_add_executable(PokeTimerPlus WIN32 MACOSX_BUNDLE main.cpp)

# App icon (same pipeline as PokeFinderPlus): .ico in the Windows .exe, .icns in the macOS bundle.
# The window icon comes from the PNGs in Form/Images/Icon (Qt resources, see Form/CMakeLists.txt).
target_sources(PokeTimerPlus PRIVATE
    "$<$<PLATFORM_ID:Windows>:${CMAKE_CURRENT_SOURCE_DIR}/appicon.rc>"
    "$<$<PLATFORM_ID:Darwin>:${CMAKE_CURRENT_SOURCE_DIR}/poketimerplus.icns>"
)
set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/poketimerplus.icns PROPERTIES
    MACOSX_PACKAGE_LOCATION "Resources"
)

set_target_properties(PokeTimerPlus PROPERTIES
    MACOSX_BUNDLE_ICON_FILE poketimerplus.icns
    MACOSX_BUNDLE_BUNDLE_NAME PokeTimerPlus
    MACOSX_BUNDLE_GUI_IDENTIFIER io.github.vict0rious7.poketimerplus
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
)
target_link_libraries(PokeTimerPlus PRIVATE PokeTimerPlusForm Qt6::Widgets)

# Phase0-BeepLab is a separate CMake project (measurement tool), not part of this build.
