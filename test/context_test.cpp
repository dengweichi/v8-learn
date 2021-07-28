//
// Created by CF on 2021/5/21.
//
#include "./base/environment.h"
#include "libplatform/libplatform.h"
#include <iostream>
#include <string>
#include <thread>

TEST_F(Environment, context_new) {
    v8::Isolate *isolate = getIsolate();
    v8::Locker locker(isolate);
    v8::HandleScope handleScope(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    v8::Local<v8::Function> function = v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value> &info) -> void {
                                           info.GetReturnValue().Set(1);
                                       }).ToLocalChecked();
    v8::Local<v8::Value> result = function->Call(context, v8::Undefined(isolate), 0, {}).ToLocalChecked();
    EXPECT_TRUE(result->IsNumber());
    EXPECT_TRUE(result.As<v8::Number>()->Value() == 1);
}

class ExampleExtension : public v8::Extension {
public:
    ExampleExtension() : v8::Extension("v8/ExampleExtension", source){};

    // 模块查找函数
    v8::Local<v8::FunctionTemplate> GetNativeFunctionTemplate(v8::Isolate *isolate, v8::Local<v8::String> name) override {
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
TEST_F(Environment, context_ExtensionConfiguration) {
    std::unique_ptr<ExampleExtension> extension = std::make_unique<ExampleExtension>();
    // 注册v8扩展
    v8::RegisterExtension(std::move(extension));
    v8::Isolate *isolate = getIsolate();
    v8::Locker locker(isolate);
    v8::HandleScope handleScope(isolate);
    // 配置生效的扩展名，默认所以扩展在所以上下文不生效，需要配置给创建上下。
    const char *extensionNames[] = {
            "v8/ExampleExtension"};
    // 扩展配置
    v8::ExtensionConfiguration extensionConfiguration(1, extensionNames);
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

TEST_F(Environment, context_GlobalObjectTemplate) {
    v8::Isolate *isolate = getIsolate();
    v8::Locker locker(isolate);
    v8::HandleScope handleScope(isolate);

    // 创建一个对象模板
    v8::Local<v8::ObjectTemplate> objectTemplate = v8::ObjectTemplate::New(isolate);
    // 为对象模板添加属性 property 和函数 fun
    objectTemplate->Set(isolate, "property", v8::Number::New(isolate, 1));
    objectTemplate->Set(isolate, "fun", v8::FunctionTemplate::New(isolate, [](const v8::FunctionCallbackInfo<v8::Value> &info) -> void {
                            info.GetReturnValue().Set(2);
                        }));
    // 设置内置数组变量同名的属性.
    objectTemplate->Set(isolate, "Array", v8::String::NewFromUtf8Literal(isolate, "Array"));
    {
        v8::Local<v8::Context> context1 = v8::Context::New(isolate, nullptr, objectTemplate);
        v8::Context::Scope context_scope(context1);
        // 获取全局变量 Array
        v8::Local<v8::Value> globalArray = context1->Global()->Get(context1, v8::String::NewFromUtf8Literal(isolate, "Array")).ToLocalChecked();
        EXPECT_TRUE(globalArray->IsObject());
        EXPECT_FALSE(globalArray->IsString());

        v8::Local<v8::Value> result = v8::Script::Compile(context1, v8::String::NewFromUtf8Literal(isolate, "property")).ToLocalChecked()->Run(context1).ToLocalChecked();
        EXPECT_TRUE(result.As<v8::Number>()->ToInt32(context1).ToLocalChecked()->Value() == 1);
        result = v8::Script::Compile(context1, v8::String::NewFromUtf8Literal(isolate, "fun()")).ToLocalChecked()->Run(context1).ToLocalChecked();
        EXPECT_TRUE(result.As<v8::Number>()->ToInt32(context1).ToLocalChecked()->Value() == 2);
        // 改变 property 的值
        result = v8::Script::Compile(context1, v8::String::NewFromUtf8Literal(isolate, "property = 3;property")).ToLocalChecked()->Run(context1).ToLocalChecked();
        EXPECT_TRUE(result.As<v8::Number>()->ToInt32(context1).ToLocalChecked()->Value() == 3);
    }
    {
        v8::Local<v8::Context> context2 = v8::Context::New(isolate, nullptr, objectTemplate);
        v8::Context::Scope context_scope(context2);
        v8::Local<v8::Value> result = v8::Script::Compile(context2, v8::String::NewFromUtf8Literal(isolate, "property")).ToLocalChecked()->Run(context2).ToLocalChecked();
        EXPECT_TRUE(result.As<v8::Number>()->ToInt32(context2).ToLocalChecked()->Value() == 1);
        result = v8::Script::Compile(context2, v8::String::NewFromUtf8Literal(isolate, "fun()")).ToLocalChecked()->Run(context2).ToLocalChecked();
        EXPECT_TRUE(result.As<v8::Number>()->ToInt32(context2).ToLocalChecked()->Value() == 2);
    }
}

TEST_F(Environment, context_globalObject) {
    v8::Isolate *isolate = getIsolate();
    v8::Locker locker(isolate);
    v8::HandleScope handleScope(isolate);
    v8::Local<v8::String> securityToken = v8::String::NewFromUtf8Literal(isolate, "Password");
    v8::Local<v8::Context> context1 = v8::Context::New(isolate, nullptr);
    v8::Local<v8::String> property = v8::String::NewFromUtf8Literal(isolate, "property");
    {
        context1->Enter();
        // 设设置安全令牌。看下面的解析。
        context1->SetSecurityToken(securityToken);
        v8::Local<v8::Object> global1 = context1->Global();
        // 再全局对象上设置熟悉 property = 1
        EXPECT_TRUE(global1->Set(context1, property, v8::Number::New(isolate, 1)).FromJust());
        context1->Exit();
        EXPECT_TRUE(global1->Equals(context1, context1->Global()).FromJust());

        // 把context1的全局对象作为创建 context2
        v8::Local<v8::Context> context2 = v8::Context::New(isolate, nullptr, v8::MaybeLocal<v8::ObjectTemplate>(), global1);
        EXPECT_FALSE(global1->Equals(context1, context1->Global()).FromJust());

        context2->Enter();
        context2->SetSecurityToken(securityToken);
        v8::Local<v8::Object> global2 = context2->Global();
        // context1 和context2 的全局代理是同一个
        EXPECT_TRUE(global1->Equals(context2, context2->Global()).FromJust());
        {
            //全局对象的状态将被完全重置并且仅保留对象标识。
            EXPECT_TRUE(global2->Get(context2, property).ToLocalChecked()->IsUndefined());
            v8::Context::Scope context_scope(context1);
            EXPECT_TRUE(global1->Get(context1, property).ToLocalChecked()->IsUndefined());
            EXPECT_TRUE(context1->Global()->Get(context1, property).ToLocalChecked().As<v8::Number>()->Value() == 1);
        }
        context2->Exit();
        {
            context1->Enter();
            EXPECT_TRUE(global1->Set(context1, property, v8::Number::New(isolate, 2)).FromJust());
            EXPECT_TRUE(global1->Get(context1, property).ToLocalChecked()->ToInt32(context1).ToLocalChecked()->Value() == 2);
            EXPECT_TRUE(global1->Equals(context2, context2->Global()).FromJust());
            context1->Exit();
            context2->Enter();
            EXPECT_TRUE(global2->Get(context2, property).ToLocalChecked()->ToInt32(context2).ToLocalChecked()->Value() == 2);
            EXPECT_TRUE(global2->Set(context2, property, v8::Number::New(isolate, 3)).FromJust());
            context2->Exit();
            context1->Enter();
            EXPECT_TRUE(global1->Get(context1, property).ToLocalChecked()->ToInt32(context1).ToLocalChecked()->Value() == 3);
            EXPECT_TRUE(global1->Get(context2, property).ToLocalChecked()->ToInt32(context2).ToLocalChecked()->Value() == 3);
            context1->Exit();
        }
    }
}


TEST_F(Environment, context_MicrotaskQueue) {
    v8::Isolate *isolate = getIsolate();
    v8::Locker locker(isolate);
    v8::HandleScope handleScope(isolate);
    // 创建微任务队列，微任务队列的执行策略是由嵌入式应用主动执行
    std::unique_ptr<v8::MicrotaskQueue> microtaskQueuePtr = v8::MicrotaskQueue::New(isolate, v8::MicrotasksPolicy::kExplicit);
    v8::MicrotaskQueue *microtaskQueue = microtaskQueuePtr.get();
    v8::Local<v8::Context> context = v8::Context::New(isolate, nullptr, v8::MaybeLocal<v8::ObjectTemplate>(), v8::MaybeLocal<v8::Value>(), v8::DeserializeInternalFieldsCallback(), microtaskQueue);
    int data = 0;
    isolate->EnqueueMicrotask([](void *data) -> void {
        (*static_cast<int *>(data))++;
    },
                              &data);
    EXPECT_TRUE(context->GetMicrotaskQueue() == microtaskQueue);
    isolate->PerformMicrotaskCheckpoint();
    EXPECT_TRUE(data == 1);
}

TEST_F(Environment, context_Global) {
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
    processObject->Set(context, v8::String::NewFromUtf8Literal(isolate, "version"), v8::String::NewFromUtf8Literal(isolate, "1.0.0")).FromJust();
    v8::Local<v8::Object> nodeEnvObject = v8::Object::New(isolate);
    nodeEnvObject->Set(context, v8::String::NewFromUtf8Literal(isolate, "NODE_ENV"), v8::String::NewFromUtf8Literal(isolate, "production")).FromJust();
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
                      }).ToLocalChecked())
            .FromJust();

