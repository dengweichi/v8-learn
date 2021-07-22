
#include "../environment.h"
#include "libplatform/libplatform.h"
#include <iostream>
#include <string>
#include <thread>

extern v8::Platform *g_default_platform;

bool IsAligned(unsigned long long value, unsigned long long alignment) {
    return (value & (alignment - 1)) == 0;
}
/**
 * 测试隔离实例地址空间4GB 对齐
 */
/*TEST(isolate_test, isolate_4g_address_aligned) {
    unsigned long long GB = 1024 * 1024 * 1024;
    v8::Isolate::CreateParams create_params1;
    create_params1.array_buffer_allocator =
            v8::ArrayBuffer::Allocator::NewDefaultAllocator();
    v8::Isolate::CreateParams create_params2;
    create_params2.array_buffer_allocator =
            v8::ArrayBuffer::Allocator::NewDefaultAllocator();
    v8::Isolate *isolate1 = v8::Isolate::New(create_params1);
    v8::Isolate *isolate2 = v8::Isolate::New(create_params2);
    {
        EXPECT_TRUE(IsAligned(reinterpret_cast<unsigned long long>(isolate1),
                              size_t{4} * GB));
        EXPECT_TRUE(IsAligned(reinterpret_cast<unsigned long long>(isolate2),
                              size_t{4} * GB));
    }
    isolate1->Dispose();
    isolate2->Dispose();
}*/

double globalNum = 0;
TEST(isolate_test, isolate_jsObject_communication) {
    v8::Isolate::CreateParams create_params;
    create_params.array_buffer_allocator =
            v8::ArrayBuffer::Allocator::NewDefaultAllocator();
    v8::Isolate *isolate1 = v8::Isolate::New(create_params);
    {
        v8::Isolate::Scope scope1(isolate1);
        v8::HandleScope handleScope1(isolate1);
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
    }
    v8::Isolate *isolate2 = v8::Isolate::New(create_params);
    {
        v8::Isolate::Scope scope2(isolate2);
        v8::HandleScope handleScope2(isolate2);
        v8::Local<v8::Context> context2 = v8::Context::New(isolate2);
        v8::Context::Scope context_scope2(context2);
        v8::Local<v8::Function> function2 =
                v8::Function::New(
                        context2,
                        [](const v8::FunctionCallbackInfo<v8::Value> &info) -> void {
                            info.GetReturnValue().Set(globalNum);
                        })
                        .ToLocalChecked();
        EXPECT_TRUE(context2->Global()->Set(
                                              context2, v8::String::NewFromUtf8Literal(isolate2, "globalGetFun"),
                                              function2)
                            .FromJust());
        const char *scriptSource2 = "globalGetFun()";
        v8::Local<v8::String> source2 =
                v8::String::NewFromUtf8(isolate2, scriptSource2).ToLocalChecked();
        v8::Local<v8::Script> script2 =
                v8::Script::Compile(context2, source2).ToLocalChecked();
        v8::Local<v8::Value> result2 = script2->Run(context2).ToLocalChecked();
        EXPECT_EQ(result2.As<v8::Number>()->Value(), 2);
    }
    isolate1->Dispose();
    isolate2->Dispose();
    delete create_params.array_buffer_allocator;
}

