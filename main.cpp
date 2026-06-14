#include <jni.h>
#include <unistd.h>
#include <android/log.h>
#include <string.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdint.h>
#include "dobby.h"

#define LOG_TAG "BYPASS_X11"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

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

    if (libstub_base != 0 && x11_value >= libstub_base) {
        uintptr_t ida_offset = x11_value - libstub_base;
        __android_log_print(ANDROID_LOG_WARN, LOG_TAG, "🎯 [X11 TRACER] Абсолютный адрес: 0x%lx | Смещение в IDA: 0x%lx", x11_value, ida_offset);
    } else {
        Dl_info info;
        if (dladdr((void*)x11_value, &info) && info.dli_fname) {
            uintptr_t diff = x11_value - (uintptr_t)info.dli_fbase;
            __android_log_print(ANDROID_LOG_WARN, LOG_TAG, "🎯 [X11 TRACER] Вызов внешнего модуля: %s (Offset: 0x%lx)", info.dli_fname, diff);
        } else {
            __android_log_print(ANDROID_LOG_WARN, LOG_TAG, "🎯 [X11 TRACER] Неизвестный регион / Куча: 0x%lx", x11_value);
        }
    }
}

void* interception_thread(void* arg) {
    LOGI("[*] Фоновый поток запущен. Ожидаем загрузку libstub.so...");
    
    while (libstub_base == 0) {
        libstub_base = get_module_base("libstub.so");
        usleep(5000);
    }
    
    LOGI("[+] libstub.so обнаружена по адресу: 0x%lx. Устанавливаем инструмент...", libstub_base);
    
    uintptr_t target_func = libstub_base + 0x5FA64;
    
    DobbyInstrument((void*)target_func, sub_5FA64_instrument_callback);
    LOGI("[🏆] Инструмент на sub_5FA64 (BR X11) успешно установлен!");
    
    return nullptr;
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    pthread_t th;
    pthread_create(&th, nullptr, interception_thread, nullptr);

    return JNI_VERSION_1_6;
}
