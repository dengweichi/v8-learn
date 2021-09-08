
#include "./base/abstractAsyncTask.h"
#include "./base/environment.h"
#include "libplatform/libplatform.h"
#include <iostream>
#include <thread>
static unsigned int global_count = 0;

TEST_F(Environment, async_callback_read_file) {
    global_count = 0;
    v8::Isolate *isolate = getIsolate();
    {
        v8::HandleScope handleScope(isolate);
        v8::Local<v8::Context> context = v8::Context::New(isolate);
        v8::Context::Scope context_scope(context);
        v8::Local<v8::Function> function = v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value> &info) -> void {
            global_count++;
        }).ToLocalChecked();

        v8::Global<v8::Context> persistentContext(isolate, context);
        v8::Global<v8::Function> persistentFunction(isolate, function);

        std::thread thread([](v8::Isolate* isolate, v8::Global<v8::Context> persistentContext,
                              v8::Global<v8::Function> persistentFunction) -> void {
            v8::Locker locker(isolate);
            v8::Isolate::Scope scope(isolate);
            v8::HandleScope handleScope(isolate);
            v8::Local<v8::Context> context = persistentContext.Get(isolate);
            v8::Local<v8::Function> callBack = persistentFunction.Get(isolate);
            callBack->Call(context,v8::Null(isolate), 0 , {}).ToLocalChecked();
        },isolate, std::move(persistentContext), std::move(persistentFunction));
        // std::this_thread::sleep_for(std::chrono::seconds(3));
        thread.join();
        EXPECT_EQ(global_count, 1);
    }
}


TEST_F(Environment, promise_resolver) {
    global_count = 0;
    v8::Isolate *isolate = getIsolate();
    // 设置微任务队列策略
    isolate->SetMicrotasksPolicy(v8::MicrotasksPolicy::kExplicit);
    v8::HandleScope handleScope(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    v8::Context::Scope context_scope(context);
    v8::Local<v8::Promise::Resolver> resolver;
    EXPECT_TRUE(v8::Promise::Resolver::New(context).ToLocal(&resolver));
    v8::Local<v8::Promise> promise = resolver->GetPromise();
    EXPECT_TRUE(promise->Then(context, v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value> &info) -> void {
                                           global_count++;
                                           EXPECT_EQ(info.Length(), 1);
                                           EXPECT_TRUE(info[0]->IsNumber());
                                           EXPECT_EQ(info[0].As<v8::Number>()->Value(), 1);
                                       }).ToLocalChecked())
                        .ToLocal(&promise));
    EXPECT_TRUE(resolver->Resolve(context, v8::Number::New(isolate, 1)).FromJust());

    EXPECT_EQ(global_count, 0);
    // 清空微任务队列
    isolate->PerformMicrotaskCheckpoint();
    EXPECT_EQ(global_count, 1);
}