TEST(isolate_test, Multithreading) {
    v8::Isolate::CreateParams create_params;
    create_params.array_buffer_allocator = v8::ArrayBuffer::Allocator::NewDefaultAllocator();
    v8::Isolate *isolate = v8::Isolate::New(create_params);
    {
        v8::Persistent<v8::Context, v8::CopyablePersistentTraits<v8::Context>> globalContext;
        {
            v8::Isolate::Scope scope(isolate);
            v8::HandleScope handleScope(isolate);
            v8::Local<v8::Context> context = v8::Context::New(isolate);
            v8::Context::Scope context_scope(context);
            const char *scriptSource = "function fun() {"
                                       "  return 1;"
                                       "}"
                                       "fun();";
            v8::Local<v8::String> source = v8::String::NewFromUtf8(isolate, scriptSource).ToLocalChecked();
            v8::Local<v8::Script> script = v8::Script::Compile(context, source).ToLocalChecked();
            v8::Local<v8::Value> result = script->Run(context).ToLocalChecked();
            EXPECT_TRUE(context->Global()->Get(context, v8::String::NewFromUtf8Literal(isolate, "fun")).ToLocalChecked()->IsFunction());
            EXPECT_EQ(result.As<v8::Number>()->Value(), 1);
            // 把上线文句柄做持久化处理。下面章节会详细介绍
            v8::Persistent<v8::Context, v8::CopyablePersistentTraits<v8::Context>> globalContext1(isolate, context);
            globalContext = globalContext1;
        }
        std::thread thread([](v8::Isolate *isolate, v8::Persistent<v8::Context, v8::CopyablePersistentTraits<v8::Context>> context) -> void {
            v8::Isolate::Scope scope(isolate);
            v8::HandleScope handleScope(isolate);
            v8::Locker locker(isolate);
            v8::Local<v8::Context> localContext = v8::Local<v8::Context>::New(isolate, context);
            v8::Context::Scope context_scope(localContext);
            const char *scriptSource = "fun()";
            v8::Local<v8::String> source = v8::String::NewFromUtf8(isolate, scriptSource).ToLocalChecked();
            v8::Local<v8::Script> script = v8::Script::Compile(localContext, source).ToLocalChecked();
            v8::Local<v8::Value> result = script->Run(localContext).ToLocalChecked();
            EXPECT_EQ(result.As<v8::Number>()->Value(), 1);
        },
                           isolate, globalContext);
        thread.join();
        globalContext.SetWeak();
    }
    isolate->Dispose();
}

TEST(isolate_test, scope) {
    v8::Isolate::CreateParams create_params;
    create_params.array_buffer_allocator = v8::ArrayBuffer::Allocator::NewDefaultAllocator();
    v8::Isolate *isolate1 = v8::Isolate::New(create_params);

    {
        EXPECT_TRUE(v8::Isolate::GetCurrent() == nullptr);
        isolate1->Enter();
        EXPECT_TRUE(v8::Isolate::GetCurrent() == isolate1);
        isolate1->Exit();
        EXPECT_TRUE(v8::Isolate::GetCurrent() == nullptr);
    }

    {
        EXPECT_TRUE(v8::Isolate::GetCurrent() == nullptr);
        v8::Isolate::Scope scope(isolate1);
        EXPECT_TRUE(v8::Isolate::GetCurrent() == isolate1);
    }

    EXPECT_TRUE(v8::Isolate::GetCurrent() == nullptr);

    {
        v8::Isolate::Scope scope(isolate1);
        // 把隔离实例传递到其他线程使用
        std::thread thread([](v8::Isolate *isolate1) -> void {
            v8::Locker locker(isolate1);
            EXPECT_TRUE(v8::Isolate::GetCurrent() == nullptr);
            v8::Isolate::Scope scope(isolate1);
            EXPECT_TRUE(v8::Isolate::GetCurrent() == isolate1);
        },
                           isolate1);
        thread.join();
    }
    v8::Isolate *isolate2 = v8::Isolate::New(create_params);
    {
        isolate1->Enter();
        EXPECT_TRUE(v8::Isolate::GetCurrent() == isolate1);
        isolate2->Enter();
        EXPECT_TRUE(v8::Isolate::GetCurrent() == isolate2);
        isolate2->Exit();
        EXPECT_TRUE(v8::Isolate::GetCurrent() == isolate1);
        isolate1->Exit();
    }
    isolate1->Dispose();
    isolate2->Dispose();
    delete create_params.array_buffer_allocator;
}

TEST_F(Environment, isolate_data) {
    v8::Isolate *isolate = getIsolate();
    uint32_t slot = isolate->GetNumberOfDataSlots();
    std::string *data = new std::string("string");
    isolate->SetData(slot, data);
    std::thread thread([](v8::Isolate *isolate, uint32_t slot) -> void {
        auto *data = static_cast<std::string *>(isolate->GetData(slot));
        EXPECT_TRUE(*data == "string");
    },
                       isolate, slot);
    thread.join();
    std::string *slotDta = static_cast<std::string *>(isolate->GetData(slot));
    EXPECT_TRUE(slotDta == data);
    delete data;
}

