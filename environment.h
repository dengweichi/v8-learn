//
// Created by user on 2021/5/17.
//

#ifndef V8_EXTENSION_ENVIRONMENT_H
#define V8_EXTENSION_ENVIRONMENT_H
#include "v8.h"
#include "gtest/gtest.h"

class Environment: public testing::Test {
private:
    v8::Isolate* _isolate;
    v8::ArrayBuffer::Allocator* _array_buffer_allocator;
protected:
    void SetUp() override;
    void TearDown() override;

public:
    inline v8::Isolate* getIsolate ();
};


#endif //V8_EXTENSION_ENVIRONMENT_H
