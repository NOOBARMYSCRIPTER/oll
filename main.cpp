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
int (*orig_kill)(pid_t pid, int sig);
void (*orig_sub_5EFA0)(void* args) = nullptr;

pid_t anti_cheat_child_pid = 0;

void my_sub_5EFA0(void* args) {
    LOGI("🛡️ [CRITICAL BYPASS] sub_5EFA0 intercepted! Anti-cheat thread creation blocked successfully.");
    return;
}

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

int my_kill(pid_t pid, int sig) {
    if (sig == 4 || sig == 9 || sig == 11) {
        if (orig_log_buf_write) {
            char buf;
            snprintf(buf, sizeof(buf), "🛡️ [SIGNAL BLOCK] Anti-cheat attempted to send signal %d to process %d. BLOCKING!", sig, pid);
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
    LOGI("[+] INITIALIZING COMBINED ANTI-SIGNAL & RE-HOOK LAYER");

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

    void* lib_handle = dlopen("libstub.so", RTLD_NOW);
    if (!lib_handle) {
        lib_handle = dlopen("libstub.so", RTLD_LAZY);
    }

    if (lib_handle) {
        Dl_info info;
        if (dladdr(lib_handle, &info) && info.dli_fbase) {
            uintptr_t base_address = (uintptr_t)info.dli_fbase;
            void* target_function = (void*)(base_address + 0x5EFA0);
            
            DobbyHook(target_function, (void*)my_sub_5EFA0, (void**)&orig_sub_5EFA0);
            LOGI("[✅] Internal thread function 0x5EFA0 successfully patched!");
        }
    }

    return JNI_VERSION_1_6;
}
