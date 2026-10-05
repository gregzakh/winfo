#include "winfo.h"

int main(int argc, char** argv) {
    Command cmds[] = {
        {"-b",  "system boot statistics", get_boot_statistic},
        {"-d",  "dispay version", get_display_version},
        {"-it", "installation time", get_installation_time},
        {"-k",  "system kernel version", get_kernel_version},
        {"-m",  "machine hardware name", get_machine_hardware},
        {"-o",  "registered owner", get_registered_owner},
        {"-p",  "system product type", get_product_type},
        {"-r",  "kernel release", get_kernel_release},
        {"-s",  "system suite mask", get_suite_mask},
        {"-u",  "update build revision (UBR)", get_ubr},
        {"-v",  "Windows version", get_system_version},
        {"-vu", "Windows version with UBR", get_system_versionex}
    };
    const int32_t count = array_size(cmds);

    if (argc != 2) {
        err("Usage: %s <COMMAND>\n\nAvailable commands:", argv[0]);
        for (int32_t i = 0; i < count; i++) {
            err("   %-3s - %s", cmds[i].key, cmds[i].description);
        }
        return 1;
    }

    for (int32_t i = 0; i < count; i++) {
        if (_stricmp(argv[1], cmds[i].key) == 0) {
            const char* msg = cmds[i].func();
            if (msg)
                log("%s", msg);
            else
                err("Unexpected behaviour");
            return 0;
        }
    }

    err("Unknown command");
    return 0;
}