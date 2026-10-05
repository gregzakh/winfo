#ifndef _WINFO_H_
#define _WINFO_H_

#include <stdio.h>
#include <stdint.h>
#include <intrin.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

typedef const char* (*Func)(void);

typedef struct {
    const char* key;
    const char* description;
    Func func;
} Command;

#define _PRINT_LOG(stream, fmt, ...) do { \
    fprintf(stream, fmt, ##__VA_ARGS__);  \
    fprintf(stream, "\r\n");                \
} while(0)

#define log(fmt, ...) _PRINT_LOG(stdout, fmt, ##__VA_ARGS__)
#define err(fmt, ...) _PRINT_LOG(stderr, fmt, ##__VA_ARGS__)

#define S(x) #x,
#define array_size(x) (sizeof(x) / sizeof((x)[0]))
#define read_kdata(T, x) (*((T*)(0x7FFE0000 + (x))))
#define read_uint16(x) read_kdata(uint16_t, (x))
#define read_uint32(x) read_kdata(uint32_t, (x))
#define __transform(x) (((x & 0xF) * (x & 0xF) - 3 * (x & 0xF) + 19) / 29)

#define NtBuildNumber 0x260
#define NtProductType 0x264
#define NtProductTypeEnum S(WinNT) S(LanManNT) S(Server)
#define NativeProcessorArchitecture 0x26a
#define NativeProcessorArchitectureEnum S(X86) S(ARM) S(x86_64) S(Neutral) S(ARM64) S(x86OnARM) S(Unknown)
#define NtMajorVersion 0x26c
#define NtMinorVersion 0x270
#define BootId 0x2c4
#define SuiteMask 0x2d0
#define SuiteMaskEnum S(SmallBusiness) S(Enterprise) S(BackOffice) S(CommunicationServer) S(TerminalServer) \
                      S(SmallBusinessRestricted) S(EmbeddedNT) S(DataCenter) S(SingleUserTS) S(Personal)    \
                      S(Blade) S(EmbeddedRestricted) S(SecurityAppliance) S(StorageServer) S(ComputeServer) \
                      S(WHServer) S(PhoneNT) S(MultiUserTS) S(MaxSuiteType)


const char* get_product_type(void) {
    const char* pt[] = { NtProductTypeEnum };
    return pt[read_uint32(NtProductType) - 1];
}

const char* get_machine_hardware(void) {
    const char* npa[] = { NativeProcessorArchitectureEnum };
    return npa[__transform(read_uint16(NativeProcessorArchitecture))];
}

const char* get_suite_mask(void) {
    const char* sm[] = { SuiteMaskEnum };
    int32_t total_points = array_size(sm);
    uint32_t suite = read_uint32(SuiteMask);

    static char buf[1024];
    size_t cur_len = 0;
    const size_t max_len = sizeof(buf) - 1;

    for (int32_t i = 0; i < total_points; i++) {
        if ((suite & (1 << i)) != 0) {
            size_t str_len = strnlen_s(sm[i], max_len - cur_len);
            if (str_len == 0 || str_len >= (max_len - cur_len))
                break;
            
            if (cur_len > 0) {
                if (cur_len + 1 >= max_len)
                    break;
                
                buf[cur_len++] = ',';
                buf[cur_len] = '\0';
            }

            strcpy_s(buf + cur_len, max_len - cur_len, sm[i]);
            cur_len += str_len;
        }
    }
    
    return buf;
}

const char* get_system_version(void) {
    static char buf[32];
    sprintf_s(buf, sizeof(buf), "%u.%u.%u", read_uint32(NtMajorVersion),
                                            read_uint32(NtMinorVersion),
                                            read_uint32(NtBuildNumber));
    return buf;
}

#include <windows.h>
#pragma comment (lib, "advapi32")

#define CurrentVersion "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion"
#define PrefetchParameters "SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Memory Management\\PrefetchParameters"

struct reg_value {
    int32_t  status;
    uint32_t type;
    uint32_t size;
    uint8_t  *buffer;
};

struct reg_value reg_read(HKEY root, LPCSTR path, LPCSTR value) {
    struct reg_value val = { 0 };
    HKEY key = NULL;
    LSTATUS result;
    uint8_t *tmp = NULL;

    if (!path || !value) {
        val.status = ERROR_INVALID_PARAMETER;
        goto cleanup;
    }

    result = RegOpenKeyExA(root, path, 0, KEY_QUERY_VALUE | KEY_WOW64_64KEY, &key);
    if (result != ERROR_SUCCESS) {
        val.status = result;
        goto cleanup;
    }

