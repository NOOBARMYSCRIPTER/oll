#include <jni.h>
#include <unistd.h>
#include <android/log.h>
#include <string.h>
#include <dlfcn.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "dobby.h"

#define LOG_TAG "BYPASS"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

pid_t (*orig_fork)();
int (*orig_log_buf_write)(int bufID, int priority, const char* tag, const char* msg);

pid_t anti_cheat_child_pid = 0;

pid_t my_fork() {
    if (!orig_fork) return 0;
    
    pid_t actual_pid = orig_fork();
    
    if (actual_pid == 0) {
        return 0;
    } else {
        anti_cheat_child_pid = actual_pid;
        return actual_pid;
    }
}

int (*orig_kill)(pid_t pid, int sig);
int my_kill(pid_t pid, int sig) {
    if (sig == 4 || sig == 9 || sig == 11) {
        if (orig_log_buf_write) {
            char buf[128];
            snprintf(buf, sizeof(buf), "🛡️ [SIGNAL BLOCK] Античит пытался послать сигнал %d процессу %d. БЛОКИРУЕМ!", sig, pid);
            orig_log_buf_write(0, ANDROID_LOG_WARN, "BYPASS_DEBUG", buf);
        }
        return 0;
    }
    if (orig_kill) {
        return orig_kill(pid, sig);
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
    LOGI("[+] INITIALIZING ANTI-SIGNAL BYPASS LAYER");

    void* fork_addr = DobbySymbolResolver("libc.so", "fork");
    if (fork_addr) {
        DobbyHook(fork_addr, (void*)my_fork, (void**)&orig_fork);
    }

    void* kill_addr = DobbySymbolResolver("libc.so", "kill");
    if (kill_addr) {
        DobbyHook(kill_addr, (void*)my_kill, (void**)&orig_kill);
    }

    void* log_addr = DobbySymbolResolver("liblog.so", "__android_log_buf_write");
    if (log_addr) {
        DobbyHook(log_addr, (void*)my_log_buf_write, (void**)&orig_log_buf_write);
    }

    return JNI_VERSION_1_6;
}
