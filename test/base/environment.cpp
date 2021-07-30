//
// Created by user on 2021/5/17.
//

#include "environment.h"
/**
 *  获取工作目录
 * @return
 */
std::string Environment::GetWorkingDirectory() {
#if defined(WIN)
    char system_buffer[256];
    DWORD len = GetCurrentDirectoryA(256, system_buffer);
    return system_buffer;
#elif defined(LINUX)
    char curdir[256];
    getcwd(curdir, 256);
    return curdir;
#else
    return "";
#endif
}

/**
 * 判断是否为绝对路径
 * @param path
 * @return
 */
bool  Environment::IsAbsolutePath(const std::string& path) {
#if defined(WIN)
    // This is an incorrect approximation, but should
    // work for all our test-running cases.
    return path.find(':') != std::string::npos;
#else
    return path[0] == '/';
#endif
}

/**
 * 获取路径上得目录名称
 * @param path
 * @return
 */
std::string  Environment::DirName(const std::string& path) {
    if(!IsAbsolutePath(path)){
        return  "";
    }
    unsigned long  long last_slash = path.find_last_of('/');
    last_slash != std::string::npos;
    return path.substr(0, last_slash);
}

/**
 * 平常花路径
 * @param path
 * @param dir_name
 * @return
 */
std::string Environment::NormalizePath(const std::string& path,
                          const std::string& dir_name) {
    std::string absolute_path;
    if (IsAbsolutePath(path)) {
        absolute_path = path;
    } else {
        absolute_path = dir_name + '/' + path;
    }
    std::replace(absolute_path.begin(), absolute_path.end(), '\\', '/');
    std::vector<std::string> segments;
    std::istringstream segment_stream(absolute_path);
    std::string segment;
    while (std::getline(segment_stream, segment, '/')) {
        if (segment == "..") {
            segments.pop_back();
        } else if (segment != ".") {
            segments.push_back(segment);
        }
    }
    // Join path segments.
    std::ostringstream os;
    std::copy(segments.begin(), segments.end() - 1,
              std::ostream_iterator<std::string>(os, "/"));
    os << *segments.rbegin();
    return os.str();
}

std::string&& Environment::ReadFile (std::string& path){
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    // 读取主模块文件
    std::ifstream in(path.c_str());
    // 如果打开文件失败
    if (!in.is_open()) {
        return std::move(std::string(""));
    }
    std::string source;
    char buffer[256];
    // 如果没有读取到文件结束符位置。
    while(!in.eof()){
        in.getline(buffer,256);
        source.append(buffer);
    };
    return std::move(source);
}



void Environment::SetUp() {
  v8::Isolate::CreateParams create_params;
  _array_buffer_allocator = v8::ArrayBuffer::Allocator::NewDefaultAllocator();
  create_params.array_buffer_allocator = _array_buffer_allocator;
  _isolate = v8::Isolate::New(create_params);
  _isolate->Enter();
}

void Environment::TearDown() {
  _isolate->Exit();
  _isolate->Dispose();
  delete _array_buffer_allocator;
}