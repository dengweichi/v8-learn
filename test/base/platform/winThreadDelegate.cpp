//
// Created by CF on 2021/7/26.
//
#include "winThreadDelegate.h"
#include<Windows.h>
#include <process.h>
void WinThreadDelegate::createThread() {
    uintptr_t result = _beginthreadex(nullptr, 0, [](void* arg) -> unsigned int {
      auto delegate = reinterpret_cast<WinThreadDelegate*>(arg);
      delegate->abstractAsyncTask->run();
      return 0;
    }, this, 0, &this->threadId);
    thread = reinterpret_cast<HANDLE>(result);
}

void WinThreadDelegate::joinThread() {
    if (this->threadId != GetCurrentThreadId()) {
        WaitForSingleObject(this->thread, INFINITE);
    }
}

WinThreadDelegate::~WinThreadDelegate() {
    if (thread != nullptr) {
        CloseHandle(this->thread);
    }
}
