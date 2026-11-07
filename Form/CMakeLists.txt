project(PokeTimerPlusForm)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

configure_file(AppVersion.hpp.in ${CMAKE_CURRENT_BINARY_DIR}/AppVersion.hpp)

add_library(PokeTimerPlusForm STATIC
    MainWindow.cpp
    MainWindow.hpp
    SettingsDialog.cpp
    SettingsDialog.hpp
    TimerController.cpp
    TimerController.hpp
    TimerDisplay.cpp
    TimerDisplay.hpp
    TimerPanels.cpp
    TimerPanels.hpp
)

qt_add_resources(PokeTimerPlusForm sounds
    PREFIX /sounds
    BASE Sounds
    FILES Sounds/beep.wav Sounds/ding.wav Sounds/pop.wav Sounds/tick.wav
)

qt_add_resources(PokeTimerPlusForm icons
    PREFIX /icons
    BASE Images/Icon
    FILES
        Images/Icon/poketimerplus_16x16.png
        Images/Icon/poketimerplus_24x24.png
        Images/Icon/poketimerplus_32x32.png
        Images/Icon/poketimerplus_48x48.png
        Images/Icon/poketimerplus_256x256.png
)

target_include_directories(PokeTimerPlusForm PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(PokeTimerPlusForm PUBLIC PokeTimerPlusCore Qt6::Widgets)
