//
// Created by CF on 2021/7/26.
//

#ifndef V8_LEARN_LINUX_THREAD_DELEGATE_H
#define V8_LEARN_LINUX_THREAD_DELEGATE_H
#include "./abstractAsyncTask.h"
#include <pthread.h>
#include "./abstractAsyncTask.h"

class LinuxThreadDelegate: public AbstractAsyncTask::Delegate{
private:
    pthread_t tid = 0;
public:
    explicit LinuxThreadDelegate(AbstractAsyncTask *abstractAsyncTask): AbstractAsyncTask::Delegate(abstractAsyncTask){};
    void createThread() override;
    void joinThread() override;
    void detachThread() override;
};
#endif//V8_LEARN_LINUX_THREAD_DELEGATE_H