    // 校验version
    v8::Local<v8::Value> version = v8::Script::Compile(context, v8::String::NewFromUtf8Literal(isolate, "process.version")).ToLocalChecked()->Run(context).ToLocalChecked();
    EXPECT_TRUE(version->StrictEquals(v8::String::NewFromUtf8Literal(isolate, "1.0.0")));
    version = v8::Script::Compile(context, v8::String::NewFromUtf8Literal(isolate, "global.process.version")).ToLocalChecked()->Run(context).ToLocalChecked();
    EXPECT_TRUE(version->StrictEquals(v8::String::NewFromUtf8Literal(isolate, "1.0.0")));
    version = globalObject->Get(context, v8::String::NewFromUtf8Literal(isolate, "process")).ToLocalChecked().As<v8::Object>()->Get(context, v8::String::NewFromUtf8Literal(isolate, "version")).ToLocalChecked();
    EXPECT_TRUE(version->StrictEquals(v8::String::NewFromUtf8Literal(isolate, "1.0.0")));

    // 校验NODE_ENV
    v8::Local<v8::Value> nodeEnv = v8::Script::Compile(context, v8::String::NewFromUtf8(isolate, "process.env.NODE_ENV").ToLocalChecked()).ToLocalChecked()->Run(context).ToLocalChecked();
    EXPECT_TRUE(nodeEnv->StrictEquals(v8::String::NewFromUtf8Literal(isolate, "production")));
    nodeEnv = v8::Script::Compile(context, v8::String::NewFromUtf8(isolate, "global.process.env.NODE_ENV").ToLocalChecked()).ToLocalChecked()->Run(context).ToLocalChecked();
    EXPECT_TRUE(nodeEnv->StrictEquals(v8::String::NewFromUtf8Literal(isolate, "production")));
    nodeEnv = globalObject->Get(context, v8::String::NewFromUtf8Literal(isolate, "process")).ToLocalChecked().As<v8::Object>()->Get(context, v8::String::NewFromUtf8Literal(isolate, "env")).ToLocalChecked().As<v8::Object>()->Get(context, v8::String::NewFromUtf8Literal(isolate, "NODE_ENV")).ToLocalChecked();
    EXPECT_TRUE(nodeEnv->StrictEquals(v8::String::NewFromUtf8Literal(isolate, "production")));


