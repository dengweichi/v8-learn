//
// Created by CF on 2021/5/21.
//
#include "../environment.h"
#include <iostream>
#include <thread>
#include <string>
#include "libplatform/libplatform.h"

TEST_F(Environment, context_new) {
  v8::Isolate* isolate = getIsolate();
  v8::Locker locker(isolate);
  v8::HandleScope handleScope(isolate);
  v8::Local<v8::Context> context = v8::Context::New(isolate);
  v8::Local<v8::Function> function = v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value>& info)-> void {
      info.GetReturnValue().Set(1);
  }).ToLocalChecked();
  v8::Local<v8::Value> result = function->Call(context, v8::Undefined(isolate), 0, {}).ToLocalChecked();
  EXPECT_TRUE(result->IsNumber());
  EXPECT_TRUE(result.As<v8::Number>()->Value() == 1);
}

class ExampleExtension : public v8::Extension {
public:
  ExampleExtension() : v8::Extension("v8/ExampleExtension", source){};

  v8::Local<v8::FunctionTemplate>
  GetNativeFunctionTemplate(v8::Isolate *isolate,
                            v8::Local<v8::String> name) override {
    if (name->StringEquals(v8::String::NewFromUtf8Literal(isolate, "get"))) {
      return v8::FunctionTemplate::New(isolate, get);
    }
    return v8::FunctionTemplate::New(isolate, add);
  };

  static void add(const v8::FunctionCallbackInfo<v8::Value> &info) {
    v8::Isolate *isolate = info.GetIsolate();
    v8::HandleScope handleScope(isolate);
    // 参数校验
    if (info.Length() != 2 || !info[0]->IsNumber() || !info[1]->IsNumber()) {
      isolate->ThrowException(v8::Exception::Error(
          v8::String::NewFromUtf8Literal(isolate, "参数错误")));
    }
    // 数据转换
    double first = info[0].As<v8::Number>()->Value();
    double second = info[1].As<v8::Number>()->Value();
    double result = first + second;
    info.GetReturnValue().Set(result);
  }
  static void get(const v8::FunctionCallbackInfo<v8::Value> &info) {
    v8::Isolate *isolate = info.GetIsolate();
    v8::HandleScope handleScope(isolate);
    info.GetReturnValue().Set(
        v8::String::NewFromUtf8Literal(isolate, "hello word"));
  }

private:
  constexpr static const char *const source = "native function add();"
                                              "native function get();";
};
TEST_F(Environment, ExtensionConfiguration){
  std::unique_ptr<ExampleExtension> extension = std::make_unique<ExampleExtension>();
  v8::RegisterExtension(std::move(extension));

  v8::Isolate* isolate = getIsolate();
  v8::Locker locker(isolate);
  v8::HandleScope handleScope(isolate);
  const char* extensionNames [] = {
      "v8/ExampleExtension"
  };
  v8::ExtensionConfiguration extensionConfiguration(1, extensionNames );
  v8::Local<v8::Context> context = v8::Context::New(isolate, &extensionConfiguration);
  v8::Context::Scope context_scope(context);
  v8::MaybeLocal<v8::Value> result =
      v8::Script::Compile(
          context, v8::String::NewFromUtf8(isolate, "add(1,2)").ToLocalChecked())
          .ToLocalChecked()
          ->Run(context);
  EXPECT_TRUE(result.ToLocalChecked()->IsNumber());
  EXPECT_TRUE(result.ToLocalChecked().As<v8::Number>()->Value() == 3);
  result =
      v8::Script::Compile(
          context, v8::String::NewFromUtf8(isolate, "get").ToLocalChecked())
          .ToLocalChecked()
          ->Run(context);
  EXPECT_TRUE(result.ToLocalChecked()->IsFunction());
  v8::Local<v8::Function> get = result.ToLocalChecked().As<v8::Function>();
  result = get->Call(context, v8::Undefined(isolate), 0, {});
  EXPECT_TRUE(result.ToLocalChecked()->IsString());
  EXPECT_TRUE(result.ToLocalChecked().As<v8::String>()->StrictEquals(v8::String::NewFromUtf8(isolate, "hello word").ToLocalChecked()));
}

TEST_F(Environment, global_object) {
  v8::Isolate* isolate = getIsolate();
  v8::Locker locker(isolate);
  v8::HandleScope handleScope(isolate);
  v8::Local<v8::String> securityToken = v8::String::NewFromUtf8(isolate, "token").ToLocalChecked();
  {
    v8::Local<v8::Context> context1 = v8::Context::New(isolate);
    context1->Enter();
    context1->SetSecurityToken(securityToken);
    context1->Global()->Set(context1, v8::String::NewFromUtf8(isolate, "result").ToLocalChecked(), v8::Number::New(isolate, 1));

    v8::Local<v8::Context> context2 = v8::Context::New(isolate, nullptr, v8::MaybeLocal<v8::ObjectTemplate>(), context1->Global());
    context2->Enter();
    context2->SetSecurityToken(securityToken);
    v8::Local<v8::Value> result = context2->Global()->Get(context2, v8::String::NewFromUtf8(isolate, "result").ToLocalChecked()).ToLocalChecked();
    EXPECT_TRUE(result->IsUndefined());

    context2->Exit();
    context1->Exit();
  }

}