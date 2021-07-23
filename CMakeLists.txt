cmake_minimum_required(VERSION 3.5.3)
project(v8_learn)

set(CMAKE_CXX_STANDARD 14)


# 如果没有获取到指针变量大小，模式记录为64位系统
if(NOT(CMAKE_SIZEOF_VOID_P))
    # 获取指针变量大小失败。默认重设为64为系统
    message(WARNING "Failed to get the size of pointer variable. The default is reset to 64 for the system")
    set(CMAKE_SIZEOF_VOID_P 8)
endif()

# 只支持x86-64位处理器
if( (NOT(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "AMD64")) AND (NOT(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "x86_64")))
    message(FATAL_ERROR "only support x86-64")
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
        test/primitive_test.cpp
        test/extension_test.cpp
        test/module_test.cpp
        test/promise_test.cpp
        test/node_build_in_module.cpp)

#只支持 64位的linux和 64位windows系统
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")

    if (NOT(CMAKE_CXX_COMPILER_ID STREQUAL "GNU"))
        message(FATAL_ERROR "linux system only support GNU or Clang")
    endif ()

    #设置宏LINUX
    add_definitions(-DLINUX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x -pthread -Wl,--no-as-needed -ldl")

    # 64位系统下
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        # 64位环境下使用指针压缩
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -DV8_COMPRESS_POINTERS")
        message(STATUS "build linux 64 ${CMAKE_BUILD_TYPE} mode")
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            target_link_libraries(${PROJECT_NAME}
                    ${PROJECT_SOURCE_DIR}/v8/lib/linux/x64/debug/libv8_monolith.a
                    ${PROJECT_SOURCE_DIR}/v8/lib/linux/x64/debug/libv8_libbase.a
                    ${PROJECT_SOURCE_DIR}/v8/lib/linux/x64/debug/libv8_libplatform.a)
        else()
            target_link_libraries(${PROJECT_NAME}
                    ${PROJECT_SOURCE_DIR}/v8/lib/linux/x64/release/libv8_monolith.a
                    ${PROJECT_SOURCE_DIR}/v8/lib/linux/x64/release/libv8_libbase.a
                    ${PROJECT_SOURCE_DIR}/v8/lib/linux/x64/release/libv8_libplatform.a)
        endif()

    else()
        message(STATUS "build linux 32 ${CMAKE_BUILD_TYPE} mode")
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            target_link_libraries(${PROJECT_NAME}
                    ${PROJECT_SOURCE_DIR}/v8/lib/linux/ia32/debug/libv8_monolith.a
                    ${PROJECT_SOURCE_DIR}/v8/lib/linux/ia32/debug/libv8_libbase.a
                    ${PROJECT_SOURCE_DIR}/v8/lib/linux/ia32/debug/libv8_libplatform.a)
        else()
            target_link_libraries(${PROJECT_NAME}
                    ${PROJECT_SOURCE_DIR}/v8/lib/linux/ia32/release/libv8_monolith.a
                    ${PROJECT_SOURCE_DIR}/v8/lib/linux/ia32/release/libv8_libbase.a
                    ${PROJECT_SOURCE_DIR}/v8/lib/linux/ia32/release/libv8_libplatform.a)
        endif()
    endif()

elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")

    if (NOT(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC"))
        message(SEND_ERROR "windows system only support MSVC")
    endif ()

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        foreach(var
                CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
                CMAKE_C_FLAGS_MINSIZEREL CMAKE_C_FLAGS_RELWITHDEBINFO
                CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
                CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO
                )
            if(${var} MATCHES "/MDd")
                # 正则表达式替换/MD为/MT
                string(REGEX REPLACE "/MDd" "/MTd" ${var} "${${var}}")
            endif()
        endforeach()
    else()
        # 将所有默认的C,CXX编译选项中的/MD替换成/MT.
        foreach(var
                CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
                CMAKE_C_FLAGS_MINSIZEREL CMAKE_C_FLAGS_RELWITHDEBINFO
                CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
                CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO
                )
            if(${var} MATCHES "/MD")
                # 正则表达式替换/MD为/MT
                string(REGEX REPLACE "/MD" "/MT" ${var} "${${var}}")
            endif()
        endforeach()
    endif()


    # 设置宏 WIN
    add_definitions(-DWIN)
    # windows 链接库
    set(WINDOW_LINK_LIB winmm.lib dbghelp.lib)

    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        message(STATUS "build windows 64 ${CMAKE_BUILD_TYPE} mode")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DV8_COMPRESS_POINTERS")
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            target_link_libraries(${PROJECT_NAME}
                    ${WINDOW_LINK_LIB}
                    ${PROJECT_SOURCE_DIR}/v8/lib/win/x64/debug/v8_monolith.lib
                    ${PROJECT_SOURCE_DIR}/v8/lib/win/x64/debug/v8_libbase.lib
                    ${PROJECT_SOURCE_DIR}/v8/lib/win/x64/debug/v8_libplatform.lib)
        else()
            target_link_libraries(${PROJECT_NAME}
                    ${WINDOW_LINK_LIB}
                    ${PROJECT_SOURCE_DIR}/v8/lib/win/x64/release/v8_monolith.lib
                    ${PROJECT_SOURCE_DIR}/v8/lib/win/x64/release/v8_libbase.lib
                    ${PROJECT_SOURCE_DIR}/v8/lib/win/x64/release/v8_libplatform.lib)
        endif()
    else()
        message(STATUS "build windows 32 ${CMAKE_BUILD_TYPE} mode")

        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            target_link_libraries(${PROJECT_NAME}
                    ${WINDOW_LINK_LIB}
                    ${PROJECT_SOURCE_DIR}/v8/lib/win/ia32/debug/v8_monolith.lib
                    ${PROJECT_SOURCE_DIR}/v8/lib/win/ia32/debug/v8_libbase.lib
                    ${PROJECT_SOURCE_DIR}/v8/lib/win/ia32/debug/v8_libplatform.lib)
        else()
            target_link_libraries(${PROJECT_NAME}
                    ${WINDOW_LINK_LIB}
                    ${PROJECT_SOURCE_DIR}/v8/lib/win/ia32/release/v8_monolith.lib
                    ${PROJECT_SOURCE_DIR}/v8/lib/win/ia32/release/v8_libbase.lib
                    ${PROJECT_SOURCE_DIR}/v8/lib/win/ia32/release/v8_libplatform.lib)
        endif()
    endif()
else()
    message(FATAL_ERROR "only support x86 linux and windows system")
endif()



target_link_libraries(${PROJECT_NAME} gtest gtest_main)
target_link_libraries(${PROJECT_NAME} gmock gmock_main)

file(COPY ./test/source DESTINATION ${CMAKE_BINARY_DIR})