    // 校验setTimeout
    v8::Script::Compile(context, v8::String::NewFromUtf8Literal(isolate, "setTimeout(() => {}, 0);")).ToLocalChecked()->Run(context).ToLocalChecked();
    v8::Script::Compile(context, v8::String::NewFromUtf8(isolate, "global.setTimeout(() => {}, 0);").ToLocalChecked()).ToLocalChecked()->Run(context).ToLocalChecked();
    v8::Local<v8::Value> setTimeout = globalObject->Get(context, v8::String::NewFromUtf8Literal(isolate, "setTimeout")).ToLocalChecked();

    EXPECT_TRUE(setTimeout->IsFunction());
}

TEST_F(Environment, context_SecurityChecks) {
    v8::Isolate *isolate = getIsolate();
    isolate->SetAbortOnUncaughtExceptionCallback([](v8::Isolate *isolate) -> bool {
        return false;
    });
    v8::Locker locker(isolate);
    v8::HandleScope handleScope(isolate);
    v8::Local<v8::String> mainDomain = v8::String::NewFromUtf8Literal(isolate, "https://liebao.cn");
    v8::Local<v8::String> otherDomain = v8::String::NewFromUtf8Literal(isolate, "https:://pdf.liebao.cn");
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    v8::Context::Scope context_scope(context);
    context->SetSecurityToken(mainDomain);
    // 在全局函数中注入 fun 函数 和全局变量 global
    const char *source = "function fun () {\n"
                         " return this.global;\n"
                         "};"
                         "var global = 0;";
    v8::Script::Compile(context, v8::String::NewFromUtf8(isolate, source).ToLocalChecked()).ToLocalChecked()->Run(context).ToLocalChecked();
    // 获取编译全局函数fun
    v8::Local<v8::Function> fun = context->Global()->Get(context, v8::String::NewFromUtf8Literal(isolate, "fun")).ToLocalChecked().As<v8::Function>();
    // 获取全局变量
    v8::Local<v8::Number> global = context->Global()->Get(context, v8::String::NewFromUtf8Literal(isolate, "global")).ToLocalChecked().As<v8::Number>();
    EXPECT_TRUE(global->Value() == 0);

    {
        v8::Local<v8::Context> context1 = v8::Context::New(isolate);
        v8::Context::Scope context_scope1(context1);
        // 设置和外部上下文同一个 SetSecurityToken
        context1->SetSecurityToken(mainDomain);
        // 设置全局变量
        v8::Script::Compile(context1, v8::String::NewFromUtf8(isolate, "var global= 1;").ToLocalChecked()).ToLocalChecked()->Run(context1).ToLocalChecked();
        v8::Local<v8::Value> result = fun->Call(context1, context1->Global(), 0, {}).ToLocalChecked();
        EXPECT_TRUE(result->IsNumber());
        EXPECT_TRUE(result.As<v8::Number>()->Value() == 1);
    }
    {
        v8::Local<v8::Context> context2 = v8::Context::New(isolate);
        v8::Context::Scope context_scope1(context2);
        // 设置新的 securityToken
        context2->SetSecurityToken(otherDomain);
        v8::TryCatch tryCatch(isolate);
        EXPECT_TRUE(fun->Call(context2, context2->Global(), 0, {}).IsEmpty());
        EXPECT_TRUE(tryCatch.HasCaught());
    }
    {
        v8::Local<v8::Context> context3 = v8::Context::New(isolate);
        v8::Context::Scope context_scope3(context3);
        context3->SetSecurityToken(mainDomain);
        // 使用 parent设置context3的全局对象对 context的全局上下文的引用。
        context3->Global()->Set(context3, v8::String::NewFromUtf8Literal(isolate, "parent"), context->Global()).FromJust();
        const char *source = "function fun () {\n"
                             " return 1;\n"
                             "};"
                             "parent.fun();";
        v8::Local<v8::Value> result = v8::Script::Compile(context3, v8::String::NewFromUtf8(isolate, source).ToLocalChecked()).ToLocalChecked()->Run(context3).ToLocalChecked();
        EXPECT_TRUE(result->IsNumber());
        EXPECT_TRUE(result.As<v8::Number>()->Value() == 0);

        // 使用 iframe3设置context1的全局对象对 context3的全局上下文的引用。
        context->Global()->Set(context, v8::String::NewFromUtf8Literal(isolate, "iframe3"), context3->Global()).FromJust();
        result = v8::Script::Compile(context, v8::String::NewFromUtf8(isolate, "iframe3.fun();").ToLocalChecked()).ToLocalChecked()->Run(context).ToLocalChecked();
        EXPECT_TRUE(result->IsNumber());
        EXPECT_TRUE(result.As<v8::Number>()->Value() == 1);
    }
    {
        v8::Local<v8::Context> context4 = v8::Context::New(isolate);
        v8::Context::Scope context_scope4(context4);
        context4->SetSecurityToken(otherDomain);
        // 使用 parent设置context3的全局对象对 context的全局上下文的引用。
        context4->Global()->Set(context4, v8::String::NewFromUtf8Literal(isolate, "parent"), context->Global()).FromJust();

        v8::TryCatch tryCatch(isolate);
        EXPECT_TRUE(v8::Script::Compile(context4, v8::String::NewFromUtf8(isolate, "parent.fun()").ToLocalChecked()).ToLocalChecked()->Run(context4).IsEmpty());
        EXPECT_TRUE(tryCatch.HasCaught());
    }
}

