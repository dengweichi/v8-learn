#include "../environment.h"
#include "libplatform/libplatform.h"
#include <bitset>
#include <cmath>
#include <iostream>
#include <string>
#include <thread>

namespace v10 {
    typedef uint64_t Address;
    /**
   * 堆对象
   */
    class HeapObject {
    private:
        std::vector<Address> handleList;
        bool isDestroy = false;

    protected:
        HeapObject() {}

    public:
        size_t handleSize() { return handleList.size(); };
        bool addHandle(Address address) {
            // 防止重复
            for (std::vector<Address>::iterator it = handleList.begin();
                 it != handleList.end(); it++) {
                if (*it == address) {
                    return false;
                }
            }
            handleList.push_back(address);
            return true;
        };
        bool removeHandle(Address address) {
            for (std::vector<Address>::iterator it = handleList.begin();
                 it != handleList.end(); it++) {
                if (*it == address) {
                    handleList.erase(it);
                    return true;
                }
            }
            return false;
        };
    };

    /**
  * 隔离实例
  */
    class Isolate {
    private:
        std::vector<HeapObject *> heapObjectList;

    public:
        // 添加堆对象
        bool addHeapObjectList(HeapObject *heapObject) {
            for (std::vector<HeapObject *>::iterator it = heapObjectList.begin(); it != heapObjectList.end(); it++) {
                if (*it == heapObject) {
                    return false;
                }
            }
            heapObjectList.push_back(heapObject);
            return true;
        }
        /**
     * 垃圾回收
     * 当 HeapObject的句柄的数量为0 执行垃圾回收。
     */
        void garbageCollection() {

            for (std::vector<HeapObject *>::iterator it = heapObjectList.begin(); it != heapObjectList.end();) {
                HeapObject *heapObject = (*it);
                if (heapObject->handleSize() == 0) {
                    heapObjectList.erase(it);
                    delete heapObject;
                } else {
                    it++;
                }
            }
        };
        size_t heapObjectSize() {
            return heapObjectList.size();
        }
    };
    /**
   * 句柄
   */
    template<class T>
    class Handle {
    private:
        T *_ptr = nullptr;
        Isolate *_isolate = nullptr;
        void recycle() {
            // 防止重复销毁
            if (_ptr == nullptr) {
                return;
            }
            HeapObject *heapObject = dynamic_cast<HeapObject *>(_ptr);
            // 当执行删除当前句柄后。
            heapObject->removeHandle(reinterpret_cast<Address>(this));
            _ptr = nullptr;
        }

    public:
        bool operator==(Handle &that) {
            return _ptr == that._ptr;
        };
        // 复制构造函数
        template<class S>
        Handle(Handle<S> &handle) {
            handle._ptr = _ptr;
            HeapObject *heapObject = dynamic_cast<HeapObject *>(_ptr);
            heapObject->addHandle(reinterpret_cast<Address>(&handle));
            _isolate->addHeapObjectList(*handle);
        }

        bool isEmpty() {
            return _ptr == nullptr;
        }
        void clear() {
            recycle();
        }
        T *operator->() const { return _ptr; }

        T *operator*() const { return _ptr; }
        ~Handle() {
            recycle();
        }
        explicit Handle(Isolate *isolate, T *ptr) : _ptr(ptr), _isolate(isolate) {
            HeapObject *heapObject = dynamic_cast<HeapObject *>(_ptr);
            _isolate->addHeapObjectList(heapObject);
            heapObject->addHandle(reinterpret_cast<Address>(this));
        }
        explicit Handle(Isolate *isolate, Handle<T> &handle) : _isolate(isolate) {
            _ptr = handle._ptr;
            HeapObject *heapObject = dynamic_cast<HeapObject *>(_ptr);
            heapObject->addHandle(reinterpret_cast<Address>(this));
        }
        friend class HeapObject;
    };


