project(PokeTimerPlusTest)

find_package(Qt6 COMPONENTS Core Test REQUIRED)
qt_standard_project_setup()

qt_add_executable(PokeTimerPlusTest
    main.cpp
    AudioTest.cpp
    AudioTest.hpp
    StoreTest.cpp
    StoreTest.hpp
    ScheduleTest.cpp
    ScheduleTest.hpp
    TimersTest.cpp
    TimersTest.hpp
    XYTidTest.cpp
    XYTidTest.hpp
)

target_compile_definitions(PokeTimerPlusTest PRIVATE GOLDEN_JSON="${CMAKE_CURRENT_SOURCE_DIR}/Resources/eontimer_golden.json"
    XYTID_JSON="${CMAKE_CURRENT_SOURCE_DIR}/Resources/xytid_6gentidsearch.json")
target_link_libraries(PokeTimerPlusTest PRIVATE PokeTimerPlusCore nlohmann_json::nlohmann_json Qt6::Core Qt6::Test)

add_test(NAME PokeTimerPlusTest COMMAND PokeTimerPlusTest)
