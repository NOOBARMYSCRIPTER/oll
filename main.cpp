#include <jni.h>
#include <unistd.h>
#include <android/log.h>
#include <string.h>
#include <unwind.h>
#include <dlfcn.h>
#include <stdio.h>
#include "dobby.h"

#define LOG_TAG "BYPASS"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

struct BacktraceState {
    void** current;
    void** end;
};

int (*orig_log_buf_write)(int bufID, int priority, const char* tag, const char* msg);
pid_t (*orig_fork)();

pid_t my_fork() {
    void* return_address = __builtin_return_address(0);
    
    if (orig_log_buf_write) {
        Dl_info info;
        char fork_buf[512]; 
        
        if (dladdr(return_address, &info) && info.dli_fname) {
            uintptr_t offset = (uintptr_t)return_address - (uintptr_t)info.dli_fbase;
            
            snprintf(fork_buf, sizeof(fork_buf), 
                     "🚨 [FORK DETECTED] Called fork() from: %s (IDA Offset: 0x%lx)", 
                     info.dli_fname, offset);
        } else {
            snprintf(fork_buf, sizeof(fork_buf), 
                     "🚨 [FORK DETECTED] Raw caller address: %p", 
                     return_address);
        }
        
        orig_log_buf_write(0, ANDROID_LOG_WARN, "BYPASS_DEBUG", fork_buf);
    }

    if (!orig_fork) return 0;
    
    pid_t res = orig_fork();
    if (res > 0) {
        if (orig_log_buf_write) {
            orig_log_buf_write(0, ANDROID_LOG_INFO, "BYPASS", "🚨 [NATIVE FORK] Spoofing child process PID to 0!");
        }
        return 0;
    }
    return res;
}

_Unwind_Reason_Code unwind_callback(struct _Unwind_Context* context, void* arg) {
    BacktraceState* state = static_cast<BacktraceState*>(arg);
    uintptr_t pc = _Unwind_GetIP(context);
    
    if (pc) {
        if (state->current == state->end) {
            return _URC_END_OF_STACK;
        } else {
            *state->current++ = reinterpret_cast<void*>(pc);
        }
    }
    return _URC_NO_REASON;
}

size_t capture_backtrace(void** buffer, size_t max_lines) {
    BacktraceState state = {buffer, buffer + max_lines};
    _Unwind_Backtrace(unwind_callback, &state);
    return state.current - buffer;
}

int my_log_buf_write(int bufID, int priority, const char* tag, const char* msg) {
    if (orig_log_buf_write) {
        return orig_log_buf_write(bufID, priority, tag, msg);
    }
    return 0;
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    LOGI("[+] AUTO-INITIALIZING HOOKS VIA JNI_ONLOAD (HOUDINI STABLE)");

    void* fork_addr = DobbySymbolResolver("libc.so", "fork");
    if (fork_addr) {
        DobbyHook(fork_addr, (void*)my_fork, (void**)&orig_fork);
        LOGI("[+] Native hook on fork() successfully installed.");
    }

    void* log_addr = DobbySymbolResolver("liblog.so", "__android_log_buf_write");
    if (log_addr) {
        DobbyHook(log_addr, (void*)my_log_buf_write, (void**)&orig_log_buf_write);
        LOGI("[+] Native hook on __android_log_buf_write successfully installed.");
    }

    return JNI_VERSION_1_6;
}