TEST_F(Environment, promise_reject) {
    global_count = 0;
    v8::Isolate *isolate = getIsolate();
    // 设置微任务队列策略
    isolate->SetMicrotasksPolicy(v8::MicrotasksPolicy::kExplicit);
    v8::HandleScope handleScope(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    v8::Context::Scope context_scope(context);
    v8::Local<v8::Promise::Resolver> resolver;
    EXPECT_TRUE(v8::Promise::Resolver::New(context).ToLocal(&resolver));
    v8::Local<v8::Promise> promise = resolver->GetPromise();

    EXPECT_TRUE(promise->Then(context,
                              v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value> &info) -> void {
                                  global_count++;
                              }).ToLocalChecked(),
                              v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value> &info) -> void {
                                  global_count += 2;
                                  v8::Isolate *isolate = info.GetIsolate();
                                  isolate->ThrowException(v8::String::NewFromUtf8Literal(isolate, "error"));
                              }).ToLocalChecked())
                        .ToLocal(&promise));

    EXPECT_TRUE(resolver->Reject(context, v8::Null(isolate)).FromJust());

    EXPECT_EQ(global_count, 0);
    isolate->PerformMicrotaskCheckpoint();
    EXPECT_EQ(global_count, 2);

    EXPECT_TRUE(promise->Then(context,
                              v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value> &info) -> void {
                                  global_count++;
                              }).ToLocalChecked(),
                              v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value> &info) -> void {
                                  global_count += 2;
                                  v8::Isolate *isolate = info.GetIsolate();
                                  isolate->ThrowException(v8::String::NewFromUtf8Literal(isolate, "error"));
                              }).ToLocalChecked())
                        .ToLocal(&promise));

    EXPECT_EQ(global_count, 2);
    isolate->PerformMicrotaskCheckpoint();
    EXPECT_EQ(global_count, 4);

    EXPECT_TRUE(promise->Catch(context, v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value> &info) -> void {
                                            global_count += 2;
                                        }).ToLocalChecked())
                        .ToLocal(&promise));

    EXPECT_EQ(global_count, 4);
    isolate->PerformMicrotaskCheckpoint();
    EXPECT_EQ(global_count, 6);
}


class PromiseAsyncTask : public AbstractAsyncTask {
private:
    v8::Persistent<v8::Context> persistentContext;
    v8::Persistent<v8::Promise::Resolver> persistentResolver;
    v8::Isolate *isolate;

public:
    PromiseAsyncTask(v8::Isolate *isolate, v8::Local<v8::Context> context, v8::Local<v8::Promise::Resolver> resolver) : AbstractAsyncTask() {
        this->isolate = isolate;
        this->persistentContext.Reset(isolate, context);
        this->persistentResolver.Reset(isolate, resolver);
    }
    void run() override {
        v8::Locker locker(isolate);
        v8::Isolate::Scope scope(isolate);
        {
            v8::HandleScope handleScope(isolate);
            v8::Local<v8::Context> context = persistentContext.Get(isolate);
            v8::Context::Scope context_scope(context);
            v8::Local<v8::Promise::Resolver> resolver = persistentResolver.Get(isolate);
            EXPECT_TRUE(resolver->Resolve(context, v8::Number::New(isolate, 1)).FromJust());
            EXPECT_EQ(global_count, 0);
            isolate->PerformMicrotaskCheckpoint();
            EXPECT_EQ(global_count, 1);
        }
    }
    ~PromiseAsyncTask() {
        persistentResolver.Reset();
        persistentContext.Reset();
    }
};

TEST_F(Environment, promise_async_resolver) {
    global_count = 0;
    v8::Isolate *isolate = getIsolate();
    // 设置微任务队列策略为明确执行得
    isolate->SetMicrotasksPolicy(v8::MicrotasksPolicy::kExplicit);
    std::unique_ptr<PromiseAsyncTask> promiseAsyncTask;
    {
        v8::HandleScope handleScope(isolate);
        v8::Local<v8::Context> context = v8::Context::New(isolate);
        v8::Context::Scope context_scope(context);
        v8::Local<v8::Promise::Resolver> resolver;
        EXPECT_TRUE(v8::Promise::Resolver::New(context).ToLocal(&resolver));
        v8::Local<v8::Promise> promise = resolver->GetPromise();

        // 添加promise 回调
        EXPECT_TRUE(promise->Then(context, v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value> &info) -> void {
                                               global_count++;
                                               EXPECT_EQ(info.Length(), 1);
                                               EXPECT_TRUE(info[0]->IsNumber());
                                               EXPECT_EQ(info[0].As<v8::Number>()->Value(), 1);
                                           }).ToLocalChecked())
                            .ToLocal(&promise));
        // 异步执行
        promiseAsyncTask = std::make_unique<PromiseAsyncTask>(isolate, context, resolver);
    }
    promiseAsyncTask->start();
    promiseAsyncTask->join();
    EXPECT_EQ(global_count, 1);
}
