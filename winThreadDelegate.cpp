//
// Created by CF on 2021/7/26.
//
#include "winThreadDelegate.h"
#include<Windows.h>

void WinThreadDelegate::createThread() {
    CreateThread(nullptr, 0, [](LPVOID p) -> DWORD {
       auto delegate = reinterpret_cast<WinThreadDelegate*>(p);
       delegate->abstractAsyncTask->run();
    }, this, 0, &this->threadId);
}
void WinThreadDelegate::detachThread() {

}
void WinThreadDelegate::joinThread() {

}
