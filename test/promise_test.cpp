
#include "../environment.h"
#include "libplatform/libplatform.h"
#include <iostream>
#include <string>
#include <thread>

static int global_promise = 0;
TEST_F(Environment, promise_then_resolver) {
    v8::Isolate *isolate = getIsolate();
    {
        v8::Isolate::Scope scope(isolate);
        v8::HandleScope handleScope(isolate);
        v8::Local<v8::Context> context = v8::Context::New(isolate);

        // 设置微任务队列策略为明确执行得
        isolate->SetMicrotasksPolicy(v8::MicrotasksPolicy::kExplicit);
        v8::Context::Scope context_scope(context);
        v8::Local<v8::Promise::Resolver> resolver;
        EXPECT_TRUE(v8::Promise::Resolver::New(context).ToLocal(&resolver));
        v8::Local<v8::Promise> promise = resolver->GetPromise();

        EXPECT_TRUE(promise->Then(context,
                                  v8::Function::New(context,
                                                    [](const v8::FunctionCallbackInfo<v8::Value> &info) -> void {
                                                        global_promise++;
                                                        EXPECT_EQ(info.Length(), 1);
                                                        EXPECT_TRUE(info[0]->IsNumber());
                                                        EXPECT_EQ(info[0].As<v8::Number>()->Value(), 1);
                                                    }).ToLocalChecked())
                            .ToLocal(&promise));

        // 持久化上下文和promise resolver
        v8::Persistent<v8::Context, v8::CopyablePersistentTraits<v8::Context>> persistentContext(isolate, context);
        v8::Persistent<v8::Promise::Resolver, v8::CopyablePersistentTraits<v8::Promise::Resolver>> persistentResolver(
                isolate, resolver);
        // 异步执行
        std::thread thread([](v8::Isolate *isolate,
                              v8::Persistent<v8::Context, v8::CopyablePersistentTraits<v8::Context>> persistentContext,
                              v8::Persistent<v8::Promise::Resolver, v8::CopyablePersistentTraits<v8::Promise::Resolver>> persistentResolver) -> void {
            v8::HandleScope handleScope(isolate);
            v8::Locker locker(isolate);
            v8::Local<v8::Context> context = persistentContext.Get(isolate);
            context->Enter();
            v8::Local<v8::Promise::Resolver> resolver = persistentResolver.Get(isolate);
            EXPECT_TRUE(resolver->Resolve(context, v8::Number::New(isolate, 1)).FromJust());

            EXPECT_EQ(global_promise, 0);
            isolate->PerformMicrotaskCheckpoint();
            EXPECT_EQ(global_promise, 1);
        }, isolate, persistentContext, persistentResolver);
        thread.join();
    }
}