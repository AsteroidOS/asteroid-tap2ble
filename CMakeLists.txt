cmake_minimum_required(VERSION 3.16)

project(asteroid-tap2ble)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_AUTOMOC ON)

find_package(Qt5 COMPONENTS Core DBus REQUIRED)

include_directories(src)

set(MAIN_SOURCE_FILES
    src/main.cpp
	src/ble-dbus.cpp
    src/tap.cpp
	src/ble.cpp)

add_executable(asteroid-tap2ble ${MAIN_SOURCE_FILES})
target_link_libraries(asteroid-tap2ble resolv Qt5::Core Qt5::DBus)

install(TARGETS asteroid-tap2ble DESTINATION bin)
