
#include "../environment.h"
#include <iostream>
#include <thread>
#include <string>
#include "libplatform/libplatform.h"

extern v8::Platform* g_default_platform;

bool IsAligned(unsigned long long value, unsigned long long alignment) {
  return (value & (alignment - 1)) == 0;
}
/**
 * 测试隔离实例地址空间4GB 对齐
 */
TEST(isolate_test, isolate_4g_address_aligned) {
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
}

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
    context2->Global()->Set(
        context2, v8::String::NewFromUtf8Literal(isolate2, "globalGetFun"),
        function2);
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
      v8::Locker locker(isolate);
      v8::Isolate::Scope scope(isolate);
      v8::HandleScope handleScope(isolate);
      v8::Local<v8::Context> context = v8::Context::New(isolate);
      v8::Context::Scope context_scope(context);
      const char* scriptSource = "function fun() {"
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
    std::thread thread( [](v8::Isolate *isolate, v8::Persistent<v8::Context, v8::CopyablePersistentTraits<v8::Context>> context) -> void {
      v8::Locker locker(isolate);
      v8::Isolate::Scope scope(isolate);
      v8::HandleScope handleScope(isolate);
      v8::Local<v8::Context> localContext = v8::Local<v8::Context>::New(isolate, context);
      v8::Context::Scope context_scope(localContext);
      const char* scriptSource = "fun()";
      v8::Local<v8::String> source = v8::String::NewFromUtf8(isolate, scriptSource).ToLocalChecked();
      v8::Local<v8::Script> script = v8::Script::Compile(localContext, source).ToLocalChecked();
      v8::Local<v8::Value> result = script->Run(localContext).ToLocalChecked();
      EXPECT_EQ(result.As<v8::Number>()->Value(), 1);
    }, isolate, globalContext);
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
    std::thread thread([](v8:: Isolate* isolate1) -> void {
      v8::Locker locker(isolate1);
      EXPECT_TRUE(v8::Isolate::GetCurrent() == nullptr);
      v8::Isolate::Scope scope(isolate1);
      EXPECT_TRUE(v8::Isolate::GetCurrent() == isolate1);
    }, isolate1);
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

TEST_F(Environment, data) {
  v8::Isolate* isolate = getIsolate();
  uint32_t slot = isolate->GetNumberOfDataSlots();
  std::string* data = new std::string("string");
  isolate->SetData(slot, data);
  std::thread thread([](v8::Isolate* isolate, uint32_t slot) -> void {
    auto* data = static_cast<std::string*>(isolate->GetData(slot));
    EXPECT_TRUE(*data == "string");
  }, isolate, slot);
  thread.join();
  std::string* slotDta = static_cast<std::string*>(isolate->GetData(slot));
  EXPECT_TRUE(slotDta == data);
  delete data;
}

TEST_F(Environment, dynamicallyImport) {
  v8::Isolate *isolate = getIsolate();
  v8::Locker locker(isolate);
  {
    // 请求  import('xxx.js')执行的回调，由嵌入式应用提供回调
    isolate->SetHostImportModuleDynamicallyCallback(
        [](v8::Local<v8::Context> context,
           v8::Local<v8::ScriptOrModule> referrer,
           v8::Local<v8::String> specifier,
           v8::Local<v8::FixedArray> import_assertions)
            -> v8::MaybeLocal<v8::Promise> {
          v8::Isolate *isolate = context->GetIsolate();
          v8::Locker locker(isolate);
          v8::MaybeLocal<v8::Promise::Resolver> maybe_resolver =
              v8::Promise::Resolver::New(context);
          v8::Local<v8::Promise::Resolver> resolver;
          if (maybe_resolver.ToLocal(&resolver)) {
            const char *scriptSource = "export const fun = function () {"
                                       "  return 1;"
                                       "}";
            v8::ScriptOrigin origin(
                isolate, specifier,
                0, 0, false, -1, v8::Local<v8::Value>(), false, false, true);
            v8::ScriptCompiler::Source source(
                v8::String::NewFromUtf8(isolate, scriptSource).ToLocalChecked(),
                origin);
            v8::Local<v8::Module> module =
                v8::ScriptCompiler::CompileModule(
                    isolate, &source,
                    v8::ScriptCompiler::CompileOptions::kNoCompileOptions,
                    v8::ScriptCompiler::NoCacheReason::kNoCacheNoReason)
                    .ToLocalChecked();
            module->InstantiateModule(
                context,
                [](v8::Local<v8::Context> context, v8::Local<v8::String> specifier,
                   v8::Local<v8::FixedArray> import_assertions,
                   v8::Local<v8::Module> referrer) -> v8::MaybeLocal<v8::Module> {
                  return v8::MaybeLocal<v8::Module>();
                });

            resolver->Resolve(context, module->GetModuleNamespace());
            return  resolver->GetPromise();
          }
          return v8::MaybeLocal<v8::Promise>();
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
    const char *scriptSource = "import('./second.js').then(( { fun } )=> {"
                               "  const result = fun(); "
                               "  print(result); "
                               "})";
    // 全局注册打印函数
    context->Global()->Set(
        context, v8::String::NewFromUtf8(isolate, "print").ToLocalChecked(),
        v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value>& info)->void {
          v8::Isolate* isolate = info.GetIsolate();
          v8::HandleScope handleScope(isolate);
          v8::Local<v8::Context> context = isolate->GetCurrentContext();
          EXPECT_TRUE(info[0].As<v8::Number>()->Value() == 1);
        }).ToLocalChecked());
    // 脚本元信息
    v8::ScriptOrigin origin(
        isolate, v8::String::NewFromUtf8(isolate, "index.js").ToLocalChecked(),
        0, 0, false, -1, v8::Local<v8::Value>(), false, false, true);
    // 脚本
    v8::ScriptCompiler::Source source(
        v8::String::NewFromUtf8(isolate, scriptSource).ToLocalChecked(),
        origin);
    // 把脚本以 es-module去编译
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
        });
    // 执行模块
    module->Evaluate(context).ToLocalChecked();
  }
}

TEST_F(Environment, context) {
  v8::Isolate* isolate = getIsolate();
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
  v8::Isolate* isolate = getIsolate();
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
  v8::Isolate* isolate = getIsolate();
  v8::Locker locker(isolate);
  v8::HandleScope handleScope(isolate);
  v8::Local<v8::Context> context = v8::Context::New(isolate);
  isolate->EnqueueMicrotask([](void* data) -> void {
    (*static_cast<int*>(data))++;
  }, &status);
  bool result = true;
  do {
    result = v8::platform::PumpMessageLoop(g_default_platform, isolate, v8::platform::MessageLoopBehavior::kDoNotWait);
  } while (result);
  EXPECT_TRUE(status == 1);
}




















