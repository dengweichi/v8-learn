cmake_minimum_required(VERSION 3.5.1)
project(v8_learn)

set(CMAKE_CXX_STANDARD 14)

#只支持64位系统
if(NOT(CMAKE_SIZEOF_VOID_P EQUAL 8))
    message(SEND_ERROR "only support 64 bits")
endif()

# 只支持x86-64位 cpu
if(NOT(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "x86_64"))
    message(SEND_ERROR "only support x86-64 cpu")
endif()

add_subdirectory(./googletest)
include_directories(./googletest/googletest/include)
include_directories(./googletest/googlemock/include)

include_directories(${PROJECT_SOURCE_DIR}/v8/include)


add_executable(${PROJECT_NAME}
        main.cpp
        environment.cpp
        environment.h
        test/isolate_test.cpp
        test/context_test.cpp
        test/handle_test.cpp
        test/handle_scope_test.cpp
        test/promise_test.cpp
        test/primtive_test.cpp
        test/extension_test.cpp
        test/module_test.cpp
        test/node_build_in_module.cpp)

#只支持 64位的linux和 64位windows系统
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    #设置宏LINUX
    add_definitions(-DLINUX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x -pthread -DV8_COMPRESS_POINTERS")
    target_link_libraries(${PROJECT_NAME} ${PROJECT_SOURCE_DIR}/v8/lib/linux/libv8_monolith.a
            ${PROJECT_SOURCE_DIR}/v8/lib/linux/libv8_libplatform.a
            ${PROJECT_SOURCE_DIR}/v8/lib/linux/libv8_libbase.a)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    # 设置宏 WIN
    add_definitions(-DWIN)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x -DV8_COMPRESS_POINTERS")
else()
    message(SEND_ERROR "only support 64 bits linux and windows system")
endif()



target_link_libraries(v8_learn gtest gtest_main)
target_link_libraries(v8_learn gmock gmock_main)