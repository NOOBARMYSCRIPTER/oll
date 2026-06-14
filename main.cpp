#include <jni.h>
#include <unistd.h>
#include <android/log.h>
#include <string.h>
#include <dlfcn.h>
#include <stdio.h>
#include "dobby.h"

#define LOG_TAG "BYPASS"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

int (*orig_log_buf_write)(int bufID, int priority, const char* tag, const char* msg);
pid_t (*orig_fork)();

pid_t my_fork() {
    if (orig_log_buf_write) {
        orig_log_buf_write(0, ANDROID_LOG_WARN, "BYPASS_DEBUG", "[+] ============ SCANNING STACK FOR CALLER ============");

        // Get the current Stack Pointer (SP) on ARM64
        register uintptr_t* sp __asm__("sp");
        
        // Scan the next 128 pointers up the stack
        size_t scan_depth = 128;
        size_t found_frames = 0;

        for (size_t i = 0; i < scan_depth; i++) {
            uintptr_t possible_pc = sp[i];

            // Verify if the address looks like executable code in user space
            if (possible_pc > 0x0000000001000000 && possible_pc < 0x0000ffffffffffff) {
                Dl_info info;
                if (dladdr((void*)possible_pc, &info) && info.dli_fname) {
                    
                    // Filter out our own bypass library and the system libc
                    if (strstr(info.dli_fname, "liboxide_bypass.so") == nullptr && 
                        strstr(info.dli_fname, "libc.so") == nullptr) {
                        
                        uintptr_t offset = possible_pc - (uintptr_t)info.dli_fbase;
                        char line_buf[512];
                        snprintf(line_buf, sizeof(line_buf), "  🎯 Found offset: %s (IDA Offset: 0x%lx)", 
                                 info.dli_fname, offset);
                        
                        orig_log_buf_write(0, ANDROID_LOG_WARN, "BYPASS_DEBUG", line_buf);
                        found_frames++;
                        
                        if (found_frames >= 5) break; // First few matches are sufficient
                    }
                }
            }
        }
        orig_log_buf_write(0, ANDROID_LOG_WARN, "BYPASS_DEBUG", "[+] ===================================================");
    }

    if (!orig_fork) return 0;
    
    pid_t res = orig_fork();
    if (res > 0) {
        if (orig_log_buf_write) {
            orig_log_buf_write(0, ANDROID_LOG_INFO, "BYPASS", "🚨 [NATIVE FORK] Spoofing child process PID to 0!");
        }
        return 0;
    }
    return res;
}

int my_log_buf_write(int bufID, int priority, const char* tag, const char* msg) {
    if (orig_log_buf_write) {
        return orig_log_buf_write(bufID, priority, tag, msg);
    }
    return 0;
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    LOGI("[+] AUTO-INITIALIZING HOOKS VIA JNI_ONLOAD (HOUDINI STABLE)");

    void* fork_addr = DobbySymbolResolver("libc.so", "fork");
    if (fork_addr) {
        DobbyHook(fork_addr, (void*)my_fork, (void**)&orig_fork);
        LOGI("[+] Native hook on fork() successfully installed.");
    }

    void* log_addr = DobbySymbolResolver("liblog.so", "__android_log_buf_write");
    if (log_addr) {
        DobbyHook(log_addr, (void*)my_log_buf_write, (void**)&orig_log_buf_write);
        LOGI("[+] Native hook on __android_log_buf_write successfully installed.");
    }

    return JNI_VERSION_1_6;
}
