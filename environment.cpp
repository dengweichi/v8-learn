//
// Created by user on 2021/5/17.
//

#include "environment.h"

void Environment::SetUp() {
    v8::Isolate::CreateParams create_params;
    _array_buffer_allocator = v8::ArrayBuffer::Allocator::NewDefaultAllocator();
    create_params.array_buffer_allocator = _array_buffer_allocator;
    _isolate = v8::Isolate::New(create_params);
    _isolate->Enter();
}

void Environment::TearDown() {
    _isolate->Dispose();
    delete _array_buffer_allocator;
}

v8::Isolate *Environment::getIsolate() {
    return _isolate;
}
