
#include "../environment.h"
#include <iostream>
#include <thread>

void threadExe(v8::Isolate *isolate, v8::Persistent<v8::Context, v8::CopyablePersistentTraits<v8::Context>> context) {
  isolate->Enter();
  v8::Local<v8::Context> localContext = v8::Local<v8::Context>::New(isolate, context);
  v8::Context::Scope context_scope(localContext);
  const char* scriptSource = "fun()";
  v8::Local<v8::String> source = v8::String::NewFromUtf8(isolate, scriptSource).ToLocalChecked();
  v8::MaybeLocal<v8::Script> script = v8::Script::Compile(localContext, source);
  if (script.IsEmpty()) {
    std::cout << "isempty" << std::endl;
  }
  v8::Local<v8::Value> result = script.ToLocalChecked()->Run(localContext).ToLocalChecked();
  EXPECT_EQ(result.As<v8::Number>()->Value(), 1);
  isolate->Exit();
}

TEST(isolate_test, Multithreading) {
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = v8::ArrayBuffer::Allocator::NewDefaultAllocator();
  v8::Isolate *isolate = v8::Isolate::New(create_params);
  isolate->Enter();
  v8::HandleScope handleScope(isolate);
  v8::Local<v8::Context> context = v8::Context::New(isolate);
  v8::Context::Scope context_scope(context);
  const char* scriptSource = "function fun() {"
                             "  return 1;"
                             "}"
                             "fun();";
  v8::Local<v8::String> source = v8::String::NewFromUtf8(isolate, scriptSource).ToLocalChecked();
  v8::Local<v8::Script> script = v8::Script::Compile(context, source).ToLocalChecked();
  v8::Local<v8::Value> result = script->Run(context).ToLocalChecked();
  EXPECT_TRUE(context->Global()->Get(context, v8::String::NewFromUtf8Literal(isolate, "fun")).ToLocalChecked()->IsFunction());
  EXPECT_EQ(result.As<v8::Number>()->Value(), 1);

  v8::Persistent<v8::Context, v8::CopyablePersistentTraits<v8::Context>> globalContext(isolate, context);
  std::thread thread(threadExe, isolate, globalContext);
  thread.join();
  isolate->Exit();
}

bool IsAligned(unsigned long long value, unsigned long long alignment) {
  return (value & (alignment - 1)) == 0;
}
/**
 * 测试隔离实例地址空间4GB 对齐
 */
TEST(isolate_test, isolate_4g_address_aligned) {
  unsigned long long GB = 1024 * 1024 * 1024;
  v8::Isolate::CreateParams create_params1;
  create_params1.array_buffer_allocator =
      v8::ArrayBuffer::Allocator::NewDefaultAllocator();
  v8::Isolate::CreateParams create_params2;
  create_params2.array_buffer_allocator =
      v8::ArrayBuffer::Allocator::NewDefaultAllocator();
  v8::Isolate *isolate1 = v8::Isolate::New(create_params1);
  v8::Isolate *isolate2 = v8::Isolate::New(create_params2);
  EXPECT_TRUE(IsAligned(reinterpret_cast<unsigned long long>(isolate1),
                        size_t{4} * GB));
  EXPECT_TRUE(IsAligned(reinterpret_cast<unsigned long long>(isolate2),
                        size_t{4} * GB));
}

double globalNum = 0;
TEST(isolate_test, isolate_jsObject_communication) {
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator =
      v8::ArrayBuffer::Allocator::NewDefaultAllocator();
  v8::Isolate *isolate1 = v8::Isolate::New(create_params);

  isolate1->Enter();
  v8::HandleScope scope1(isolate1);
  v8::Local<v8::Context> context1 = v8::Context::New(isolate1);
  v8::Context::Scope context_scope1(context1);
  // 创建函数
  v8::Local<v8::Function> function1 =
      v8::Function::New(
          context1,
          [](const v8::FunctionCallbackInfo<v8::Value> &info) -> void {
            globalNum = info[0].As<v8::Number>()->Value();
            info.GetReturnValue().Set(globalNum);
          })
          .ToLocalChecked();
  // 把函数set到全局变量中
  context1->Global()->Set(
      context1, v8::String::NewFromUtf8Literal(isolate1, "globalSetFun"),
      function1);
  // 编译执行代码
  const char *scriptSource1 = "globalSetFun(2)";
  v8::Local<v8::String> source1 =
      v8::String::NewFromUtf8(isolate1, scriptSource1).ToLocalChecked();
  v8::Local<v8::Script> script1 =
      v8::Script::Compile(context1, source1).ToLocalChecked();
  v8::Local<v8::Value> result1 = script1->Run(context1).ToLocalChecked();
  EXPECT_EQ(result1.As<v8::Number>()->Value(), 2);
  EXPECT_EQ(globalNum, 2);
  isolate1->Exit();

  v8::Isolate *isolate2 = v8::Isolate::New(create_params);
  isolate2->Enter();
  v8::HandleScope scope2(isolate2);
  v8::Local<v8::Context> context2 = v8::Context::New(isolate2);
  v8::Context::Scope context_scope2(context2);
  v8::Local<v8::Function> function2 =
      v8::Function::New(
          context2,
          [](const v8::FunctionCallbackInfo<v8::Value> &info) -> void {
            info.GetReturnValue().Set(globalNum);
          })
          .ToLocalChecked();
  context2->Global()->Set(
      context2, v8::String::NewFromUtf8Literal(isolate2, "globalGetFun"),
      function2);
  const char *scriptSource2 = "globalGetFun()";
  v8::Local<v8::String> source2 =
      v8::String::NewFromUtf8(isolate2, scriptSource2).ToLocalChecked();
  v8::Local<v8::Script> script2 =
      v8::Script::Compile(context2, source2).ToLocalChecked();
  v8::Local<v8::Value> result2 = script2->Run(context2).ToLocalChecked();
  EXPECT_EQ(result2.As<v8::Number>()->Value(), 2);
  isolate2->Exit();
}
