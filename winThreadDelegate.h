//
// Created by CF on 2021/7/26.
//

#ifndef V8_LEARN_WIN_THREAD_DELEGATE_H
#define V8_LEARN_WIN_THREAD_DELEGATE_H
#include "./abstractAsyncTask.h"
#include <windows.h>

class WinThreadDelegate:public AbstractAsyncTask::Delegate{
private:
    DWORD  threadId = 0;
public:
    explicit WinThreadDelegate(AbstractAsyncTask* abstractAsyncTask): AbstractAsyncTask::Delegate(abstractAsyncTask){};
    void createThread() override;
    void detachThread() override;
    void joinThread() override;
};
#endif//V8_LEARN_WIN_THREAD_DELEGATE_H
