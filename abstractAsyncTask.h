//
// Created by CF on 2021/7/26.
//

#ifndef V8_LEARN_ABSTRACT_ASYNC_TASK_H
#define V8_LEARN_ABSTRACT_ASYNC_TASK_H
 class AbstractAsyncTask{
     using threadFun = void (*)();
 public:
     AbstractAsyncTask();
     ~AbstractAsyncTask();
     virtual void run() = 0;
     void join();
     void detach();
     void start();
     class Delegate{
     protected:
         AbstractAsyncTask* abstractAsyncTask = nullptr;
     public:
         explicit Delegate(AbstractAsyncTask* abstractAsyncTask):abstractAsyncTask(abstractAsyncTask){};
         virtual void createThread() = 0;
         virtual void joinThread() = 0;
         virtual void detachThread() = 0;
     };
 private:
     Delegate* delegate;
 };
#endif// V8_LEARN_ABSTRACT_ASYNC_TASK_H
