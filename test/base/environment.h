//
// Created by user on 2021/5/17.
//

#ifndef V8_EXTENSION_ENVIRONMENT_H
#define V8_EXTENSION_ENVIRONMENT_H
#include "v8.h"
#include "gtest/gtest.h"
#include <algorithm>
#include <string>
#include<fstream>
#include <sstream>
#include <iterator>

#if defined(WIN)
#include <windows.h>
#elif defined(LINUX)
#include <unistd.h>
#else
#endif

extern v8::Platform *g_default_platform;


class Environment : public ::testing::Test {
private:
    v8::Isolate *_isolate;
    v8::ArrayBuffer::Allocator *_array_buffer_allocator;

protected:
    void SetUp() override;
    void TearDown() override;

public:
    v8::Isolate *getIsolate() {
        return _isolate;
    }
    /**
     * 获取路径上得目录名称
     * @param path
     * @return
     */
    static std::string DirName(const std::string &path);
    /**
     * 平常化路径
     * @param path
     * @param dir_name
     * @return
     */
    static std::string NormalizePath(const std::string &path,
                                     const std::string &dir_name);
    /**
     * 判断是否为绝对路径
     * @param path
     * @return
     */
    static bool IsAbsolutePath(const std::string &path);
    /**
     *  获取工作目录
     * @return
     */
    static std::string GetWorkingDirectory();
    /**
     * 读取文件
     * @param path
     * @return
     */
    static std::string&& ReadFile (std::string& path);
};

#endif//V8_EXTENSION_ENVIRONMENT_H