    class String : public HeapObject {
    private:
        String(const char *str) {
            _str = str;
        };
        std::string _str;

    public:
        static Handle<String> New(Isolate *isolate, const char *str) {
            return Handle<String>(isolate, new String(str));
        }
        std::string getString() {
            return _str;
        }
    };
}// namespace v10
TEST(handle_scope_test, custom_handle_test) {
    v10::Isolate *isolate = new v10::Isolate();
    {
        EXPECT_TRUE(isolate->heapObjectSize() == 0);
        v10::Handle<v10::String> handleString1 = v10::String::New(isolate, "string");
        EXPECT_TRUE(handleString1->handleSize() == 1);

        EXPECT_TRUE(isolate->heapObjectSize() == 1);
        v10::Handle<v10::String> handleString2(isolate, handleString1);
        EXPECT_TRUE(isolate->heapObjectSize() == 1);
        EXPECT_TRUE(handleString1->handleSize() == 2);
    }
    EXPECT_TRUE(isolate->heapObjectSize() == 1);
    // 执行垃圾回收
    isolate->garbageCollection();
    EXPECT_TRUE(isolate->heapObjectSize() == 0);
    delete isolate;
}

v8::Local<v8::Value> nativeAddWidthHandleScope(v8::Local<v8::Number> first, v8::Local<v8::Number> second) {
    v8::Isolate *isolate = v8::Isolate::GetCurrent();
    v8::HandleScope handleScope(isolate);
    float firstNum = first->Value();
    float secondNum = second->Value();
    v8::Local<v8::Number> result = v8::Number::New(isolate, firstNum + secondNum);
    return result;
}

v8::Local<v8::Value> nativeAddWidthEscapableHandleScope(v8::Local<v8::Number> first, v8::Local<v8::Number> second) {
    v8::Isolate *isolate = v8::Isolate::GetCurrent();
    // 逃逸句柄作用域
    v8::EscapableHandleScope escapableHandleScope(isolate);
    float firstNum = first->Value();
    float secondNum = second->Value();
    v8::Local<v8::Number> result = v8::Number::New(isolate, firstNum + secondNum);
    return escapableHandleScope.Escape(result);
}

v8::Local<v8::Number> nativeAdd(v8::Local<v8::Number> first, v8::Local<v8::Number> second) {
    v8::Isolate *isolate = v8::Isolate::GetCurrent();
    float firstNum = first->Value();
    float secondNum = second->Value();
    v8::Local<v8::Number> result = v8::Number::New(isolate, firstNum + secondNum);
    return result;
}

void globalAddWithHandleScope(const v8::FunctionCallbackInfo<v8::Value> &info) {
    v8::Isolate *isolate = info.GetIsolate();
    v8::HandleScope handleScope(isolate);
    double firstNum = info[0].As<v8::Number>()->Value();
    double secondNum = info[1].As<v8::Number>()->Value();
    v8::Local<v8::Number> result = v8::Number::New(isolate, firstNum + secondNum);
    info.GetReturnValue().Set(result);
}

void globalAddWithEscapableHandleScope(const v8::FunctionCallbackInfo<v8::Value> &info) {
    v8::Isolate *isolate = info.GetIsolate();
    // 逃逸句柄作用域
    v8::EscapableHandleScope escapableHandleScope(isolate);
    double firstNum = info[0].As<v8::Number>()->Value();
    double secondNum = info[1].As<v8::Number>()->Value();
    v8::Local<v8::Number> result = v8::Number::New(isolate, firstNum + secondNum);
    info.GetReturnValue().Set(escapableHandleScope.Escape(result));
}

void globalAdd(const v8::FunctionCallbackInfo<v8::Value> &info) {
    v8::Isolate *isolate = info.GetIsolate();
    double firstNum = info[0].As<v8::Number>()->Value();
    double secondNum = info[1].As<v8::Number>()->Value();
    v8::Local<v8::Number> result = v8::Number::New(isolate, firstNum + secondNum);
    info.GetReturnValue().Set(result);
}


