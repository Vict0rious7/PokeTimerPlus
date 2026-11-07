project(PokeTimerPlusCore)

add_subdirectory(External)

add_library(PokeTimerPlusCore STATIC
    Audio/AudioEngine.cpp
    Audio/AudioEngine.hpp
    Audio/BeepMixer.cpp
    Audio/BeepMixer.hpp
    Audio/Wav.cpp
    Audio/Wav.hpp
    Audio/miniaudio_impl.c
    Profile/CalibrationStore.cpp
    Profile/CalibrationStore.hpp
    Timer/Calibrator.cpp
    Timer/Calibrator.hpp
    Timer/FrameRate.cpp
    Timer/FrameRate.hpp
    Timer/RngTypes.cpp
    Timer/RngTypes.hpp
    Timer/Schedule.cpp
    Timer/Schedule.hpp
    Timer/Timers.cpp
    Timer/Timers.hpp
    Util/Clock.hpp
)

target_include_directories(PokeTimerPlusCore PRIVATE External/miniaudio)
if (APPLE)
    target_link_libraries(PokeTimerPlusCore PRIVATE "-framework CoreFoundation" "-framework CoreAudio" "-framework AudioToolbox")
elseif (UNIX)
    find_package(Threads REQUIRED)
    target_link_libraries(PokeTimerPlusCore PRIVATE Threads::Threads ${CMAKE_DL_LIBS} m)
endif ()

target_link_libraries(PokeTimerPlusCore PUBLIC PokeTimerPlusIncludes PRIVATE nlohmann_json::nlohmann_json)
