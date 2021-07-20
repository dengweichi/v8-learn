#include "../environment.h"
#include <iostream>
#include <thread>
#include "libplatform/libplatform.h"


class ConsoleExtension: public v8::Extension{
private:
    // 在全局对象中注入 console 对象。
    constexpr static const char* source = "if(!console) {\n"
                                          " var console = {};\n"
                                          "};\n"
                                          "";
public:
    ConsoleExtension(): v8::Extension("v8/Console", source){}
};

class LogExtension: public v8::Extension{
private:
    /**
     * console 对象由 LogExtension的依赖 ConsoleExtension创建提供
     */
    constexpr static const char* source = "console.log = function (data){\n"
                                             "  native function log();\n"
                                             "};\n"
                                             "native function print();\n";
    /**
     *  输出到控制台
     * @param info
     */
    static void cout (const v8::FunctionCallbackInfo<v8::Value> &info) {
        v8::Isolate* isolate = info.GetIsolate();
        v8::HandleScope handleScope(isolate);
        v8::Local<v8::Context> context = isolate->GetCurrentContext();
        if(info[0]->IsNull() || info[0]->IsFunction()) {
            return;
        }
        if (info[0]->IsObject()) {
            std::cout << *v8::String::Utf8Value(isolate, v8::JSON::Stringify(context, info[0]).ToLocalChecked()) << std::endl;
        } else {
            std::cout<<  *v8::String::Utf8Value(isolate, info[0].As<v8::String>()) << std::endl;
        }
    }
public:
    LogExtension(int depCount, const char** deps): v8::Extension("v8/Log", source, depCount, deps){}

    /**
     * 通过不同的参数名称 返回不同的函数模板。
     * @param isolate
     * @param name
     * @return
     */
    v8::Local<v8::FunctionTemplate> GetNativeFunctionTemplate(
            v8::Isolate* isolate, v8::Local<v8::String> name) {
        if (name->StrictEquals(v8::String::NewFromUtf8Literal(isolate, "print"))) {
            return v8::FunctionTemplate::New(isolate, print);
        } else if (name->StrictEquals(v8::String::NewFromUtf8Literal(isolate, "log"))) {
            return  v8::FunctionTemplate::New(isolate, log);
        } else {
            return v8::FunctionTemplate::New(isolate);
        };
    }


    static void print(const v8::FunctionCallbackInfo<v8::Value> &info) {
        cout(info);
    }
    static void log(const v8::FunctionCallbackInfo<v8::Value> &info) {
        cout(info);
    }

};

TEST_F(Environment, extension_test) {
    v8::RegisterExtension(std::make_unique<ConsoleExtension>());
    const char* deps[] = { "v8/Console"  };
    v8::RegisterExtension(std::make_unique<LogExtension>(1, deps));
    v8::Isolate* isolate = getIsolate();
    v8::Locker locker(isolate);
    v8::HandleScope handleScope(isolate);

    // 配置生效的扩展名，默认所以扩展在所以上下文不生效，需要配置给创建上下。
    const char* extensionNames [] = {
            "v8/Log"
    };
    // 扩展配置
    v8::ExtensionConfiguration extensionConfiguration(1, extensionNames );
    v8::Local<v8::Context> context = v8::Context::New(isolate, &extensionConfiguration);
    EXPECT_FALSE(context.IsEmpty());
    EXPECT_TRUE(context->Global()->Get(context, v8::String::NewFromUtf8Literal(isolate, "console")).ToLocalChecked()->IsObject());
    EXPECT_TRUE(context->Global()->Get(context, v8::String::NewFromUtf8Literal(isolate, "console"))
    .ToLocalChecked().As<v8::Object>()->Get(context, v8::String::NewFromUtf8Literal(isolate, "log")).ToLocalChecked()->IsFunction());
}
TEST_F(Environment, extension_auto_test) {
    v8::RegisterExtension(std::make_unique<ConsoleExtension>());
    const char* deps[] = { "v8/Console"  };
    std::unique_ptr<LogExtension> logExtension = std::make_unique<LogExtension>(1, deps);
    // 设置为自带启动，该扩展在每个上下文中生效，不在需要创建上下文时指定
    logExtension->set_auto_enable(true);
    v8::RegisterExtension(std::move(logExtension));
    v8::Isolate* isolate = getIsolate();
    v8::Locker locker(isolate);
    v8::HandleScope handleScope(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    EXPECT_FALSE(context.IsEmpty());
    EXPECT_TRUE(context->Global()->Get(context, v8::String::NewFromUtf8Literal(isolate, "console")).ToLocalChecked()->IsObject());
    EXPECT_TRUE(context->Global()->Get(context, v8::String::NewFromUtf8Literal(isolate, "console"))
                        .ToLocalChecked().As<v8::Object>()->Get(context, v8::String::NewFromUtf8Literal(isolate, "log")).ToLocalChecked()->IsFunction());
}
