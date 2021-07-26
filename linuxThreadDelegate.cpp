//
// Created by CF on 2021/7/26.
//

#include "linuxThreadDelegate.h"
using threadFun = void (*)();

void LinuxThreadDelegate::createThread() {
    pthread_create(
            &this->tid, nullptr, [](void *that) -> void * {
                auto* delegate = reinterpret_cast<LinuxThreadDelegate *>(that);
                delegate->abstractAsyncTask->run();
            },
            this);
}
void LinuxThreadDelegate::joinThread() {
    pthread_join(this->tid, nullptr);
}
void LinuxThreadDelegate::detachThread() {
    pthread_detach(this->tid);
}
