cmake_minimum_required(VERSION 3.20)

project(AlarmClockRestoMod LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(alarm_clock_core
    src/alarm_clock.cpp
)

target_include_directories(alarm_clock_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_executable(alarm_clock
    src/main.cpp
)

target_link_libraries(alarm_clock PRIVATE alarm_clock_core)

enable_testing()

add_executable(alarm_clock_tests
    tests/alarm_clock_tests.cpp
)

target_link_libraries(alarm_clock_tests PRIVATE alarm_clock_core)

add_test(NAME alarm_clock_tests COMMAND alarm_clock_tests)
