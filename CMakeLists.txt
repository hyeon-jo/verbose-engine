cmake_minimum_required(VERSION 3.16)

project(control_app VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Find required packages
find_package(Qt5 COMPONENTS Core Widgets REQUIRED)
find_package(Boost REQUIRED COMPONENTS system serialization)
find_package(Threads REQUIRED)
find_package(OpenCV REQUIRED)

add_executable(control_app
    main.cpp
    control_app.cpp
    control_app.hpp
    image_viewer.cpp
    image_viewer.hpp
    tcp_client.cpp
    tcp_client.hpp
    messages.hpp
)

target_link_libraries(control_app PRIVATE
    Qt5::Core
    Qt5::Widgets
    Boost::system
    Boost::serialization
    Threads::Threads
    pthread
    ${OpenCV_LIBS}
) 