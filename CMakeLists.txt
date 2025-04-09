cmake_minimum_required(VERSION 3.10)

project(control_app)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Find required packages
find_package(Qt5 COMPONENTS Core Widgets Test REQUIRED)
find_package(Boost COMPONENTS system serialization REQUIRED)
find_package(Threads REQUIRED)

add_executable(control_app
    main.cpp
    control_app.cpp
    control_app.hpp
    messages.hpp
    tcp_client.cpp
    tcp_client.hpp
    backend_types.hpp
    sensor_window.cpp
    sensor_window.hpp
)

target_link_libraries(control_app PRIVATE
    Qt5::Core
    Qt5::Widgets
    Boost::system
    Boost::serialization
    Threads::Threads
    pthread
)

# Unit tests
enable_testing()

# 테스트 디렉토리 추가
add_subdirectory(tests) 