
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
                                  v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value> &info) -> void {

                                  }).ToLocalChecked())
                            .ToLocal(&promise));

        v8::Global<v8::Context> globalContext(isolate, context);
        v8::Global<v8::Promise::Resolver> globalResolver(isolate, resolver);

        std::thread thread([](v8::Isolate *isolate,v8::Global<v8::Context> context, v8::Global<v8::Promise::Resolver> resolver) -> void {
          v8::Isolate::Scope scope(isolate);
          v8::HandleScope handleScope(isolate);
          v8::Locker locker(isolate);
        },isolate, globalContext, globalResolver);

        thread.join();

    }
}