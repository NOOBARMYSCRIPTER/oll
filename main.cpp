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

#define LOG_TAG "BYPASS_FINAL"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

uintptr_t libstub_base = 0;

uint32_t original_br_x11 = 0xD61F0160; 

void* (*orig_sub_8C894)(void* result, unsigned int* data_block);
unsigned int fake_block[16];

void* my_sub_8C894(void* result, unsigned int* data_block) {
    uintptr_t current_block_addr = (uintptr_t)data_block;
    uintptr_t target_patched_addr = libstub_base + 0x5FB88;

    if (current_block_addr <= target_patched_addr && target_patched_addr < (current_block_addr + 64)) {
        memcpy(fake_block, data_block, 64);
        size_t offset_in_block = (target_patched_addr - current_block_addr) / sizeof(unsigned int);
        
        fake_block[offset_in_block] = original_br_x11;
        
        __android_log_print(ANDROID_LOG_WARN, LOG_TAG, "🛡️ [CRC BYPASS] Скрыли патч на 0x5FB88 от SHA-256 сканера");
        
        return orig_sub_8C894(result, fake_block);
    }

    return orig_sub_8C894(result, data_block);
}

void loc_5FB88_instrument_callback(void* address, DobbyRegisterContext* ctx) {
    uintptr_t* raw_regs = (uintptr_t*)ctx;

    uintptr_t x11_target = raw_regs[11];
    
    if (libstub_base == 0) {
        libstub_base = get_module_base("libstub.so");
    }

    if (libstub_base != 0 && x11_target >= libstub_base) {
        uintptr_t ida_offset = x11_target - libstub_base;
        __android_log_print(ANDROID_LOG_ERROR, "🏆 UNPACK_SUCCESS", "🎯 АНТИЧИТ ПРЫГАЕТ НА СМЕЩЕНИЕ В IDA: 0x%lx (Абс: 0x%lx)", ida_offset, x11_target);
    } else {
        Dl_info info;
        if (dladdr((void*)x11_target, &info) && info.dli_fname) {
            uintptr_t diff = x11_target - (uintptr_t)info.dli_fbase;
            __android_log_print(ANDROID_LOG_ERROR, "🏆 UNPACK_SUCCESS", "🎯 ПРЫЖОК В ДРУГУЮ ЛИБУ: %s (Смещение: 0x%lx)", info.dli_fname, diff);
        } else {
            __android_log_print(ANDROID_LOG_ERROR, "🏆 UNPACK_SUCCESS", "🎯 ПРЫЖОК В КУЧУ/ДИНАМИЧЕСКИЙ КОД: 0x%lx", x11_target);
        }
    }
}

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

void* interception_thread(void* arg) {
    while (libstub_base == 0) {
        libstub_base = get_module_base("libstub.so");
        usleep(5000);
    }
    
    LOGI("[+] libstub.so обнаружена по адресу: 0x%lx", libstub_base);

    uintptr_t hash_func = libstub_base + 0x8C894;
    DobbyHook((void*)hash_func, (void*)my_sub_8C894, (void**)&orig_sub_8C894);
    LOGI("[+] Защита CRC успешно активирована.");

    uintptr_t target_br = libstub_base + 0x5FB88;
    DobbyInstrument((void*)target_br, loc_5FB88_instrument_callback);
    LOGI("[+] Инструмент на loc_5FB88 (BR X11) взведен!");

    return nullptr;
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    pthread_t th;
    pthread_create(&th, nullptr, interception_thread, nullptr);
    return JNI_VERSION_1_6;
}