TEST_F(Environment, context_embedderData) {
    v8::Isolate *isolate = getIsolate();
    v8::Locker locker(isolate);
    v8::HandleScope handleScope(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    v8::Context::Scope context_scope(context);
    // 获取相对当前隔离实例唯一插槽域
    uint32_t index = context->GetNumberOfEmbedderDataFields();
    v8::Local<v8::String> data = v8::String::NewFromUtf8Literal(isolate, "data");
    context->SetEmbedderData(index, data);

    v8::Persistent<v8::Context, v8::CopyablePersistentTraits<v8::Context>> persistentContext(isolate, context);
    std::thread thread([](v8::Isolate *isolate, v8::Persistent<v8::Context, v8::CopyablePersistentTraits<v8::Context>> persistentContext, uint32_t index) {
        v8::Local<v8::Context> context = persistentContext.Get(isolate);
        EXPECT_TRUE(context->GetEmbedderData(index)->IsString());
        EXPECT_TRUE(context->GetEmbedderData(index).As<v8::String>()->StringEquals(v8::String::NewFromUtf8Literal(isolate, "data")));
    },
                       isolate, persistentContext, index);
    thread.join();
    EXPECT_TRUE(context->GetEmbedderData(index)->IsString());
    EXPECT_TRUE(context->GetEmbedderData(index).As<v8::String>()->StringEquals(v8::String::NewFromUtf8Literal(isolate, "data")));
}

TEST_F(Environment, context_AlignedPointerFromEmbedderData) {
    v8::Isolate *isolate = getIsolate();
    v8::Locker locker(isolate);
    v8::HandleScope handleScope(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    v8::Context::Scope context_scope(context);
    // 获取相对当前隔离实例唯一插槽域
    uint32_t index = context->GetNumberOfEmbedderDataFields();
    std::string data("data");
    context->SetAlignedPointerInEmbedderData(index, &data);

    v8::Persistent<v8::Context, v8::CopyablePersistentTraits<v8::Context>> persistentContext(isolate, context);
    std::thread thread([](v8::Isolate *isolate, v8::Persistent<v8::Context, v8::CopyablePersistentTraits<v8::Context>> persistentContext, uint32_t index) {

        v8::Local<v8::Context> context = persistentContext.Get(isolate);
        EXPECT_TRUE(*static_cast<std::string *>(context->GetAlignedPointerFromEmbedderData(index)) == "data");
    },
                       isolate, persistentContext, index);
    thread.join();
    EXPECT_TRUE(static_cast<std::string *>(context->GetAlignedPointerFromEmbedderData(index)) == &data);
}


TEST_F(Environment, context_DetachGlobal) {
    v8::Isolate *isolate = getIsolate();
    // 设置为拦截异常不中止进程执行
    isolate->SetAbortOnUncaughtExceptionCallback([](v8::Isolate *isolate) -> bool {
        return false;
    });
    v8::Locker locker(isolate);
    v8::HandleScope handleScope(isolate);

    v8::Local<v8::String> mainDomain = v8::String::NewFromUtf8Literal(isolate, "https://liebao.cn");

    v8::Local<v8::Context> context1 = v8::Context::New(isolate);
    context1->SetSecurityToken(mainDomain);
    {
        v8::Context::Scope context_scope(context1);
        // 设置了全局变量
        EXPECT_FALSE(v8::Script::Compile(context1,
                                         v8::String::NewFromUtf8Literal(isolate, "var property = 1;"))
                             .ToLocalChecked()
                             ->Run(context1)
                             .ToLocalChecked()
                             .IsEmpty());
    }

    v8::Local<v8::Context> context2 = v8::Context::New(isolate);
    context2->SetSecurityToken(mainDomain);
    {
        v8::Context::Scope context_scope(context2);
        EXPECT_TRUE(context2->Global()->Set(context2, v8::String::NewFromUtf8Literal(isolate, "parent"), context1->Global()).FromJust());
        v8::Local<v8::Value> result = v8::Script::Compile(context2, v8::String::NewFromUtf8Literal(
                                                                            isolate, "parent.property"))
                                              .ToLocalChecked()
                                              ->Run(context2)
                                              .ToLocalChecked();
        EXPECT_TRUE(result->ToInt32(context2).ToLocalChecked()->Value() == 1);
    }
    v8::Local<v8::Object> global1 = context1->Global();
    // context1全局变量在分离前
    {
        v8::Context::Scope context_scope(context1);
        EXPECT_TRUE(global1->Equals(context1, context1->Global()).FromJust());
        EXPECT_TRUE(global1->Get(context1, v8::String::NewFromUtf8Literal(isolate, "property")).ToLocalChecked()->ToInt32(context1).ToLocalChecked()->Value() == 1);
    }
    context1->DetachGlobal();
    // context1全局变量在分离后
    {
        v8::Context::Scope context_scope(context1);
        EXPECT_FALSE(global1->Equals(context1, context1->Global()).FromJust());
        EXPECT_TRUE(global1->Get(context1, v8::String::NewFromUtf8Literal(isolate, "property")).IsEmpty());
        EXPECT_TRUE(context1->Global()->Get(context1, v8::String::NewFromUtf8Literal(isolate, "property")).ToLocalChecked()->ToInt32(context1).ToLocalChecked()->Value() == 1);
    }

    {
        v8::Context::Scope context_scope(context2);
        v8::TryCatch tryCatch(isolate);
        EXPECT_TRUE(v8::Script::Compile(context2, v8::String::NewFromUtf8Literal(
                                                          isolate, "parent.property"))
                            .ToLocalChecked()
                            ->Run(context2)
                            .IsEmpty());
        EXPECT_TRUE(tryCatch.HasCaught());
        // global1全局对象被分离，但还是存在，只是不能访问。
        EXPECT_TRUE(context2->Global()->Get(context2, v8::String::NewFromUtf8Literal(isolate, "parent")).ToLocalChecked()->IsObject());
    }

    // 把 context1 分离的全局对象绑定到 context3
    v8::Local<v8::Context> context3 = v8::Context::New(isolate, nullptr, v8::MaybeLocal<v8::ObjectTemplate>(), global1);
    context3->SetSecurityToken(mainDomain);
    EXPECT_TRUE(global1->Equals(context3, context3->Global()).FromJust());
    EXPECT_TRUE(global1->Equals(context2, context2->Global()->Get(context2, v8::String::NewFromUtf8Literal(isolate, "parent")).ToLocalChecked()).FromJust());
    EXPECT_FALSE(context1->Global()->Equals(context3, context3->Global()).FromJust());
    {
        v8::Context::Scope context_scope(context3);
        EXPECT_TRUE(context3->Global()->Get(context3, v8::String::NewFromUtf8Literal(isolate, "property")).ToLocalChecked()->IsUndefined());
        // 设置了全局变量
        EXPECT_FALSE(v8::Script::Compile(context3,
                                         v8::String::NewFromUtf8Literal(isolate, "var property = 2;"))
                             .ToLocalChecked()
                             ->Run(context3)
                             .ToLocalChecked()
                             .IsEmpty());
    }
    // context2 的parent重新绑定到 context1的全局变量种
    {
        v8::Context::Scope context_scope(context2);
        v8::TryCatch tryCatch(isolate);
        EXPECT_TRUE(v8::Script::Compile(context2, v8::String::NewFromUtf8Literal(
                                                          isolate, "parent.property"))
                            .ToLocalChecked()
                            ->Run(context2)
                            .ToLocalChecked()
                            ->ToInt32(context2)
                            .ToLocalChecked()
                            ->Value() == 2);
    }
}
bool propertyAccessCheckCallback(v8::Local<v8::Context> accessing_context,
                                 v8::Local<v8::Object> accessed_object,
                                 v8::Local<v8::Value> data) {
    return true;
}
void propertyGetter(v8::Local<v8::Name> property,
                    const v8::PropertyCallbackInfo<v8::Value> &info) {
    info.GetReturnValue().Set(1);
}
void propertySetter(v8::Local<v8::Name> property, v8::Local<v8::Value> value,
                    const v8::PropertyCallbackInfo<v8::Value> &info) {
}
void propertyQuery(v8::Local<v8::Name> property,
                   const v8::PropertyCallbackInfo<v8::Integer> &info) {
}

void propertyDelete(v8::Local<v8::Name> property,
                    const v8::PropertyCallbackInfo<v8::Boolean> &info) {
}
void propertyEnumerator(const v8::PropertyCallbackInfo<v8::Array> &info) {
}
void indexedPropertyGetter(uint32_t index,
                           const v8::PropertyCallbackInfo<v8::Value> &info) {
}
void indexPropertySetter(uint32_t index, v8::Local<v8::Value> value,
                         const v8::PropertyCallbackInfo<v8::Value> &info) {
}
void indexPropertyQuery(uint32_t index,
                        const v8::PropertyCallbackInfo<v8::Integer> &info) {
}
void indexPropertyDelete(uint32_t index,
                         const v8::PropertyCallbackInfo<v8::Boolean> &info) {
}
void indexPropertyEnumerator(const v8::PropertyCallbackInfo<v8::Array> &info) {
}

TEST_F(Environment, context_NewRemoteContext) {
    v8::Isolate *isolate = getIsolate();
    v8::Locker locker(isolate);
    v8::HandleScope handleScope(isolate);
    v8::Local<v8::ObjectTemplate> objectTemplate = v8::ObjectTemplate::New(isolate);
    // 属性访问处理器。包括普通属性和索引属性的 setter, gettter, query, delete, property。类似于代理。
    objectTemplate->SetAccessCheckCallbackAndHandler(propertyAccessCheckCallback,
                                                     v8::NamedPropertyHandlerConfiguration(propertyGetter,
                                                                                           propertySetter,
                                                                                           propertyQuery,
                                                                                           propertyDelete,
                                                                                           propertyEnumerator),
                                                     v8::IndexedPropertyHandlerConfiguration(indexedPropertyGetter,
                                                                                             indexPropertySetter,
                                                                                             indexPropertyQuery,
                                                                                             indexPropertyDelete,
                                                                                             indexPropertyEnumerator));
    v8::Local<v8::Object> globalObject = v8::Context::NewRemoteContext(isolate, objectTemplate).ToLocalChecked();
    {
        v8::Local<v8::Context> context1 = v8::Context::New(isolate);
        v8::Context::Scope context_scope(context1);
        EXPECT_TRUE(globalObject->Get(context1, v8::String::NewFromUtf8Literal(isolate, "property")).ToLocalChecked().As<v8::Number>()->Value() == 1);
    }
    {
        v8::Local<v8::Context> context2 = v8::Context::New(isolate);
        v8::Context::Scope context_scope(context2);
        EXPECT_TRUE(globalObject->Get(context2, v8::String::NewFromUtf8Literal(isolate, "property")).ToLocalChecked().As<v8::Number>()->Value() == 1);
    }
}