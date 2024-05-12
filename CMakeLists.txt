# CMakeLists.txt
cmake_minimum_required(VERSION 3.15)
set(CMAKE_TOOLCHAIN_FILE
    "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake"
    CACHE STRING "Vcpkg toolchain file")
project(netspectre-ma)

# Find dependencies provided by vcpkg (via vcpkg.cmake)
find_package(Boost REQUIRED COMPONENTS asio)

# main target
add_executable(service)
target_sources(service PRIVATE src/service.cpp)
target_link_libraries(service PRIVATE Boost::asio)
set_property(TARGET service PROPERTY CXX_STANDARD 23)

# tests target
add_executable(attacker)
target_sources(attacker PRIVATE src/attacker.cpp)
target_link_libraries(attacker PRIVATE Boost::asio)
set_property(TARGET attacker PROPERTY CXX_STANDARD 23)