class ModuleResolutionData {
public:
    ModuleResolutionData(v8::Isolate *_isolate, v8::Local<v8::Module> _module, v8::Local<v8::Promise::Resolver> _resolver) {
        isolate = _isolate;
        module.Reset(_isolate, _module);
        resolver.Reset(_isolate, _resolver);
    };
    v8::Global<v8::Module> module;
    v8::Global<v8::Promise::Resolver> resolver;
    v8::Isolate *isolate;
    ~ModuleResolutionData() {
        module.SetWeak();
        resolver.SetWeak();
    }
};

TEST_F(Environment, dynamicallyImport) {
    // 设置允许顶层await v8 9.0版本需要需要 flags 启动
    v8::V8::SetFlagsFromString("--harmony-top-level-await");
    v8::Isolate *isolate = getIsolate();
    isolate->SetMicrotasksPolicy(v8::MicrotasksPolicy::kExplicit);
    v8::Locker locker(isolate);
    {
        // 请求  import('xxx.js')执行的回调，由嵌入式应用提供回调
        isolate->SetHostImportModuleDynamicallyCallback(
                [](v8::Local<v8::Context> context,
                   v8::Local<v8::ScriptOrModule> referrer,
                   v8::Local<v8::String> specifier,
                   v8::Local<v8::FixedArray> import_assertions) -> v8::MaybeLocal<v8::Promise> {
                    // 获取该上下文的隔离实例
                    v8::Isolate *isolate = context->GetIsolate();
                    v8::Locker locker(isolate);

                    // v8::Local<v8::ScriptOrModule> referrer 为引用的模块，在这里为index。js
                    // v8::Local<v8::String> specifier, 为请求的模块名称
                    EXPECT_TRUE(specifier->StringEquals(v8::String::NewFromUtf8Literal(isolate, "https://liebao.cn/second.js")));
                    EXPECT_TRUE(referrer->GetResourceName().As<v8::String>()->StringEquals(
                            v8::String::NewFromUtf8Literal(isolate, "https://liebao.cn/index.js")));
                    // 创建 promise
                    v8::Local<v8::Promise::Resolver> resolver = v8::Promise::Resolver::New(context).ToLocalChecked();
                    const char *scriptSource = "export const fun = function () {\n"
                                               "  return 1;\n"
                                               "}\n";
                    // 脚本编译需要的编译条件。
                    v8::ScriptOrigin origin(
                            isolate, specifier,
                            0, 0, false, -1, v8::Local<v8::Value>(), false, false, true);
                    v8::ScriptCompiler::Source source(
                            v8::String::NewFromUtf8(isolate, scriptSource).ToLocalChecked(),
                            origin);
                    // 把脚本作为模块去编译
                    v8::Local<v8::Module> module =
                            v8::ScriptCompiler::CompileModule(
                                    isolate, &source,
                                    v8::ScriptCompiler::CompileOptions::kNoCompileOptions,
                                    v8::ScriptCompiler::NoCacheReason::kNoCacheNoReason)
                                    .ToLocalChecked();
                    // 实例化模块
                    module->InstantiateModule(
                                  context,
                                  [](v8::Local<v8::Context> context, v8::Local<v8::String> specifier,
                                     v8::Local<v8::FixedArray> import_assertions,
                                     v8::Local<v8::Module> referrer) -> v8::MaybeLocal<v8::Module> {
                                      return v8::MaybeLocal<v8::Module>();
                                  })
                            .FromJust();
                    v8::Local<v8::Value> result = module->Evaluate(context).ToLocalChecked();
                    // 执行事件循环
                    isolate->PerformMicrotaskCheckpoint();

                    EXPECT_TRUE(result->IsPromise());

                    ModuleResolutionData *moduleResolutionData = new ModuleResolutionData(isolate, module, resolver);
                    // 外部扩展。用于把 c++指针包装成句柄
                    v8::Local<v8::External> external = v8::External::New(isolate, moduleResolutionData);
                    // 设置Promise 回调
                    result.As<v8::Promise>()->Then(context, v8::Function::New(
                                                                    context, [](const v8::FunctionCallbackInfo<v8::Value> &info) -> void {
                                                                        v8::Isolate *isolate = info.GetIsolate();
                                                                        v8::HandleScope handleScope(isolate);
                                                                        // 把ModuleResolutionData 指针转换成 std::unique_ptr指针。在析构函数中把全局句柄变量 setWeak。可以让gc回收。
                                                                        std::unique_ptr<ModuleResolutionData> moduleResolutionData(static_cast<ModuleResolutionData *>(info.Data().As<v8::External>()->Value()));
                                                                        v8::Local<v8::Module> module = moduleResolutionData->module.Get(isolate);
                                                                        v8::Local<v8::Promise::Resolver> resolver = moduleResolutionData->resolver.Get(isolate);
                                                                        resolver->Resolve(isolate->GetCurrentContext(), module->GetModuleNamespace()).FromJust();
                                                                    },
                                                                    external)
                                                                    .ToLocalChecked())
                            .ToLocalChecked();

                    isolate->PerformMicrotaskCheckpoint();
                    return resolver->GetPromise();
                });

        // 获取模块元信息回调
        isolate->SetHostInitializeImportMetaObjectCallback(
                [](v8::Local<v8::Context> context, v8::Local<v8::Module> module,
                   v8::Local<v8::Object> meta) -> void {
                    v8::Isolate *isolate = context->GetIsolate();
                    meta->Set(context,
                              v8::String::NewFromUtf8(isolate, "url").ToLocalChecked(),
                              v8::String::NewFromUtf8(isolate, "https://liebao.cn")
                                      .ToLocalChecked());
                });

        v8::HandleScope handleScope(isolate);
        v8::Local<v8::Context> context = v8::Context::New(isolate);
        v8::Context::Scope context_scope(context);

        // 设置全局打印函数。用于输出fun()函数的输出结果
        context->Global()->Set(context, v8::String::NewFromUtf8Literal(isolate, "print"), v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value> &info) -> void {
                                                                                              EXPECT_TRUE(info[0]->IsNumber());
                                                                                              EXPECT_TRUE(info[0].As<v8::Number>()->Value() == 1);
                                                                                          }).ToLocalChecked())
                .FromJust();

        // 设置输出 import.meta.url 函数
        context->Global()->Set(context, v8::String::NewFromUtf8Literal(isolate, "printMetaUrl"), v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value> &info) -> void {
                                                                                                     EXPECT_TRUE(info[0]->IsString());
                                                                                                     EXPECT_TRUE(info[0].As<v8::String>()->StrictEquals(v8::String::NewFromUtf8Literal(info.GetIsolate(), "https://liebao.cn")));
                                                                                                 }).ToLocalChecked())
                .FromJust();

        const char *scriptSource = "const { fun } = await import('https://liebao.cn/second.js');\n"
                                   "const result = fun();\n"
                                   "printMetaUrl(import.meta.url);\n"
                                   "print(result);\n";

        // 脚本元信息
        v8::ScriptOrigin origin(
                isolate, v8::String::NewFromUtf8Literal(isolate, "https://liebao.cn/index.js"),
                0, 0, false, -1, v8::Local<v8::Value>(), false, false, true);
        // 脚本
        v8::ScriptCompiler::Source source(
                v8::String::NewFromUtf8(isolate, scriptSource).ToLocalChecked(),
                origin);

        v8::TryCatch tryCatch(isolate);
        // 把脚本以 es-module去编译
        v8::Local<v8::Module> module = v8::ScriptCompiler::CompileModule(
                                               isolate, &source,
                                               v8::ScriptCompiler::CompileOptions::kNoCompileOptions,
                                               v8::ScriptCompiler::NoCacheReason::kNoCacheNoReason)
                                               .ToLocalChecked();
        // 实例化模块
        module->InstantiateModule(
                      context,
                      [](v8::Local<v8::Context> context, v8::Local<v8::String> specifier,
                         v8::Local<v8::FixedArray> import_assertions,
                         v8::Local<v8::Module> referrer) -> v8::MaybeLocal<v8::Module> {
                          return v8::MaybeLocal<v8::Module>();
                      })
                .FromJust();
        // 执行模块;
        module->Evaluate(context).ToLocalChecked();
        // 执行微任务队列
        isolate->PerformMicrotaskCheckpoint();
    }
}

