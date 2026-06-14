#include <jni.h>
#include <unistd.h>
#include <android/log.h>
#include <string.h>
#include <unwind.h>
#include <thread>
#include <chrono>
#include <dlfcn.h>
#include "dobby.h"

#define LOG_TAG "BYPASS"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

pid_t (*orig_fork)();

struct BacktraceState {
    void** current;
    void** end;
};

pid_t my_fork() {
    if (!orig_fork) return 0;
    pid_t res = orig_fork();
    
    if (res > 0) {
        LOGI("🚨 [NATIVE FORK] Intercepted fork process (PID: %d). Spoofing return value to 0!", res);
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

int (*orig_log_buf_write)(int bufID, int priority, const char* tag, const char* msg);

int my_log_buf_write(int bufID, int priority, const char* tag, const char* msg) {
    if (tag && (strstr(tag, "SelfProtect") != nullptr || (msg && strstr(msg, "fork") != nullptr))) {
        LOGI("\n[NATIVE LOG DETECTED] [%s]: %s", tag, msg);
        LOGI("--- NATIVE BACKTRACE START ---");

        const size_t max_frames = 30;
        void* buffer[max_frames];
        size_t frames = capture_backtrace(buffer, max_frames);

        for (size_t i = 0; i < frames; i++) {
            void* addr = buffer[i];
            Dl_info info;

            if (dladdr(addr, &info) && info.dli_fname) {
                uintptr_t offset = reinterpret_cast<uintptr_t>(addr) - reinterpret_cast<uintptr_t>(info.dli_fbase);
                
                LOGI("  #%02zu PC %p  %s (offset: 0x%lx) %s", 
                     i, 
                     addr, 
                     info.dli_fname, 
                     (unsigned long)offset, 
                     info.dli_sname ? info.dli_sname : "");
            } else {
                LOGI("  #%02zu PC %p  [Unknown Module]", i, addr);
            }
        }
        LOGI("--- NATIVE BACKTRACE END ---\n");
    }
    return orig_log_buf_write(bufID, priority, tag, msg);
}

__attribute__((constructor)) void init() {
    LOGI("[+] NATIVE BYPASS LAUNCHED VIA DT_NEEDED!");

    std::thread([]() {
        void* fork_addr = DobbySymbolResolver("libc.so", "fork");
        if (fork_addr) {
            DobbyHook(fork_addr, (void*)my_fork, (void**)&orig_fork);
            LOGI("[+] Native hook on fork() successfully installed.");
        } else {
            LOGI("[-] Failed to find fork in libc.so");
        }
    
        void* log_addr = DobbySymbolResolver("liblog.so", "__android_log_buf_write");
        if (log_addr) {
            DobbyHook(log_addr, (void*)my_log_buf_write, (void**)&orig_log_buf_write);
            LOGI("[+] Native hook on __android_log_buf_write successfully installed.");
        } else {
            LOGI("[-] Failed to find __android_log_buf_write in liblog.so");
        }
    }).detach();
}
