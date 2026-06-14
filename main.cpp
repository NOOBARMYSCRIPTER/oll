#include <jni.h>
#include <dlfcn.h>
#include <android/log.h>
#include <stdint.h>
#include "dobby.h"

#define LOG_TAG "BYPASS_SCANNER"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

uintptr_t libstub_base = 0;

void my_sub_5FA64_handler(RegisterContext *reg_ctx, const HookEntryInfo *info) {
    uintptr_t true_anticheat_pc = reg_ctx->general_regs.x[11];
    
    if (libstub_base != 0 && true_anticheat_pc != 0) {
        uintptr_t ida_offset = true_anticheat_pc - libstub_base;
        
        LOGI("[🎯] ===================================================");
        LOGI("[🎯] ANTI-CHEAT REAL ENTRY POINT FOUND!");
        LOGI("[🎯] Absolute PC in Memory: %p", (void*)true_anticheat_pc);
        LOGI("[🎯] EXACT IDA PRO OFFSET: 0x%lx", (unsigned long)ida_offset);
        LOGI("[🎯] ===================================================");
    }
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    LOGI("[+] INITIALIZING ANTI-CHEAT DESTINATION SCANNER");

    void* lib_handle = dlopen("libstub.so", RTLD_NOW);
    if (!lib_handle) {
        lib_handle = dlopen("libstub.so", RTLD_LAZY);
    }

    if (lib_handle) {
        Dl_info info;
        if (dladdr(lib_handle, &info) && info.dli_fbase) {
            libstub_base = (uintptr_t)info.dli_fbase;
            void* target_trampoline = (void*)(libstub_base + 0x5FA64);
            
            LOGI("[+] Trampoline function calculated at: %p", target_trampoline);
            
            DobbyInstrument(target_trampoline, my_sub_5FA64_handler);
            LOGI("[✅] Context logger successfully attached to sub_5FA64!");
        } else {
            LOGE("[-] Failed to resolve base address for libstub.so");
        }
    } else {
        LOGE("[-] libstub.so not found in memory!");
    }

    return JNI_VERSION_1_6;
}
