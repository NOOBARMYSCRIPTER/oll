#include <jni.h>
#include <unistd.h>
#include <android/log.h>
#include <string.h>
#include <dlfcn.h>
#include <stdio.h>
#include <sys/types.h>
#include "dobby.h"

#define LOG_TAG "BYPASS"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

pid_t (*orig_fork)();
int (*orig_log_buf_write)(int bufID, int priority, const char* tag, const char* msg);
long (*orig_ptrace)(int request, pid_t pid, void* addr, void* data);

pid_t my_fork() {
    if (!orig_fork) return 0;
    
    pid_t actual_pid = orig_fork();
    
    if (actual_pid == 0) {
        if (orig_log_buf_write) {
            orig_log_buf_write(0, ANDROID_LOG_INFO, "BYPASS", "[+] Inside the protection child process.");
        }
        return 0;
    } else {
        if (orig_log_buf_write) {
            char buf[128];
            snprintf(buf, sizeof(buf), "[+] Created anti-cheat child process with PID: %d", actual_pid);
            orig_log_buf_write(0, ANDROID_LOG_INFO, "BYPASS", buf);
        }
        return actual_pid;
    }
}

long my_ptrace(int request, pid_t pid, void* addr, void* data) {
    if (request == 0 || request == 16) {
        if (orig_log_buf_write) {
            char buf[128];
            snprintf(buf, sizeof(buf), "[PTRACE BYPASS] Blocked ptrace attempt (REQ: %d, PID: %d)", request, pid);
            orig_log_buf_write(0, ANDROID_LOG_WARN, "BYPASS_DEBUG", buf);
        }
        return 0; 
    }
    
    if (orig_ptrace) {
        return orig_ptrace(request, pid, addr, data);
    }
    return -1;
}

int my_log_buf_write(int bufID, int priority, const char* tag, const char* msg) {
    if (orig_log_buf_write) {
        if (tag && strstr(tag, "SelfProtect") != nullptr) {
            return orig_log_buf_write(bufID, priority, "BYPASS_AC", msg);
        }
        return orig_log_buf_write(bufID, priority, tag, msg);
    }
    return 0;
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    LOGI("[+] INITIALIZING ANTI-CHEAT BYPASS LAYER");

    void* fork_addr = DobbySymbolResolver("libc.so", "fork");
    if (fork_addr) {
        DobbyHook(fork_addr, (void*)my_fork, (void**)&orig_fork);
        LOGI("[+] Hook on fork() installed.");
    }

    void* ptrace_addr = DobbySymbolResolver("libc.so", "ptrace");
    if (ptrace_addr) {
        DobbyHook(ptrace_addr, (void*)my_ptrace, (void**)&orig_ptrace);
        LOGI("[+] Hook on ptrace() installed.");
    }

    void* log_addr = DobbySymbolResolver("liblog.so", "__android_log_buf_write");
    if (log_addr) {
        DobbyHook(log_addr, (void*)my_log_buf_write, (void**)&orig_log_buf_write);
        LOGI("[+] Hook on __android_log_buf_write installed.");
    }

    return JNI_VERSION_1_6;
}
