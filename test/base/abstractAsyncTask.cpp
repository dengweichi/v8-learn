//
// Created by CF on 2021/7/26.
//
#if defined(LINUX)
#include "./platform/linuxThreadDelegate.h"
#elif defined(WIN)
#include "./platform/winThreadDelegate.h"
#endif
#include "./abstractAsyncTask.h"
#include <functional>

AbstractAsyncTask::AbstractAsyncTask() {
#if defined(LINUX)
    this->delegate = new LinuxThreadDelegate(this);
#elif defined(WIN)
    this->delegate = new WinThreadDelegate(this);
#endif
}

AbstractAsyncTask::~AbstractAsyncTask() {
    delete this->delegate;
}
void AbstractAsyncTask::join() {
    this->delegate->joinThread();
}

void AbstractAsyncTask::start() {
    this->delegate->createThread();
}
