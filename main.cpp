#include <jni.h>
#include <unistd.h>
#include <android/log.h>
#include <string.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdint.h>
#include "dobby.h"

#define LOG_TAG "BYPASS"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

pid_t (*orig_fork)();
int (*orig_log_buf_write)(int bufID, int priority, const char* tag, const char* msg);
int (*orig_kill)(pid_t pid, int sig);

pid_t anti_cheat_child_pid = 0;
uintptr_t libstub_base = 0;

uintptr_t get_module_base(const char* module_name) {
    FILE* maps = fopen("/proc/self/maps", "r");
    if (!maps) return 0;

    char line[512];
    uintptr_t base = 0;
    while (fgets(line, sizeof(line), maps)) {
        if (strstr(line, module_name) != nullptr) {
            base = strtoull(line, nullptr, 16);
            break;
        }
    }
    fclose(maps);
    return base;
}

void sub_5FA64_instrument_callback(void* address, DobbyRegisterContext* ctx) {
    uintptr_t* raw_regs = (uintptr_t*)ctx;
    
    uintptr_t x11_value = raw_regs[11];
    
    if (libstub_base == 0) {
        libstub_base = get_module_base("libstub.so");
    }

    char log_buf[256];
    if (libstub_base != 0 && x11_value >= libstub_base) {
        uintptr_t ida_offset = x11_value - libstub_base;
        snprintf(log_buf, sizeof(log_buf), "🎯 [X11 TRACER] Абсолютный адрес: 0x%lx | Смещение в IDA: 0x%lx", x11_value, ida_offset);
    } else {
        Dl_info info;
        if (dladdr((void*)x11_value, &info) && info.dli_fname) {
            uintptr_t diff = x11_value - (uintptr_t)info.dli_fbase;
            snprintf(log_buf, sizeof(log_buf), "🎯 [X11 TRACER] Вызов внешнего модуля: %s (Offset: 0x%lx)", info.dli_fname, diff);
        } else {
            snprintf(log_buf, sizeof(log_buf), "🎯 [X11 TRACER] Неизвестный регион / Куча: 0x%lx", x11_value);
        }
    }

    if (orig_log_buf_write) {
        orig_log_buf_write(0, ANDROID_LOG_WARN, "BYPASS_DEBUG", log_buf);
    } else {
        LOGW("%s", log_buf);
    }
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

void* interception_thread(void* arg) {
    LOGI("[*] Ожидаем появление libstub.so в памяти...");
    
    while (libstub_base == 0) {
        libstub_base = get_module_base("libstub.so");
        usleep(10000);
    }
    
    LOGI("[+] libstub.so найдена по адресу: 0x%lx. Ставим ассемблерный хук...", libstub_base);
    
    uintptr_t target_func = libstub_base + 0x5FA64;
    
    DobbyInstrument((void*)target_func, sub_5FA64_instrument_callback);
    LOGI("[🏆] Ассемблерный трейсер на sub_5FA64 успешно установлен!");
    
    return nullptr;
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

    pthread_t th;
    pthread_create(&th, nullptr, interception_thread, nullptr);

    return JNI_VERSION_1_6;
}
