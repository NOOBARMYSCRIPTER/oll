#include <jni.h>
#include <dlfcn.h>
#include <android/log.h>
#include <stdint.h>

#define LOG_TAG "BYPASS_SCANNER"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

uintptr_t libstub_base = 0;
void (*orig_sub_5FA64)() = nullptr;

void my_sub_5FA64() {
    uintptr_t true_anticheat_pc = 0;
    
    #if defined(__aarch64__)
    __asm__ __volatile__("mov %0, x11" : "=r"(true_anticheat_pc));
    #elif defined(__x86_64__)
    __asm__ __volatile__("mov %0, r11" : "=r"(true_anticheat_pc));
    #endif

    if (libstub_base != 0 && true_anticheat_pc != 0) {
        uintptr_t ida_offset = true_anticheat_pc - libstub_base;
        
        LOGI("[🎯] ===================================================");
        LOGI("[🎯] ANTI-CHEAT REAL ENTRY POINT FOUND!");
        LOGI("[🎯] Absolute PC in Memory: %p", (void*)true_anticheat_pc);
        LOGI("[🎯] EXACT IDA PRO OFFSET: 0x%lx", (unsigned long)ida_offset);
        LOGI("[🎯] ===================================================");
    }

    if (orig_sub_5FA64) {
        orig_sub_5FA64();
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
            
            extern int DobbyHook(void* target, void* replace, void** origin);
            DobbyHook(target_trampoline, (void*)my_sub_5FA64, (void**)&orig_sub_5FA64);
            LOGI("[✅] Logger successfully attached to sub_5FA64!");
        } else {
            LOGE("[-] Failed to resolve base address for libstub.so");
        }
    } else {
        LOGE("[-] libstub.so not found in memory!");
    }

    return JNI_VERSION_1_6;
}
