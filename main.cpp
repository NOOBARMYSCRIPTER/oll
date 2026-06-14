#include <jni.h>
#include <unistd.h>
#include <android/log.h>
#include <string.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "dobby.h"

#define LOG_TAG "BYPASS_SCAN"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

int (*orig_log_buf_write)(int bufID, int priority, const char* tag, const char* msg);
uintptr_t libstub_base = 0;
bool scanned = false;

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

void inspect_memory_safely() {
    if (libstub_base == 0) {
        libstub_base = get_module_base("libstub.so");
    }
    
    if (libstub_base == 0) {
        LOGI("[-] libstub.so не найдена в maps в данный момент.");
        return;
    }

    uintptr_t target_address = libstub_base + 0x5FA64;
    LOGI("[+] База libstub.so: 0x%lx | Целевой адрес: 0x%lx", libstub_base, target_address);

    uint32_t* code_ptr = (uint32_t*)target_address;
    
    LOGI("[👀] Снимок инструкций по смещению 0x5FA64:");
    for (int i = 0; i < 8; i++) {
        LOGI("    Address: 0x%lx | Opcode: 0x%08X", target_address + (i * 4), code_ptr[i]);
    }
}

int my_log_buf_write(int bufID, int priority, const char* tag, const char* msg) {
    if (msg && !scanned) {
        if (strstr(msg, "starting self-protect") != nullptr) {
            scanned = true;
            
            int ret = orig_log_buf_write(bufID, priority, tag, msg);
            
            LOGI("[🎯] Сработал триггер лога! Код расшифрован. Начинаем чтение...");
            inspect_memory_safely();
            
            return ret;
        }
    }

    if (orig_log_buf_write) {
        return orig_log_buf_write(bufID, priority, tag, msg);
    }
    return 0;
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    void* log_addr = DobbySymbolResolver("liblog.so", "__android_log_buf_write");
    if (log_addr) {
        DobbyHook(log_addr, (void*)my_log_buf_write, (void**)&orig_log_buf_write);
    }

    return JNI_VERSION_1_6;
}