TEST_F(Environment, context) {
    v8::Isolate *isolate = getIsolate();
    v8::Locker locker(isolate);
    EXPECT_FALSE(isolate->InContext());
    EXPECT_TRUE(isolate->GetCurrentContext().IsEmpty());
    v8::HandleScope handleScope(isolate);
    v8::Local<v8::Context> context1 = v8::Context::New(isolate);
    EXPECT_FALSE(isolate->InContext());
    context1->Enter();
    EXPECT_TRUE(isolate->InContext());
    context1->Exit();
    EXPECT_FALSE(isolate->InContext());
    {
        v8::Context::Scope context_scope(context1);
        EXPECT_TRUE(isolate->InContext());
    }
    EXPECT_FALSE(isolate->InContext());
    v8::Local<v8::Context> context2 = v8::Context::New(isolate);
    context1->Enter();
    context2->Enter();
    EXPECT_TRUE(isolate->GetCurrentContext() == context2);
    context2->Exit();
    EXPECT_TRUE(isolate->GetCurrentContext() == context1);
    context1->Exit();
    EXPECT_TRUE(isolate->GetCurrentContext().IsEmpty());
}

TEST_F(Environment, ThrowException) {
    v8::Isolate *isolate = getIsolate();
    v8::Locker locker(isolate);
    v8::HandleScope handleScope(isolate);
    v8::TryCatch tryCatch(isolate);
    isolate->ThrowException(v8::String::NewFromUtf8(isolate, "error").ToLocalChecked());
    EXPECT_TRUE(tryCatch.HasCaught());
    EXPECT_TRUE(tryCatch.CanContinue());
    EXPECT_TRUE(tryCatch.Exception().As<v8::String>()->StringEquals(v8::String::NewFromUtf8(isolate, "error").ToLocalChecked()));
}


