#include "./base/environment.h"
#include "libplatform/libplatform.h"
#include "v8.h"
#include <algorithm>
#include <functional>
#include <string>

/**
 * 模块结构体
 */
class NodeModule {
public:
    // 模块名称
    std::string nodeModuleName;
    // 模块注册函数
    std::function<void(v8::Local<v8::Context> context,
                       v8::Local<v8::Object> module,
                       v8::Local<v8::Object> exports,
                       v8::Local<v8::Function> require)>
            nodeModuleRegisterFun;
    NodeModule *front = nullptr;
    // 模块的导出数据
    v8::MaybeLocal<v8::Object> exports = v8::MaybeLocal<v8::Object>();
};


// 保存着最后一个内建模块的
static NodeModule *buildInNodeModule = nullptr;

/**
 * 获取内建模块的函数，
 * @param info
 */
void internalBinding(const v8::FunctionCallbackInfo<v8::Value> &info) {
    v8::Isolate *isolate = v8::Isolate::GetCurrent();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    std::string moduleName(*v8::String::Utf8Value(isolate, info[0].As<v8::String>()));
    NodeModule *nodeModule = buildInNodeModule;

    // 遍历内建模块链表，根据模块名称获取模块
    while (nodeModule != nullptr) {
        if (nodeModule->nodeModuleName == moduleName) {
            // 如果模块已经加载过。直接从缓存取
            if (!nodeModule->exports.IsEmpty()) {
                info.GetReturnValue().Set(nodeModule->exports.ToLocalChecked());
            }
            v8::Local<v8::Object> module = v8::Object::New(isolate);
            v8::Local<v8::Object> exports = v8::Object::New(isolate);
            EXPECT_TRUE(module->Set(context, v8::String::NewFromUtf8Literal(isolate, "exports"), exports).FromJust());
            // 调用注册函数
            nodeModule->nodeModuleRegisterFun(context, module, exports, v8::Function::New(context, internalBinding).ToLocalChecked());
            // 导出 module对象的exports熟悉值
            info.GetReturnValue().Set(module->Get(context, v8::String::NewFromUtf8Literal(isolate, "exports")).ToLocalChecked());
            return;
        }
        nodeModule = nodeModule->front;
    }
    // 如果没有找到，返回null
    info.GetReturnValue().SetNull();
}

/**
 * 清空内建模块的链表
 */
void clearBuildInNodeModule() {
    while (buildInNodeModule != nullptr) {
        NodeModule *nodeModule = buildInNodeModule;
        buildInNodeModule = buildInNodeModule->front;
        delete nodeModule;
    }
}

/**
 * 内建模块的注册函数
 * @param nodeModuleName
 * @param nodeModuleRegisterFun
 */
void buildInNodeModuleRegister(std::string nodeModuleName, void (*nodeModuleRegisterFun)(v8::Local<v8::Context> context,
                                                                                         v8::Local<v8::Object> module,
                                                                                         v8::Local<v8::Object> exports,
                                                                                         v8::Local<v8::Function> require)) {
    NodeModule *nodeModule = new NodeModule();
    nodeModule->nodeModuleName = std::move(nodeModuleName);
    nodeModule->nodeModuleRegisterFun = nodeModuleRegisterFun;
    // 保存上一个nodeModule到当前的模块下
    nodeModule->front = buildInNodeModule;
    // 更新最后一个模块
    buildInNodeModule = nodeModule;
}

/**
 * 工具模块
 * @param context
 * @param module
 * @param exports
 * @param require
 */
void fooBuildInModule(v8::Local<v8::Context> context,
                      v8::Local<v8::Object> module,
                      v8::Local<v8::Object> exports,
                      v8::Local<v8::Function> require) {
    v8::Isolate *isolate = context->GetIsolate();
    v8::HandleScope handleScope(isolate);
    // 导出加法函数
    EXPECT_TRUE(exports->Set(context, v8::String::NewFromUtf8Literal(isolate, "add"), v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value> &info) -> void {
                                                                                          double first = info[0].As<v8::Number>()->Value();
                                                                                          double second = info[1].As<v8::Number>()->Value();
                                                                                          info.GetReturnValue().Set(first + second);
                                                                                      }).ToLocalChecked())
                        .FromJust());
    EXPECT_TRUE(exports->Set(context, v8::String::NewFromUtf8Literal(isolate, "result"), v8::Number::New(isolate, 1)).FromJust());
}

/**
 * 内建bar模块
 * @param context
 * @param module
 * @param exports
 * @param require
 */
void barBuildInModule(v8::Local<v8::Context> context,
                      v8::Local<v8::Object> module,
                      v8::Local<v8::Object> exports,
                      v8::Local<v8::Function> require) {
    v8::Isolate *isolate = context->GetIsolate();
    v8::HandleScope handleScope(isolate);
    // 乘法函数
    EXPECT_TRUE(exports->Set(context, v8::String::NewFromUtf8Literal(isolate, "mul"),
                             v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value> &info) -> void {
                                 double first = info[0].As<v8::Number>()->Value();
                                 double second = info[1].As<v8::Number>()->Value();
                                 info.GetReturnValue().Set(first * second);
                             }).ToLocalChecked())
                        .FromJust());

    v8::Local<v8::Value> argv[] = {v8::String::NewFromUtf8Literal(isolate, "foo")};
    // 调用foo模块
    v8::Local<v8::Object> fooModule = require->Call(context, context->Global(), 1, argv).ToLocalChecked().As<v8::Object>();
    // 把模块foo模块的属性值result设置到bar的属性params上
    EXPECT_TRUE(exports->Set(context, v8::String::NewFromUtf8Literal(isolate, "params"),
                             fooModule->Get(context, v8::String::NewFromUtf8Literal(isolate, "result")).ToLocalChecked())
                        .FromJust());
}


TEST_F(Environment, node_build_in_module_test) {
    v8::Isolate *isolate = getIsolate();
    v8::HandleScope handleScope(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    v8::Context::Scope context_scope(context);

    // 注册内建模块
    buildInNodeModuleRegister("foo", fooBuildInModule);
    buildInNodeModuleRegister("bar", barBuildInModule);

    v8::Local<v8::Object> process = v8::Object::New(isolate);
    // 设置 process.binding()函数
    EXPECT_TRUE(process->Set(context, v8::String::NewFromUtf8Literal(isolate, "binding"),
                             v8::Function::New(context, internalBinding).ToLocalChecked())
                        .FromJust());
    // 设置全局对象process
    EXPECT_TRUE(context->Global()->Set(context, v8::String::NewFromUtf8Literal(isolate, "process"), process).FromJust());
    const char *source = "const { params, mul } = process.binding('bar');\n"
                         "const { result, add } = process.binding('foo');\n"
                         "mul(add(params, result), result);";
    v8::Local<v8::Script> script = v8::Script::Compile(context, v8::String::NewFromUtf8(isolate, source).ToLocalChecked()).ToLocalChecked();
    v8::Local<v8::Value> result = script->Run(context).ToLocalChecked();
    EXPECT_TRUE(result.As<v8::Number>()->Value() == 2);
    // 清空所有的内建模块
    clearBuildInNodeModule();
}