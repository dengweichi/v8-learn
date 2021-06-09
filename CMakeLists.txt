cmake_minimum_required(VERSION 3.5.1)
project(v8_learn)

set(CMAKE_CXX_STANDARD 14)

set(googleTestDir ./googletest)
add_subdirectory(${googleTestDir})
include_directories(${googleTestDir}/googletest/include)
include_directories(${googleTestDir}/googlemock/include)

include_directories(${PROJECT_SOURCE_DIR}/include/v8)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x -pthread -DV8_COMPRESS_POINTERS")
add_executable(v8_learn
        main.cpp
        environment.cpp
        environment.h
        test/isolate_test.cpp
        test/context_test.cpp
        test/handle_test.cpp
        test/handle_scope_test.cpp
        test/primtive_test.cpp test/extension_test.cpp test/module_test.cpp test/node_build_in_module.cpp)
target_link_libraries(v8_learn ${PROJECT_SOURCE_DIR}/deps/v8/libv8_monolith.a ${PROJECT_SOURCE_DIR}/deps/v8/libv8_libplatform.a ${PROJECT_SOURCE_DIR}/deps/v8/libv8_libbase.a)

target_link_libraries(v8_learn gtest gtest_main)
target_link_libraries(v8_learn gmock gmock_main)