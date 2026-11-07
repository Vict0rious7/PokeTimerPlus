cmake_minimum_required(VERSION 3.21)
project(BeepLab VERSION 0.1.0 LANGUAGES C CXX)
if(APPLE)
    enable_language(OBJCXX)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(TEST "Build the analysis tests" ON)

# Pure C++ analysis core (no Qt): unit-tested on synthetic recordings.
add_library(beeplab_core STATIC src/Wav.cpp src/Analysis.cpp src/BeepMixer.cpp)
target_include_directories(beeplab_core PUBLIC src)

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Multimedia)
qt_standard_project_setup()

qt_add_executable(BeepLab MACOSX_BUNDLE
    src/main.cpp
    src/MainWindow.cpp src/MainWindow.h
    src/Engines.h
    src/SoundEffectEngine.cpp
    src/QtStreamEngine.cpp
    src/MiniaudioEngine.cpp
    src/miniaudio_impl.c
    src/Recorder.cpp src/Recorder.h
    src/Platform.h
)
if(APPLE)
    target_sources(BeepLab PRIVATE src/Platform_mac.mm)
    target_link_libraries(BeepLab PRIVATE "-framework Foundation" "-framework IOKit" "-framework CoreFoundation"
                                          "-framework CoreAudio" "-framework AudioToolbox")
    set_target_properties(BeepLab PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/cmake/Info.plist.in
        MACOSX_BUNDLE_GUI_IDENTIFIER io.github.vict0rious7.poketimerplus.beeplab
        MACOSX_BUNDLE_BUNDLE_NAME BeepLab
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION})
else()
    target_sources(BeepLab PRIVATE src/Platform_stub.cpp)
    find_package(Threads REQUIRED)
    target_link_libraries(BeepLab PRIVATE Threads::Threads ${CMAKE_DL_LIBS} m)
endif()

target_include_directories(BeepLab PRIVATE third_party/miniaudio)
target_compile_definitions(BeepLab PRIVATE BEEPLAB_RESULTS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/results")
target_link_libraries(BeepLab PRIVATE beeplab_core Qt6::Widgets Qt6::Multimedia)

qt_add_resources(BeepLab sounds
    PREFIX /
    BASE resources
    FILES resources/sounds/beep.wav resources/sounds/ding.wav resources/sounds/pop.wav resources/sounds/tick.wav
)

if(TEST)
    enable_testing()
    add_executable(analysis_test tests/analysis_test.cpp)
    target_link_libraries(analysis_test PRIVATE beeplab_core)
    target_compile_definitions(analysis_test PRIVATE BEEPLAB_SOUNDS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/resources/sounds")
    add_test(NAME analysis_test COMMAND analysis_test)

    add_executable(reanalyze tests/reanalyze.cpp)
    target_link_libraries(reanalyze PRIVATE beeplab_core)
    target_compile_definitions(reanalyze PRIVATE BEEPLAB_SOUNDS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/resources/sounds")
endif()
