cmake_minimum_required(VERSION 3.5.1)
project(v8_extension)

set(CMAKE_CXX_STANDARD 14)

set(googleTestDir ./googletest)
add_subdirectory(${googleTestDir})
include_directories(${googleTestDir}/googletest/include)
include_directories(${googleTestDir}/googlemock/include)

include_directories(${PROJECT_SOURCE_DIR}/include/v8)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x -pthread -DV8_COMPRESS_POINTERS")
add_executable(v8_extension main.cpp environment.cpp environment.h test/isolate_test.cpp)
target_link_libraries(v8_extension ${PROJECT_SOURCE_DIR}/deps/v8/libv8_monolith.a ${PROJECT_SOURCE_DIR}/deps/v8/libv8_libplatform.a ${PROJECT_SOURCE_DIR}/deps/v8/libv8_libbase.a)

target_link_libraries(v8_extension gtest gtest_main)
target_link_libraries(v8_extension gmock gmock_main)