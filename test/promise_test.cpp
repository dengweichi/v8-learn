
#include "./base/abstractAsyncTask.h"
#include "./base/environment.h"
#include "libplatform/libplatform.h"
#include <iostream>

static unsigned int global_count = 0;

class AsyncCallBackReadFileTask : public AbstractAsyncTask {
private:
    v8::Persistent<v8::Context> persistentContext;
    v8::Persistent<v8::Function> persistentCallBack;
    std::string relativePath;
    v8::Isolate *isolate;

public:
    AsyncCallBackReadFileTask(v8::Isolate *isolate, v8::Local<v8::Context> context, const char *relativePath, v8::Local<v8::Function> callBack) : AbstractAsyncTask() {
        this->isolate = isolate;
        this->persistentContext.Reset(isolate, context);
        this->persistentCallBack.Reset(isolate, callBack);
        this->relativePath = std::string(relativePath);
    }
    void run() override {
        v8::Locker locker(this->isolate);
        v8::Isolate::Scope scope(this->isolate);
        {
            v8::HandleScope handleScope(isolate);
            v8::Local<v8::Context> context = this->persistentContext.Get(this->isolate);
            v8::Context::Scope context_scope(context);
            v8::Local<v8::Function> callback = this->persistentCallBack.Get(this->isolate);

            // 获取文件的绝对路径
            std::string workDir = Environment::GetWorkingDirectory();
            std::string absolutePath = Environment::NormalizePath(this->relativePath, workDir);
            if (!Environment::IsAbsolutePath(absolutePath)) {
                int argc = 1;
                v8::Local<v8::Value> argv[] = {v8::Exception::Error(v8::String::NewFromUtf8Literal(isolate, "路径错误"))};
                callback->Call(context, v8::Null(isolate), argc, argv).ToLocalChecked();
            } else {
                std::string fileContent = Environment::ReadFile(absolutePath);
                v8::Local<v8::String> content;
                EXPECT_TRUE(v8::String::NewFromUtf8(isolate, fileContent.c_str()).ToLocal(&content));
                int argc = 2;
                v8::Local<v8::Value> argv[] = {v8::Null(isolate), content};
                callback->Call(context, v8::Null(isolate), argc, argv).ToLocalChecked();
            }
        }
    }
    ~AsyncCallBackReadFileTask() {
        this->persistentContext.Reset();
        this->persistentCallBack.Reset();
    }
};

bool async_callback_read_file_over = false;
TEST_F(Environment, async_callback_read_file) {
    global_count = 0;
    v8::Isolate *isolate = getIsolate();
    v8::HandleScope handleScope(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    v8::Context::Scope context_scope(context);
    v8::Local<v8::Function> readFile;
    /**
     * readFile("path", (error, data) => {
     *      if(!error) {
     *          console.log(data);
     *      }
     * })
     */
    EXPECT_TRUE(v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value> &info) -> void {
                    v8::Isolate *isolate = info.GetIsolate();
                    v8::HandleScope handleScope(isolate);
                    if (info.Length() != 2 || !info[0]->IsString() || !info[1]->IsFunction()) {
                        isolate->ThrowException(v8::String::NewFromUtf8Literal(isolate, "参数错误"));
                        return;
                    }
                    v8::Local<v8::Context> context = isolate->GetCurrentContext();
                    v8::Context::Scope context_scope(context);
                    std::unique_ptr<AsyncCallBackReadFileTask> asyncCallBackReadFileTask;
                    {

                        const char *relativePath = *v8::String::Utf8Value(isolate, info[0]);
                        std::cout << relativePath << "111111111" << std::endl;
                        v8::Local<v8::Function> callBack = info[1].As<v8::Function>();
                        asyncCallBackReadFileTask = std::make_unique<AsyncCallBackReadFileTask>(isolate, context, relativePath, callBack);
                    }
                    asyncCallBackReadFileTask->start();
                    asyncCallBackReadFileTask->join();
                }).ToLocal(&readFile));
    EXPECT_TRUE(context->Global()->Set(context, v8::String::NewFromUtf8Literal(isolate, "readFile"), readFile).FromJust());

    v8::Local<v8::Function> getValue = v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value> &info) -> void {
                                            async_callback_read_file_over = true;
                                       }).ToLocalChecked();
    EXPECT_TRUE(context->Global()->Set(context, v8::String::NewFromUtf8Literal(isolate, "getValue"), getValue).FromJust());

    const char *source = "readFile('./source/test_read_file.json', (error, data) => {\n"
                         "      if(!error) {\n"
                         "          getValue(data);\n"
                         "      }\n"
                         "      getValue(null);\n"
                         "});\n";
    v8::Local<v8::Script> script =
            v8::Script::Compile(context, v8::String::NewFromUtf8(isolate, source).ToLocalChecked()).ToLocalChecked();
    script->Run(context).ToLocalChecked();
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
