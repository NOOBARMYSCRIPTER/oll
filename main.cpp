#include <jni.h>
#include <unistd.h>
#include <android/log.h>
#include <string.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

#define LOG_TAG "BYPASS_DUMPER"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

int (*orig_log_buf_write)(int bufID, int priority, const char* tag, const char* msg);
bool is_dumped = false;

void dump_library(const char* lib_name, uintptr_t start_addr, uintptr_t end_addr) {
    char out_path[256];
    snprintf(out_path, sizeof(out_path), "/data/data/com.catsbit.oxidesurvivalisland/files/%s.dump.so", lib_name);
    
    size_t size = end_addr - start_addr;
    LOGI("[+] Dumping %s [0x%lx - 0x%lx] (%zu bytes) -> %s", lib_name, start_addr, end_addr, size, out_path);
    
    FILE* out = fopen(out_path, "wb");
    if (!out) {
        snprintf(out_path, sizeof(out_path), "/data/data/com.catsbit.oxidesurvivalisland/%s.dump.so", lib_name);
        out = fopen(out_path, "wb");
    }
    
    if (out) {
        fwrite((void*)start_addr, 1, size, out);
        fclose(out);
        LOGI("[🏆] Memory dump for %s successfully saved!", lib_name);
    } else {
        LOGW("[-] Failed to open output file for writing target: %s", lib_name);
    }
}

void dump_all_loaded_libraries() {
    FILE* maps = fopen("/proc/self/maps", "r");
    if (!maps) {
        LOGW("[-] Unable to open /proc/self/maps");
        return;
    }

    char line[512];
    char last_lib[256] = {0};
    uintptr_t lib_start = 0;
    uintptr_t lib_end = 0;

    LOGI("[*] Analyzing process memory layout via /proc/self/maps...");

    while (fgets(line, sizeof(line), maps)) {
        bool is_valid_region = (strstr(line, ".so") != nullptr || strstr(line, "anon:") != nullptr || (strstr(line, "/") == nullptr && strstr(line, " ") != nullptr));
        
        if (is_valid_region && lib_start != 0) {
            uintptr_t start, end;
            char perms[5];
            char path[256] = {0};
            
            int parsed = sscanf(line, "%lx-%lx %4s %*s %*s %*s %255s", &start, &end, perms, path);
            if (parsed >= 3) {
                if (strlen(path) > 0 && strstr(path, ".so") != nullptr) {
                    if (strstr(path, "com.catsbit") == nullptr && strstr(path, "/data/app") == nullptr) {
                        continue;
                    }
                    
                    char* lib_filename = strrchr(path, '/');
                    if (lib_filename) lib_filename++;
                    else lib_filename = path;

                    if (strstr(lib_filename, "liboxide_bypass.so") != nullptr) continue;

                    if (strcmp(last_lib, lib_filename) == 0) {
                        lib_end = end;
                    } else {
                        if (lib_start != 0 && lib_end > lib_start) {
                            dump_library(last_lib, lib_start, lib_end);
                        }
                        strncpy(last_lib, lib_filename, sizeof(last_lib));
                        lib_start = start;
                        lib_end = end;
                    }
                } else if (lib_start != 0 && last_lib[0] !=  '\0') {
                    lib_end = end;
                }
            }
        } else if (strstr(line, ".so") != nullptr && (strstr(line, "com.catsbit") != nullptr || strstr(line, "/data/app") != nullptr)) {
            uintptr_t start, end;
            char perms[5];
            char path[256] = {0};
            
            if (sscanf(line, "%lx-%lx %4s %*s %*s %*s %255s", &start, &end, perms, path) == 4) {
                char* lib_filename = strrchr(path, '/');
                if (lib_filename) lib_filename++;
                else lib_filename = path;

                if (strstr(lib_filename, "liboxide_bypass.so") != nullptr) continue;

                strncpy(last_lib, lib_filename, sizeof(last_lib));
                lib_start = start;
                lib_end = end;
            }
        }
    }

    if (lib_start != 0 && lib_end > lib_start) {
        dump_library(last_lib, lib_start, lib_end);
    }

    fclose(maps);
    LOGI("[+] Complete process memory dumping procedure finished!");
}

int my_log_buf_write(int bufID, int priority, const char* tag, const char* msg) {
    if (msg && !is_dumped) {
        if (strstr(msg, "starting self-protect") != nullptr) {
            is_dumped = true;
            
            if (orig_log_buf_write) {
                orig_log_buf_write(bufID, priority, "BYPASS_DEBUG", "⚠️ [DUMPER TRIGGERED] Target log message captured! Executing memory dump in 500ms...");
            }
            
            usleep(500000); 
            
            dump_all_loaded_libraries();
        }
    }

    if (orig_log_buf_write) {
        return orig_log_buf_write(bufID, priority, tag, msg);
    }
    return 0;
}

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    LOGI("[+] INITIALIZING AUTOMATIC LOGGER TRAP BYPASS LAYER");

    void* log_addr = dlsym(dlopen("liblog.so", RTLD_NOW), "__android_log_buf_write");
    if (!log_addr) {
        log_addr = dlsym(RTLD_DEFAULT, "__android_log_buf_write");
    }

    if (log_addr) {
        extern int DobbyHook(void* target, void* replace, void** origin);
        DobbyHook(log_addr, (void*)my_log_buf_write, (void**)&orig_log_buf_write);
    }

    return JNI_VERSION_1_6;
}
