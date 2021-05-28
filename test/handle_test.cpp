
#include "../environment.h"
#include <iostream>
#include <thread>
#include <string>
#include "libplatform/libplatform.h"
#include<cmath>
#include <bitset>

template <class T>
class Handle {
private:
  T* _ptr;
public:
  explicit Handle(T* ptr):_ptr(ptr) {};
  template <class S>
  bool operator==(Handle<S>& that) {
    return _ptr == that._ptr;
  };
  bool isEmpty () {
    return _ptr == nullptr;
  }
  void clear () {
    _ptr = nullptr;
  }
  T* operator->() const { return _ptr; }

  T* operator*() const { return _ptr; }
};

TEST(handle_test, custom_handle) {
  std::string str1("string");
  std::string str3("string");
  Handle<std::string> handle1(&str1);
  Handle<std::string> handle2(&str1);
  Handle<std::string> handle3(&str3);
  EXPECT_TRUE(handle1 == handle2);
  EXPECT_TRUE(**handle1 == **handle3);
  EXPECT_TRUE(handle1->size() == 6);
  EXPECT_FALSE(handle1.isEmpty());
  handle1.clear();
  EXPECT_TRUE(handle1.isEmpty());
}

TEST_F(Environment, local_new) {
  v8::Isolate* isolate = getIsolate();
  // 在使用句柄之前。必须创建一个 句柄作用域。
  v8::HandleScope handleScope(isolate);
  // 传教一个上下文句柄
  v8::Local<v8::Context> context = v8::Context::New(isolate);
  v8::Local<v8::Context> context1(context);
  EXPECT_FALSE(&context == &context1);
  // 本地句柄重载了 == 运算符。两个句柄的比较比较的是句柄的指针
  EXPECT_TRUE(context == context1);
  v8::Local<v8::Context> context2 = v8::Local<v8::Context>::New(isolate, context);
  EXPECT_FALSE(&context == &context2);
  EXPECT_TRUE(context == context2);
}

TEST_F(Environment, local_isEmpty) {
  v8::Isolate* isolate = getIsolate();
  isolate->SetAbortOnUncaughtExceptionCallback([](v8::Isolate* isolate) -> bool{
    return false;
  });
  // 在使用句柄之前。必须创建一个 句柄作用域。
  v8::HandleScope handleScope(isolate);
  // 传教一个上下文句柄
  v8::Local<v8::Context> context = v8::Context::New(isolate);
  EXPECT_FALSE(context->Global()->Get(context, v8::String::NewFromUtf8Literal(isolate, "Promise")).IsEmpty());
  // 获取一个undefined 的值无法转型成 对象类型
  EXPECT_TRUE(context->Global()->Get(context, v8::String::NewFromUtf8Literal(isolate, "global")).ToLocalChecked()->IsUndefined());
  EXPECT_TRUE(context->Global()->Get(context, v8::String::NewFromUtf8Literal(isolate, "global")).ToLocalChecked().As<v8::Object>().IsEmpty());

}

TEST_F(Environment, local_clear) {
  v8::Isolate* isolate = getIsolate();
  // 在使用句柄之前。必须创建一个 句柄作用域。
  v8::HandleScope handleScope(isolate);
  // 传教一个上下文句柄
  v8::Local<v8::Context> context = v8::Context::New(isolate);
  EXPECT_FALSE(context.IsEmpty());
  context.Clear();
  EXPECT_TRUE(context.IsEmpty());
}

TEST_F(Environment, local_equal) {
  v8::Isolate* isolate = getIsolate();
  // 在使用句柄之前。必须创建一个 句柄作用域。
  v8::HandleScope handleScope(isolate);

  v8::Local<v8::String> str1 = v8::String::NewFromUtf8Literal(isolate, "string");
  v8::Local<v8::String> str2 = v8::String::NewFromUtf8Literal(isolate, "string");
  EXPECT_FALSE(str1 == str2);

  v8::Local<v8::Number> num1= v8::Number::New(isolate, 1);
  v8::Local<v8::Number> num2= v8::Number::New(isolate, 1);
  EXPECT_TRUE(num1 == num2);
}

TEST_F(Environment, local_smi_object) {
  v8::Isolate* isolate = getIsolate();
  v8::HandleScope handleScope(isolate);
  v8::Local<v8::Number> numHandle = v8::Number::New(isolate,   1);
  v8::Local<v8::String> strHandle = v8::String::NewFromUtf8Literal(isolate, "string");
  std::bitset<64> numAddress(*reinterpret_cast<uint64_t*>(*numHandle));
  std::bitset<64> strAddress(*reinterpret_cast<uint64_t*>(*strHandle));
  // smi类型
  EXPECT_TRUE(numAddress[0] == 0);
  // 通过右移1 位获取整数值
  EXPECT_TRUE(*reinterpret_cast<uint64_t*>(*numHandle) >> 1 == 1);
  // 指针类型
  EXPECT_TRUE(strAddress[0] == 1);
}

TEST_F(Environment, local_smi_or_heap) {
  v8::Isolate* isolate = getIsolate();
  v8::HandleScope handleScope(isolate);
  v8::Local<v8::Number> num1= v8::Number::New(isolate,   std::pow(2, 30) -1);
  v8::Local<v8::Number> num2= v8::Number::New(isolate,   std::pow(2, 30) -1);
  EXPECT_TRUE(num1 == num2);

  num1= v8::Number::New(isolate,   std::pow(2, 32));
  num2= v8::Number::New(isolate,   std::pow(2, 32));
  EXPECT_FALSE(num1 == num2);
}

TEST_F(Environment, local_cast_and_as) {
  v8::Isolate* isolate = getIsolate();
  v8::HandleScope handleScope(isolate);
  v8::Local<v8::Context> context = v8::Context::New(isolate);
  v8::Local<v8::Value> value = context->Global()->Get(context, v8::String::NewFromUtf8Literal(isolate, "Promise")).ToLocalChecked();
  EXPECT_FALSE(value.As<v8::Promise>().IsEmpty());
  EXPECT_FALSE(v8::Local<v8::Promise>::Cast(value).IsEmpty());
  EXPECT_TRUE(v8::Promise::Cast(*value)->IsFunction());
}