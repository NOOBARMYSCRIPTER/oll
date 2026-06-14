#include <jni.h>
#include <unistd.h>
#include <android/log.h>
#include <string.h>
#include <unwind.h>
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
    if (tag && msg) {
        if (strstr(tag, "SelfProtect") != nullptr || strstr(msg, "fork") != nullptr) {
            if (orig_log_buf_write) {
                orig_log_buf_write(bufID, priority, "BYPASS_DEBUG", "[+] --------------------------------------------");
                orig_log_buf_write(bufID, priority, "BYPASS_DEBUG", "[+] Intercepted anti-cheat log!");
                orig_log_buf_write(bufID, priority, tag, msg);

                const size_t max_lines = 10;
                void* buffer[max_lines];
                size_t frames = capture_backtrace(buffer, max_lines);

                orig_log_buf_write(bufID, priority, "BYPASS_DEBUG", "[+] --- BACKTRACE ---");
                
                for (size_t i = 0; i < frames; i++) {
                    Dl_info info;
                    char line_buf[512];
                    
                    if (dladdr(buffer[i], &info) && info.dli_fname) {
                        uintptr_t relative_offset = (uintptr_t)buffer[i] - (uintptr_t)info.dli_fbase;
                        
                        snprintf(line_buf, sizeof(line_buf), "  #%02zu PC 0x%lx  %s (offset: 0x%lx) %s", 
                                 i, (uintptr_t)buffer[i], info.dli_fname, relative_offset, 
                                 info.dli_sname ? info.dli_sname : "");
                    } else {
                        snprintf(line_buf, sizeof(line_buf), "  #%02zu PC 0x%lx  [Unknown Module]", i, (uintptr_t)buffer[i]);
                    }
                    
                    orig_log_buf_write(bufID, priority, "BYPASS_DEBUG", line_buf);
                }
                orig_log_buf_write(bufID, priority, "BYPASS_DEBUG", "[+] --------------------------------------------");
            }
        }
    }
    
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
