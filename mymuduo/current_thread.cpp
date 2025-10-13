#include "current_thread.h"
#include <unistd.h>
#include <sys/syscall.h>

namespace CurrentThread{
    thread_local pid_t cached_tid = 0;

    void CacheTid(){
        if (cached_tid == 0){
            // 通过 linux 系统调用, 获取当前线程 tid
            cached_tid = static_cast<pid_t>(::syscall(SYS_gettid));
        }
    }
}