#pragma once

#include <unistd.h>


namespace CurrentThread{
    extern thread_local pid_t cached_tid;

    inline int Tid(){
        if (cached_tid == 0){
            // 声明 CacheTid 函数, 避免循环包含
            extern void CacheTid();
            CacheTid();
        }

        return cached_tid;
    }

    // 添加一个辅助函数用于检查当前线程是否缓存了TID
    inline bool IsMainThread() {
        return Tid() == ::getpid();
    }
}