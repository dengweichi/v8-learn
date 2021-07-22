#include "../environment.h"
#include "libplatform/libplatform.h"
#include <iostream>

TEST_F(Environment, module_classic_test) {
    v8::Isolate *isolate = getIsolate();
    v8::HandleScope handleScope(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    v8::Context::Scope context_scope(context);
    const char *source1 = "function add (first, second) {\n"
                          "  return first + second;\n"
                          "};\n";
    // 编译源代码
    v8::Local<v8::Script> script1 = v8::Script::Compile(context, v8::String::NewFromUtf8(isolate, source1).ToLocalChecked()).ToLocalChecked();
    // 执行脚本
    script1->Run(context).ToLocalChecked();

    // 执行第二个编译的脚本，可以使用第一source1 定义的函数add
    const char *source2 = "add(1,2);";
    v8::Local<v8::Script> script2 = v8::Script::Compile(context, v8::String::NewFromUtf8(isolate, source2).ToLocalChecked()).ToLocalChecked();
    v8::Local<v8::Value> result = script2->Run(context).ToLocalChecked();
    EXPECT_TRUE(result.As<v8::Number>()->Value() == 3);
}


void module(v8::Local<v8::Object> exports) {
    v8::Isolate *isolate = v8::Isolate::GetCurrent();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    // 设置 add函数
    exports->Set(context, v8::String::NewFromUtf8Literal(isolate, "add"),
                 v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value> &info) -> void {
                     v8::Isolate *isolate = info.GetIsolate();
                     // 参数校验
                     if (info.Length() != 2 || !info[0]->IsNumber() && !info[1]->IsNumber()) {
                         isolate->ThrowException(v8::String::NewFromUtf8Literal(isolate, "参数错误"));
                         return;
                     }
                     double first = info[0].As<v8::Number>()->Value();
                     double second = info[1].As<v8::Number>()->Value();
                     double result = first + second;
                     info.GetReturnValue().Set(v8::Number::New(isolate, result));
                 }).ToLocalChecked())
            .FromJust();
    // 设置属性result = 1;
    exports->Set(context, v8::String::NewFromUtf8Literal(isolate, "result"), v8::Number::New(isolate, 1)).FromJust();
}

TEST_F(Environment, module_commonjs_test) {
    v8::Isolate *isolate = getIsolate();
    v8::HandleScope handleScope(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    v8::Context::Scope context_scope(context);
    v8::Local<v8::Object> exports = v8::Object::New(isolate);
    module(exports);
    // 获取moudle函数执行暴露的变量和函数
    v8::Local<v8::Function> add = exports->Get(context, v8::String::NewFromUtf8Literal(isolate, "add")).ToLocalChecked().As<v8::Function>();
    v8::Local<v8::Number> result = exports->Get(context, v8::String::NewFromUtf8Literal(isolate, "result")).ToLocalChecked().As<v8::Number>();
    v8::Local<v8::Value> argv[] = {v8::Number::New(isolate, 1), result};
    EXPECT_TRUE(add->Call(context, context->Global(), 2, argv).ToLocalChecked().As<v8::Number>()->Value() == 2);
};

v8::MaybeLocal<v8::Module> resolveModule(v8::Local<v8::Context> context, v8::Local<v8::String> specifier,
                                         v8::Local<v8::FixedArray> import_assertions, v8::Local<v8::Module> referrer) {
    v8::Isolate *isolate = context->GetIsolate();

    // 判断是请求加载那个模块
    if (specifier->StrictEquals(
                v8::String::NewFromUtf8Literal(isolate, "foo.js"))) {
        const char *scriptSource = " export const add = function (first, second) {\n"
                                   "    return first + second;\n"
                                   "};\n";
        v8::ScriptOrigin origin(v8::String::NewFromUtf8Literal(isolate, "foo.js"), 0, 0, false, -1, v8::Local<v8::Value>(), false, false, true);
        v8::ScriptCompiler::Source source(v8::String::NewFromUtf8(isolate, scriptSource).ToLocalChecked(), origin);
        v8::Local<v8::Module> module = v8::ScriptCompiler::CompileModule(isolate, &source).ToLocalChecked();
        module->InstantiateModule(context, resolveModule).FromJust();
        return module;
    } else {
        const char *scriptSource = " export const result = 1;";
        v8::ScriptOrigin origin(v8::String::NewFromUtf8Literal(isolate, "bar.js"), 0, 0, false, -1, v8::Local<v8::Value>(), false, false, true);
        v8::ScriptCompiler::Source source(v8::String::NewFromUtf8(isolate, scriptSource).ToLocalChecked(), origin);
        v8::Local<v8::Module> module = v8::ScriptCompiler::CompileModule(isolate, &source).ToLocalChecked();
        module->InstantiateModule(context, resolveModule).FromJust();
        return module;
    }
}


TEST_F(Environment, module_esmodule_test) {
    v8::Isolate *isolate = getIsolate();
    v8::HandleScope handleScope(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    v8::Context::Scope context_scope(context);
    const char *scriptSource = "import { add } from 'foo.js';\n"
                               "import { result } from 'bar.js';\n"
                               "add(result, 1);\n";
    // 构建编译条件
    v8::ScriptOrigin origin(v8::String::NewFromUtf8Literal(isolate, "main.js"), 1, 0, false, -1, v8::Local<v8::Value>(), false, false, true);
    // 构建编译source
    v8::ScriptCompiler::Source source(v8::String::NewFromUtf8(isolate, scriptSource).ToLocalChecked(), origin);
    // 编译模块
    v8::Local<v8::Module> module = v8::ScriptCompiler::CompileModule(isolate, &source).ToLocalChecked();
    // 实例化模块
    module->InstantiateModule(context, resolveModule).FromJust();
    // 执行模块
    module->Evaluate(context).ToLocalChecked();
}
