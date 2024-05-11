# CMakeLists.txt
cmake_minimum_required(VERSION 3.15)
set(CMAKE_TOOLCHAIN_FILE
    "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake"
    CACHE STRING "Vcpkg toolchain file")
project(netspectre-ma)

# Find dependencies provided by vcpkg (via vcpkg.cmake)

# main target
add_executable(service)
target_sources(main PRIVATE src/service.cpp)
target_link_libraries(main PRIVATE unofficial::sqlite3::sqlite3 fmt::fmt-header-only)
set_property(TARGET main PROPERTY CXX_STANDARD 23)

# tests target
add_executable(tests)
target_sources(tests PRIVATE src/test.cpp)
target_link_libraries(tests PRIVATE unofficial::sqlite3::sqlite3 fmt::fmt-header-only)
set_property(TARGET tests PROPERTY CXX_STANDARD 23)
enable_testing()
add_test(tests tests)