    result = RegQueryValueExA(key, value, NULL, (LPDWORD)&val.type, NULL, (LPDWORD)&val.size);
    if (result != ERROR_MORE_DATA && result != ERROR_SUCCESS) {
        val.status = result;
        goto cleanup;
    }

    if (val.size == 0) {
        val.status = ERROR_SUCCESS;
        goto cleanup;
    }

    tmp = (uint8_t*)malloc(val.size);
    if (!tmp) {
        val.status = ERROR_NOT_ENOUGH_MEMORY;
        goto cleanup;
    }

    result = RegQueryValueExA(key, value, NULL, (LPDWORD)&val.type, tmp, (LPDWORD)&val.size);
    if (result != ERROR_SUCCESS) {
        free(tmp);
        val.status = result;
        goto cleanup;
    }

    val.buffer = tmp;
    val.status = ERROR_SUCCESS;

cleanup:
    if (key != NULL)
        RegCloseKey(key);

    return val;
}

const char* get_verstr_read(LPCSTR value) {
    static char buf[256];
    struct reg_value val;

    if (!value)
        return NULL;

    val = reg_read(HKEY_LOCAL_MACHINE, CurrentVersion, value);
    if (val.status != ERROR_SUCCESS || val.buffer == NULL) {
        if (val.buffer)
            free(val.buffer);
        
        return NULL;
    }

    if (val.type != REG_SZ && val.type != REG_EXPAND_SZ) {
        free(val.buffer);
        return NULL;
    }

    if (val.size >= sizeof(buf)) {
        free(val.buffer);
        return NULL;
    }

    memcpy_s(buf, sizeof(buf), val.buffer, val.size);
    buf[val.size] = '\0';

    free(val.buffer);
    return buf;
}

const char* get_kernel_version(void) {
    return get_verstr_read("CurrentVersion");
}

const char* get_kernel_release(void) {
    return get_verstr_read("BuildLabEx");
}

const char* get_system_versionex(void) {
    return get_verstr_read("LCUVer");
}

const char* get_registered_owner(void) {
    return get_verstr_read("RegisteredOwner");
}

const char* get_display_version(void) {
    return get_verstr_read("DisplayVersion");
}

uint8_t reg_num_read(LPCSTR key, LPCSTR value, uint64_t *out) {
    struct reg_value val;

    if (!key || !value || !out)
        return 0;
    
    val = reg_read(HKEY_LOCAL_MACHINE, key, value);
    if (val.status != ERROR_SUCCESS || val.buffer == NULL) {
        if (val.buffer)
            free(val.buffer);

        return 0;
    }

    if (val.type != REG_DWORD && val.type != REG_QWORD) {
        free(val.buffer);
        return 0;
    }

    if ((val.type == REG_DWORD && val.size != sizeof(uint32_t)) ||
        (val.type == REG_QWORD && val.size != sizeof(uint64_t))) {
        free(val.buffer);
        return 0;
    }

    if (val.type == REG_DWORD) {
        uint32_t tmp;
        memcpy_s(&tmp, sizeof(tmp), val.buffer, val.size);
        *out = (uint64_t)tmp;
    }
    else
        memcpy_s(out, sizeof(uint64_t), val.buffer, val.size);
    
    free(val.buffer);
    return 1;
}

const char* get_boot_statistic(void) {
    uint64_t completed;
    static char buf[40];

    if (reg_num_read(PrefetchParameters, "BootId", &completed) != 1)
        return NULL;
    
    sprintf_s(buf, sizeof(buf), "Tries: %u, Completed: %llu", read_uint32(BootId), completed);
    return buf;
}

const char* get_ubr(void) {
    uint64_t ubr;
    static char buf[10];

    if (reg_num_read(CurrentVersion, "UBR", &ubr) != 1)
        return NULL;

    sprintf_s(buf, sizeof(buf), "%llu", ubr);
    return buf;
}

const char* get_installation_time(void) {
    uint64_t ft;
    static char buf[64];
    const uint64_t epoch_diff = 116444736000000000ULL;

    if (reg_num_read(CurrentVersion, "InstallTime", &ft) != 1)
        return NULL;
    
    if (ft < epoch_diff)
        return NULL;
    
    time_t unix = (time_t)((ft - epoch_diff) / 10000000ULL);

    struct tm tmx;
    localtime_s(&tmx, &unix);
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tmx);

    return buf;
}

#endif