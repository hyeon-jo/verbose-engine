cmake_minimum_required(VERSION 3.10)
project(sensor_window_tests)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5 COMPONENTS Core Widgets Test REQUIRED)

# 테스트 실행 파일 생성
add_executable(sensor_window_tests
    main.cpp
    test_sensor_window.cpp
    test_sensor_window.hpp
    ${CMAKE_SOURCE_DIR}/sensor_window.cpp
    ${CMAKE_SOURCE_DIR}/sensor_window.hpp
)

target_include_directories(sensor_window_tests PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(sensor_window_tests
    PRIVATE
    Qt5::Core
    Qt5::Widgets
    Qt5::Test
)

# 테스트 실행 설정
enable_testing()
add_test(NAME sensor_window_tests COMMAND sensor_window_tests) 