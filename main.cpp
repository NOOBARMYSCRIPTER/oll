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
long (*orig_ptrace)(int request, pid_t pid, void* addr, void* data);

pid_t my_fork() {
    if (!orig_fork) return 0;
    
    pid_t actual_pid = orig_fork();
    
    if (actual_pid == 0) {
        return 0;
    } else {
        return actual_pid;
    }
}

long my_ptrace(int request, pid_t pid, void* addr, void* data) {
    if (request == 0 || request == 16) {
        return 0; 
    }
    
    if (request == 1 || request == 2) {
        return 0;
    }

    if (orig_ptrace) {
        return orig_ptrace(request, pid, addr, data);
    }
    return -1;
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    LOGI("[+] INITIALIZING STABLE BYPASS LAYER");

    void* fork_addr = DobbySymbolResolver("libc.so", "fork");
    if (fork_addr) {
        DobbyHook(fork_addr, (void*)my_fork, (void**)&orig_fork);
    }

    void* ptrace_addr = DobbySymbolResolver("libc.so", "ptrace");
    if (ptrace_addr) {
        DobbyHook(ptrace_addr, (void*)my_ptrace, (void**)&orig_ptrace);
    }

    return JNI_VERSION_1_6;
}