TEST_F(Environment, Microtask) {
    int status = 0;
    int microtasksCompleteCount = 0;
    v8::Isolate *isolate = getIsolate();
    v8::Locker locker(isolate);
    isolate->AddMicrotasksCompletedCallback(
            [](v8::Isolate *isolate, void *data) -> void {
                (*static_cast<int *>(data))++;
            },
            &microtasksCompleteCount);

    v8::HandleScope handleScope(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    v8::Context::Scope context_scope(context);
    isolate->EnqueueMicrotask([](void *data) -> void {
        (*static_cast<int *>(data))++;
    },
                              &status);
    v8::Script::Compile(context,
                        v8::String::NewFromUtf8(isolate, "1+1").ToLocalChecked())
            .ToLocalChecked()
            ->Run(context)
            .ToLocalChecked();
    EXPECT_TRUE(status == 1);

    isolate->EnqueueMicrotask([](void *data) -> void {
        (*static_cast<int *>(data))++;
    },
                              &status);
    v8::Script::Compile(context,
                        v8::String::NewFromUtf8(isolate, "1+1").ToLocalChecked())
            .ToLocalChecked()
            ->Run(context)
            .ToLocalChecked();
    EXPECT_TRUE(status == 2);
    EXPECT_TRUE(microtasksCompleteCount == 2);
}

TEST_F(Environment, MicrotasksPolicy) {
    int status = 0;
    int microtasksCompleteCount = 0;
    v8::Isolate *isolate = getIsolate();
    v8::Locker locker(isolate);
    auto increase = [](void *data) -> void {
        (*static_cast<int *>(data))++;
    };

    isolate->AddMicrotasksCompletedCallback(
            [](v8::Isolate *isolate, void *data) -> void {
                (*static_cast<int *>(data))++;
            },
            &microtasksCompleteCount);

    v8::HandleScope handleScope(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    v8::Context::Scope context_scope(context);

    isolate->EnqueueMicrotask(increase, &status);
    // 使用v8 微任务队列默认策略
    isolate->SetMicrotasksPolicy(v8::MicrotasksPolicy::kAuto);
    v8::Script::Compile(context,
                        v8::String::NewFromUtf8(isolate, "1+1").ToLocalChecked())
            .ToLocalChecked()
            ->Run(context);
    EXPECT_TRUE(status == 1);
    EXPECT_TRUE(microtasksCompleteCount == 1);

    isolate->EnqueueMicrotask(increase, &status);
    // 使用明确的策略
    isolate->SetMicrotasksPolicy(v8::MicrotasksPolicy::kExplicit);
    v8::Script::Compile(context,
                        v8::String::NewFromUtf8(isolate, "1+1").ToLocalChecked())
            .ToLocalChecked()
            ->Run(context)
            .ToLocalChecked();
    EXPECT_TRUE(status == 1);
    EXPECT_TRUE(microtasksCompleteCount == 1);

    isolate->PerformMicrotaskCheckpoint();
    EXPECT_TRUE(status == 2);
    EXPECT_TRUE(microtasksCompleteCount == 2);


    isolate->EnqueueMicrotask(increase, &status);
    // 使用 scope策略
    isolate->SetMicrotasksPolicy(v8::MicrotasksPolicy::kScoped);
    v8::Script::Compile(context,
                        v8::String::NewFromUtf8(isolate, "1+1").ToLocalChecked())
            .ToLocalChecked()
            ->Run(context)
            .ToLocalChecked();
    EXPECT_TRUE(status == 2);
    EXPECT_TRUE(microtasksCompleteCount == 2);

    {
        v8::MicrotasksScope scope(isolate, v8::MicrotasksScope::kRunMicrotasks);
    }
    EXPECT_TRUE(status == 3);
    EXPECT_TRUE(microtasksCompleteCount == 3);
}


TEST_F(Environment, MessageListener) {
    v8::Isolate *isolate = getIsolate();
    v8::Locker locker(isolate);
    v8::HandleScope scope(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    v8::Context::Scope context_scope(context);
    {
        auto warningListener = [](v8::Local<v8::Message> message, v8::Local<v8::Value> data) -> void {
            EXPECT_EQ(v8::Isolate::kMessageWarning, message->ErrorLevel());
        };
        isolate->AddMessageListenerWithErrorLevel(warningListener, v8::Isolate::kMessageAll);
        // 没有声明
        const char *source = " params = 1;";
        v8::Script::Compile(context,
                            v8::String::NewFromUtf8(isolate, source).ToLocalChecked())
                .ToLocalChecked()
                ->Run(context)
                .ToLocalChecked();
        isolate->RemoveMessageListeners(warningListener);
    }
    {
        auto errorListener = [](v8::Local<v8::Message> message, v8::Local<v8::Value> data) -> void {
            EXPECT_EQ(v8::Isolate::kMessageError, message->ErrorLevel());
        };
        isolate->AddMessageListenerWithErrorLevel(errorListener, v8::Isolate::kMessageAll);
        // 严格模式，变量未定义
        const char *source = "throw new Error('error');";
        EXPECT_TRUE(v8::Script::Compile(context,
                                        v8::String::NewFromUtf8(isolate, source).ToLocalChecked())
                            .ToLocalChecked()
                            ->Run(context)
                            .IsEmpty());
        isolate->RemoveMessageListeners(errorListener);
    }
}
TEST_F(Environment, SetAbortOnUncaughtException) {
    v8::V8::SetFlagsFromString("--abort-on-uncaught-exception");
    v8::Isolate *isolate = getIsolate();
    v8::HandleScope scope(isolate);
    v8::Locker locker(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    v8::Context::Scope context_scope(context);
    {
        isolate->SetAbortOnUncaughtExceptionCallback([](v8::Isolate *isolate) -> bool {
            return false;
        });
        const char *source = "throw new Error('error');";
        v8::MaybeLocal<v8::Value> result =
                v8::Script::Compile(
                        context, v8::String::NewFromUtf8(isolate, source).ToLocalChecked())
                        .ToLocalChecked()
                        ->Run(context);
        EXPECT_TRUE(result.IsEmpty());
        EXPECT_FALSE(isolate->IsDead());
        isolate->SetAbortOnUncaughtExceptionCallback(nullptr);
    }
}

namespace v9 {
    namespace internal {
        class Isolate {
        private:
            const char *name = "name";

        public:
            static Isolate *New() {
                return new Isolate();
            };
            const char *getName() {
                return name;
            }
        };
    }// namespace internal
    class Isolate {
    public:
        static Isolate *New() {
            return reinterpret_cast<Isolate *>(internal::Isolate::New());
        };
        const char *getName() {
            internal::Isolate *isolate = reinterpret_cast<internal::Isolate *>(this);
            return isolate->getName();
        }
        ~Isolate() {
            internal::Isolate *isolate = reinterpret_cast<internal::Isolate *>(this);
            delete isolate;
        }
    };
}// namespace v9

TEST(isoate_test, proxy) {
    v9::Isolate *isolate = v9::Isolate::New();
    EXPECT_STREQ(isolate->getName(), "name");
    EXPECT_TRUE(sizeof(v9::Isolate) == 1);
}
