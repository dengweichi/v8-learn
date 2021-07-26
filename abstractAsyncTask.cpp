//
// Created by CF on 2021/7/26.
//
#if defined(LINUX)
#include "linuxThreadDelegate.h"
#elif defined(WIN)
#include "winThreadDelegate.h"
#endif
#include "./abstractAsyncTask.h"
#include <functional>

AbstractAsyncTask::AbstractAsyncTask() {
#if defined(LINUX)
    this->delegate = new LinuxThreadDelegate(this);
#elif defined(WIN)
#endif
}
AbstractAsyncTask::~AbstractAsyncTask() {
    delete this->delegate;
}
void AbstractAsyncTask::join() {
    this->delegate->joinThread();
}
void AbstractAsyncTask::detach() {
    this->delegate->detachThread();
}
void AbstractAsyncTask::start() {
    this->delegate->createThread();
}