TEST_F(Environment, handle_scope_test_new) {
    v8::Isolate *isolate = getIsolate();
    v8::HandleScope handleScope(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    // 设置全局函数
    EXPECT_TRUE(context->Global()->Set(context, v8::String::NewFromUtf8Literal(isolate, "globalAddWithHandleScope"), v8::Function::New(context, globalAddWithHandleScope).ToLocalChecked()).FromJust());
    EXPECT_TRUE(context->Global()->Set(context, v8::String::NewFromUtf8Literal(isolate, "globalAddWithEscapableHandleScope"), v8::Function::New(context, globalAddWithEscapableHandleScope).ToLocalChecked()).FromJust());
    EXPECT_TRUE(context->Global()->Set(context, v8::String::NewFromUtf8Literal(isolate, "globalAdd"), v8::Function::New(context, globalAdd).ToLocalChecked()).FromJust());
    v8::Local<v8::Function> globalAdd = context->Global()->Get(context, v8::String::NewFromUtf8Literal(isolate, "globalAdd")).ToLocalChecked().As<v8::Function>();
    v8::Local<v8::Function> globalAddWithHandleScope = context->Global()->Get(context, v8::String::NewFromUtf8Literal(isolate, "globalAddWithHandleScope")).ToLocalChecked().As<v8::Function>();
    v8::Local<v8::Function> globalAddWithEscapableHandleScope = context->Global()->Get(context, v8::String::NewFromUtf8Literal(isolate, "globalAddWithEscapableHandleScope")).ToLocalChecked().As<v8::Function>();

    v8::Local<v8::Number> first = v8::Number::New(isolate, 1);
    v8::Local<v8::Number> second = v8::Number::New(isolate, 2);
    v8::Local<v8::Value> result = v8::Number::New(isolate, first->Value() + second->Value());
    EXPECT_TRUE(result.As<v8::Number>()->Value() == 3);

    // 测试native函数
    result = nativeAdd(first, second);
    EXPECT_TRUE(result.As<v8::Number>()->Value() == 3);
    result = nativeAddWidthHandleScope(first, second);
    // 句柄已被销毁，试图访问会导致进程崩溃
    // result.As<v8::Number>()->IsNumber();
    result = nativeAddWidthEscapableHandleScope(first, second);
    EXPECT_TRUE(result.As<v8::Number>()->Value() == 3);

    // 测试全局函数
    v8::Local<v8::Value> argv[] = {first, second};
    result = globalAdd->Call(context, context->Global(), 2, argv).ToLocalChecked();
    EXPECT_TRUE(result.As<v8::Number>()->Value() == 3);
    result = globalAddWithHandleScope->Call(context, context->Global(), 2, argv).ToLocalChecked();
    // 句柄已被销毁，试图访问会导致进程崩溃
    // result.As<v8::Number>()->IsNumber();
    result = globalAddWithEscapableHandleScope->Call(context, context->Global(), 2, argv).ToLocalChecked();
    EXPECT_TRUE(result.As<v8::Number>()->Value() == 3);
}
TEST_F(Environment, handle_scope_test_NumberOfHandles) {
    v8::Isolate *isolate = getIsolate();
    v8::HandleScope handleScope(isolate);
    v8::Local<v8::Number> num1 = v8::Number::New(isolate, 1);
    v8::Local<v8::Number> num2 = v8::Number::New(isolate, 1);
    v8::Local<v8::Number> num3 = v8::Number::New(isolate, 1);
    v8::Local<v8::Number> num4 = v8::Number::New(isolate, 1);
    v8::Local<v8::Number> num5 = v8::Number::New(isolate, 1);
    v8::Local<v8::Number> num6 = v8::Number::New(isolate, 1);
    EXPECT_TRUE(handleScope.NumberOfHandles(isolate) == 6);
}