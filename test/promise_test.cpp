
#include "../abstractAsyncTask.h"
#include "../environment.h"
#include "libplatform/libplatform.h"
#include <iostream>
#include <string>

static unsigned int global_count = 0;
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
        this->start();
    }
    void run() override {
        v8::Locker locker(isolate);
        isolate->Enter();
        {
            v8::HandleScope handleScope(isolate);
            v8::Local<v8::Context> context = v8::Local<v8::Context>::New(isolate, persistentContext);
            v8::Context::Scope context_scope(context);
            v8::Local<v8::Promise::Resolver> resolver = v8::Local<v8::Promise::Resolver>::New(isolate, persistentResolver);
            EXPECT_TRUE(resolver->Resolve(context, v8::Number::New(isolate, 1)).FromJust());
            EXPECT_EQ(global_count, 0);
            isolate->PerformMicrotaskCheckpoint();
            EXPECT_EQ(global_count, 1);
        }
        isolate->Exit();
    }
};

TEST_F(Environment, promise_then_resolver) {
    global_count = 0;
    v8::Isolate *isolate = getIsolate();
    isolate->Enter();
    {
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
                                                        global_count++;
                                                        EXPECT_EQ(info.Length(), 1);
                                                        EXPECT_TRUE(info[0]->IsNumber());
                                                        EXPECT_EQ(info[0].As<v8::Number>()->Value(), 1);
                                                    })
                                          .ToLocalChecked())
                            .ToLocal(&promise));
        // 异步执行
        PromiseAsyncTask*  promiseAsyncTask = new PromiseAsyncTask(isolate, context, resolver);
        promiseAsyncTask->join();
        delete promiseAsyncTask;
    }
    isolate->Exit();
}