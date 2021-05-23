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
    ExampleExtension() : v8::Extension("v8/ExampleExtension", source) {};

    // 模块查找函数
    v8::Local<v8::FunctionTemplate> GetNativeFunctionTemplate(v8::Isolate *isolate,v8::Local<v8::String> name) override {
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
  // 注册v8扩展
  v8::RegisterExtension(std::move(extension));
  v8::Isolate* isolate = getIsolate();
  v8::Locker locker(isolate);
  v8::HandleScope handleScope(isolate);
  // 配置生效的扩展名，默认所以扩展在所以上下文不生效，需要配置给创建上下。
  const char* extensionNames [] = {
      "v8/ExampleExtension"
  };
  // 扩展配置
  v8::ExtensionConfiguration extensionConfiguration(1, extensionNames );
  v8::Local<v8::Context> context = v8::Context::New(isolate, &extensionConfiguration);
  v8::Context::Scope context_scope(context);

  // 编译执行 add（1，2）函数
  v8::MaybeLocal<v8::Value> result =
      v8::Script::Compile(
          context, v8::String::NewFromUtf8(isolate, "add(1,2)").ToLocalChecked())
          .ToLocalChecked()
          ->Run(context);
  EXPECT_TRUE(result.ToLocalChecked()->IsNumber());
  EXPECT_TRUE(result.ToLocalChecked().As<v8::Number>()->Value() == 3);

  // 把javaScript的函数导出到c++曾执行。
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

TEST_F(Environment, createGlobalObjectTemplate) {
    v8::Isolate* isolate = getIsolate();
    v8::Locker locker(isolate);
    v8::HandleScope handleScope(isolate);

    // 创建一个对象模板
    v8::Local<v8::ObjectTemplate> objectTemplate = v8::ObjectTemplate::New(isolate);
    // 为对象模板添加属性 property 和函数 fun
    objectTemplate->Set(isolate, "property", v8::Number::New(isolate, 1));
    objectTemplate->Set(isolate, "fun", v8::FunctionTemplate::New(isolate, [](const v8::FunctionCallbackInfo<v8::Value>& info) -> void {
        info.GetReturnValue().Set(2);
    }));
    // 设置内置数组变量同名的属性
    objectTemplate->Set(isolate, "Array", v8::String::NewFromUtf8Literal(isolate, "Array"));
    {
        v8::Local<v8::Context> context1 = v8::Context::New(isolate, nullptr,  objectTemplate);
        v8::Context::Scope context_scope(context1);
        // 获取全局变量 Array
        v8::Local<v8::Value> globalArray = context1->Global()->Get(context1, v8::String::NewFromUtf8Literal(isolate, "Array")).ToLocalChecked();
        EXPECT_TRUE(globalArray->IsObject());
        EXPECT_FALSE(globalArray->IsString());

        v8::Local<v8::Value> result = v8::Script::Compile(context1, v8::String::NewFromUtf8Literal(isolate, "property")).ToLocalChecked()
                ->Run(context1).ToLocalChecked();
        EXPECT_TRUE(result.As<v8::Number>()->ToInt32(context1).ToLocalChecked()->Value() == 1);
        result = v8::Script::Compile( context1, v8::String::NewFromUtf8Literal(isolate, "fun()")).ToLocalChecked()
                ->Run(context1).ToLocalChecked();
        EXPECT_TRUE(result.As<v8::Number>()->ToInt32(context1).ToLocalChecked()->Value() == 2);
        // 改变 property 的值
        result = v8::Script::Compile(context1, v8::String::NewFromUtf8Literal(isolate, "property = 3;property")).ToLocalChecked()
                ->Run(context1).ToLocalChecked();
        EXPECT_TRUE(result.As<v8::Number>()->ToInt32(context1).ToLocalChecked()->Value() == 3);
    }
    {
        v8::Local<v8::Context> context2 = v8::Context::New(isolate, nullptr,  objectTemplate);
        v8::Context::Scope context_scope(context2);
        v8::Local<v8::Value> result = v8::Script::Compile(context2, v8::String::NewFromUtf8Literal(isolate, "property")).ToLocalChecked()
                ->Run(context2).ToLocalChecked();
        EXPECT_TRUE(result.As<v8::Number>()->ToInt32(context2).ToLocalChecked()->Value() == 1);
        result = v8::Script::Compile( context2, v8::String::NewFromUtf8Literal(isolate, "fun()")).ToLocalChecked()
                ->Run(context2).ToLocalChecked();
        EXPECT_TRUE(result.As<v8::Number>()->ToInt32(context2).ToLocalChecked()->Value() == 2);
    }
}

TEST_F(Environment, createGlobalObject) {
    v8::Isolate *isolate = getIsolate();
    v8::Locker locker(isolate);
    v8::HandleScope handleScope(isolate);
    v8::Local<v8::String> securityToken = v8::String::NewFromUtf8Literal(isolate, "Password");
    v8::Local<v8::String> property = v8::String::NewFromUtf8Literal(isolate, "property");

    v8::Local<v8::ObjectTemplate> objectTemplate = v8::ObjectTemplate::New(isolate);
    objectTemplate->Set(isolate, "key", v8::Number::New(isolate, 1));
    {
        v8::Local<v8::Context> context1 = v8::Context::New(isolate, nullptr, objectTemplate);
        context1->Enter();
        context1->SetSecurityToken(securityToken);
        context1->Global()->Set(context1, property, v8::Number::New(isolate, 1)).FromJust();


        v8::Local<v8::Context> context2 = v8::Context::New(isolate, nullptr, objectTemplate, context1->Global());
        context2->Enter();
        context2->SetSecurityToken(securityToken);

        EXPECT_TRUE(context1->Global()->Equals(context2, context2->Global()).FromJust());

        context2->Exit();
        context1->Exit();
    }
}
TEST_F(Environment, Global) {
    v8::Isolate *isolate = getIsolate();
    v8::Locker locker(isolate);
    v8::HandleScope handleScope(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    v8::Context::Scope context_scope(context);

    v8::Local<v8::Object> globalObject = context->Global();
    // 获取内置的全局变量
    v8::Local<v8::Value> promise = globalObject->Get(context, v8::String::NewFromUtf8Literal(isolate, "Promise")).ToLocalChecked();
    EXPECT_TRUE(promise->IsFunction());

    // 使用global作为自己的代理属性
    EXPECT_TRUE(globalObject->Set(context, v8::String::NewFromUtf8Literal(isolate, "global"), globalObject).FromJust());

    // 创建进程对象
    v8::Local<v8::Object> processObject = v8::Object::New(isolate);
    processObject->Set(context, v8::String::NewFromUtf8Literal(isolate, "version"),v8::String::NewFromUtf8Literal(isolate, "1.0.0")).FromJust();
    v8::Local<v8::Object> nodeEnvObject = v8::Object::New(isolate);
    nodeEnvObject->Set(context, v8::String::NewFromUtf8Literal(isolate, "NODE_ENV"),v8::String::NewFromUtf8Literal(isolate, "production")).FromJust();
    processObject->Set(context, v8::String::NewFromUtf8Literal(isolate, "env"), nodeEnvObject).FromJust();

    // 设置全局属性
    globalObject->Set(context, v8::String::NewFromUtf8Literal(isolate, "process"), processObject).FromJust();

    // setTimeout 的函数原型为 setTimeout(function() {}, 0);两个参数。第一个为回调函数，第二个为可选的时间参数
    globalObject->Set(context, v8::String::NewFromUtf8Literal(isolate, "setTimeout"),
                      v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value> &info) -> void {
                          v8::Isolate *isolate = info.GetIsolate();
                          v8::Local<v8::Context> context = isolate->GetCurrentContext();
                          v8::HandleScope handleScope(isolate);
                          // 参数个数校验
                          if (info.Length() != 2) {
                              info.GetReturnValue().Set(isolate->ThrowException(v8::String::NewFromUtf8Literal(isolate, "参数个数错误")));
                              return;
                          }
                          // 第一个参数为函数
                          if (!info[0]->IsFunction()) {
                              info.GetReturnValue().Set(isolate->ThrowException(v8::String::NewFromUtf8Literal(isolate, "第一个参数为函数")));
                              return;
                          }
                          // 第二个参数为可选的数字类型
                          if (!info[1]->IsNumber()) {
                              info.GetReturnValue().Set(isolate->ThrowException(v8::String::NewFromUtf8Literal(isolate, "第二个参数为数字类型")));
                              return;
                          }
                          // 在这里为了简单演示直接执行回调函数
                          v8::Local<v8::Function> callback = info[0].As<v8::Function>();
                          callback->Call(context, info.This(), 0, {}).ToLocalChecked();
                      }).ToLocalChecked()).FromJust();

    // 校验version
    v8::Local<v8::Value> version = v8::Script::Compile(context, v8::String::NewFromUtf8Literal(isolate,"process.version")).ToLocalChecked()
            ->Run(context).ToLocalChecked();
    EXPECT_TRUE(version->StrictEquals(v8::String::NewFromUtf8Literal(isolate, "1.0.0")));
    version = v8::Script::Compile(context,v8::String::NewFromUtf8Literal(isolate, "global.process.version")).ToLocalChecked()
            ->Run(context).ToLocalChecked();
    EXPECT_TRUE(version->StrictEquals(v8::String::NewFromUtf8Literal(isolate, "1.0.0")));
    version = globalObject->Get(context,v8::String::NewFromUtf8Literal(isolate,"process")).ToLocalChecked()
            .As<v8::Object>()->Get(context, v8::String::NewFromUtf8Literal(isolate, "version")).ToLocalChecked();
    EXPECT_TRUE(version->StrictEquals(v8::String::NewFromUtf8Literal(isolate, "1.0.0")));

    // 校验NODE_ENV
    v8::Local<v8::Value> nodeEnv = v8::Script::Compile(context, v8::String::NewFromUtf8(isolate,"process.env.NODE_ENV").ToLocalChecked()).ToLocalChecked()
            ->Run(context).ToLocalChecked();
    EXPECT_TRUE(nodeEnv->StrictEquals(v8::String::NewFromUtf8Literal(isolate, "production")));
    nodeEnv = v8::Script::Compile(context, v8::String::NewFromUtf8(isolate,"global.process.env.NODE_ENV").ToLocalChecked()).ToLocalChecked()
            ->Run(context).ToLocalChecked();
    EXPECT_TRUE(nodeEnv->StrictEquals(v8::String::NewFromUtf8Literal(isolate, "production")));
    nodeEnv = globalObject->Get(context, v8::String::NewFromUtf8Literal(isolate,"process")).ToLocalChecked()
            .As<v8::Object>()->Get(context, v8::String::NewFromUtf8Literal(isolate, "env")).ToLocalChecked()
            .As<v8::Object>()->Get(context,v8::String::NewFromUtf8Literal(isolate,"NODE_ENV")).ToLocalChecked();
    EXPECT_TRUE(nodeEnv->StrictEquals(v8::String::NewFromUtf8Literal(isolate, "production")));


    // 校验setTimeout
    v8::Script::Compile(context,v8::String::NewFromUtf8Literal(isolate, "setTimeout(() => {}, 0);")).ToLocalChecked()
            ->Run(context).ToLocalChecked();
       v8::Script::Compile(context, v8::String::NewFromUtf8(isolate,"global.setTimeout(() => {}, 0);").ToLocalChecked()).ToLocalChecked()
            ->Run(context).ToLocalChecked();
    v8::Local<v8::Value> setTimeout = globalObject->Get(context, v8::String::NewFromUtf8Literal(isolate,"setTimeout")).ToLocalChecked();

    EXPECT_TRUE(setTimeout->IsFunction());

}