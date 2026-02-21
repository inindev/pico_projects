// sd_card_cli/sd_card_cli.c
//
// command-line interface for adafruit fruit jam (rp2350b) to interact with an sd card
// via sdio. supports commands to list files, read/write file contents, create directories,
// delete files, and control the onboard led. input is processed character by character
// with backspace/delete and command history support.
// license: mit (see license file in repository root).

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "ff.h"          // fatfs filesystem library
#include "diskio.h"      // fatfs disk i/o
#include "sd_card.h"     // carlk3 sd card driver
#include "hw_config.h"   // hw_config: sd_get_by_num()
#include "f_util.h"      // FRESULT_str()

// buffer for reading commands
#define MAX_COMMAND_LENGTH 128
static char command_buffer[MAX_COMMAND_LENGTH];

// command history (circular ring buffer)
#define HISTORY_SIZE 10
static char history[HISTORY_SIZE][MAX_COMMAND_LENGTH];
static int history_count = 0;
static int history_head = 0;

// fatfs variables
static FATFS fs;
static FIL fil;
static bool fs_mounted = false;  // track sd card mount status

// command function pointer type
typedef int (*command_func_t)(const char *args);

// command table structure
typedef struct {
    const char *name;
    command_func_t func;
} command_t;

// forward declarations of command functions
int cmd_help(const char *);
int cmd_led(const char *args);
int cmd_ls(const char *args);
int cmd_mount(const char *);
int cmd_umount(const char *);
int cmd_cat(const char *args);
int cmd_write(const char *args);
int cmd_rm(const char *args);
int cmd_mkdir(const char *args);
int cmd_info(const char *);
int cmd_cp(const char *args);
int cmd_csd(const char *);
int cmd_cls(const char *);
int cmd_format(const char *args);

// command table
static const command_t commands[] = {
    {"help", cmd_help},
    {"led", cmd_led},
    {"ls", cmd_ls},
    {"mount", cmd_mount},
    {"umount", cmd_umount},
    {"cls", cmd_cls},
    {"cat", cmd_cat},
    {"write", cmd_write},
    {"rm", cmd_rm},
    {"cp", cmd_cp},
    {"mkdir", cmd_mkdir},
    {"info", cmd_info},
    {"csd", cmd_csd},
    {"format", cmd_format},
    {NULL, NULL}  // sentinel to mark end of table
};

// command implementations

/**
 * Display available commands and hardware information.
 */
int cmd_help(const char *) {
    printf("\nAvailable commands:\n");
    printf("  help               - Show this help message\n");
    printf("  led <0|1|toggle>   - Control onboard LED\n");
    printf("  mount              - Mount SD card (SDIO mode)\n");
    printf("  umount             - Unmount SD card\n");
    printf("  cls                - Clear screen\n");
    printf("  ls [path]          - List files in directory (default: root)\n");
    printf("  cat <file>         - Display file contents\n");
    printf("  write <file> <val> - Write text to file\n");
    printf("  cp <src> <dst>     - Copy a file\n");
    printf("  rm <file>          - Delete file (rm -r for directories)\n");
    printf("  mkdir <dir>        - Create directory\n");
    printf("  info               - Show SD card capacity\n");
    printf("  csd                - Dump raw CSD register (debug)\n");
    printf("  format yes         - Format SD card as FAT32 (DESTRUCTIVE)\n");
    printf("\n");
    return 0;
}

/**
 * Control onboard LED (active-low on Fruit Jam).
 * Usage: led 0 (off), led 1 (on), led toggle
 */
int cmd_led(const char *args) {
#ifdef PICO_DEFAULT_LED_PIN
    if (!args || *args == '\0') {
        printf("Usage: led <0|1|toggle>\n");
        return 1;
    }
    if (strcmp(args, "1") == 0 || strcmp(args, "on") == 0) {
        gpio_put(PICO_DEFAULT_LED_PIN, 0);
        printf("LED on\n");
        return 0;
    } else if (strcmp(args, "0") == 0 || strcmp(args, "off") == 0) {
        gpio_put(PICO_DEFAULT_LED_PIN, 1);
        printf("LED off\n");
        return 0;
    } else if (strcmp(args, "toggle") == 0) {
        gpio_put(PICO_DEFAULT_LED_PIN, !gpio_get(PICO_DEFAULT_LED_PIN));
        printf("LED toggled\n");
        return 0;
    } else {
        printf("Usage: led <0|1|toggle>\n");
        return 1;
    }
#else
    (void)args;
    printf("No LED available on this board\n");
    return 1;
#endif
}

/**
 * List files and directories on SD card.
 * Usage: ls [path]
 * Example: ls or ls subdir
 */
int cmd_ls(const char *args) {
    if (!fs_mounted) {
        printf("Error: SD card not mounted (use 'mount' first)\n");
        return 1;
    }

    FRESULT fr;
    // Build path with drive prefix
    char path[256] = "0:/";
    if (args && *args != '\0') {
        strncat(path, args, sizeof(path) - 4);
    }

    DIR dir;
    FILINFO fno;
    fr = f_opendir(&dir, path);
    if (fr != FR_OK) {
        printf("Error opening directory: %d\n", fr);
        switch (fr) {
            case FR_INVALID_NAME:
                printf("  Reason: Invalid path name\n");
                break;
            case FR_NO_PATH:
                printf("  Reason: Path not found\n");
                break;
            case FR_DISK_ERR:
                printf("  Reason: Disk I/O error\nCheck: Card inserted? Pins correct?\n");
                break;
            case FR_NOT_READY:
                printf("  Reason: Card not ready\nTry: Re-insert card or check voltage\n");
                break;
            default:
                printf("  Reason: Unknown error\n");
                break;
        }
        return 1;
    }

    printf("Directory listing: %s\n", path);
    bool has_entries = false;
    while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0]) {
        if (fno.fattrib & AM_DIR) {
            printf("  %-20s <DIR>\n", fno.fname);
        } else {
            printf("  %-20s %lu\n", fno.fname, (unsigned long)fno.fsize);
        }
        has_entries = true;
    }
    if (!has_entries) {
        printf("  (empty)\n");
    }
    f_closedir(&dir);
    return 0;
}

/**
 * Mount SD card filesystem.
 * The library handles SDIO initialization automatically.
 */
int cmd_mount(const char *) {
    if (fs_mounted) {
        printf("SD card already mounted\n");
        return 0;
    }

    // Mount filesystem (library handles SDIO initialization automatically)
    FRESULT fr = f_mount(&fs, "0:", 1);  // "0:" = first SD card, 1 = mount now

    if (fr != FR_OK) {
        printf("Failed to mount SD card: ");
        switch (fr) {
            case FR_DISK_ERR:
                printf("Disk I/O error\nCheck: Card inserted? Pins correct?\n");
                break;
            case FR_NOT_READY:
                printf("Card not ready\nTry: Re-insert card or check voltage\n");
                break;
            case FR_NO_FILESYSTEM:
                printf("No FAT filesystem found\nFormat card as FAT32 on PC\n");
                break;
            default:
                printf("Error code %d (see ff.h for FRESULT codes)\n", fr);
                break;
        }
        return 1;
    }

    {
        sd_card_t *sd = sd_get_by_num(0);
        if (sd && sd->type == SD_IF_SDIO) {
            printf("SD card mounted (SDIO, 4-bit @ %u MHz)\n",
                   sd->sdio_if_p->baud_rate / 1000000);
        } else if (sd && sd->type == SD_IF_SPI) {
            printf("SD card mounted (SPI @ %u MHz)\n",
                   sd->spi_if_p->spi->baud_rate / 1000000);
        } else {
            printf("SD card mounted\n");
        }
    }
    fs_mounted = true;
    return 0;
}

/**
 * Unmount SD card filesystem.
 */
int cmd_umount(const char *) {
    if (!fs_mounted) {
        printf("SD card not mounted\n");
        return 0;
    }

    FRESULT fr = f_mount(NULL, "0:", 0);  // Unmount by passing NULL
    if (fr != FR_OK) {
        printf("Failed to unmount SD card: %s (%d)\n", FRESULT_str(fr), fr);
        return 1;
    }

    printf("SD card unmounted successfully\n");
    fs_mounted = false;
    return 0;
}

/**
 * Display file contents.
 * Usage: cat <filename>
 * Example: cat readme.txt
 */
int cmd_cat(const char *args) {
    if (!fs_mounted) {
        printf("Error: SD card not mounted (use 'mount' first)\n");
        return 1;
    }

    if (!args || *args == '\0') {
        printf("Error: No filename specified. Usage: cat <filename>\n");
        return 1;
    }

    // Build path with drive prefix
    char path[256];
    snprintf(path, sizeof(path), "0:/%s", args);

    FRESULT fr = f_open(&fil, path, FA_READ);
    if (fr != FR_OK) {
        printf("Error opening file '%s': %d\n", args, fr);
        switch (fr) {
            case FR_INVALID_NAME:
                printf("  Reason: Invalid filename\n");
                break;
            case FR_NO_FILE:
                printf("  Reason: File not found\n");
                break;
            case FR_DISK_ERR:
                printf("  Reason: Disk I/O error\n");
                break;
            case FR_NOT_READY:
                printf("  Reason: Card not ready\n");
                break;
            default:
                printf("  Reason: Unknown error\n");
                break;
        }
        return 1;
    }

    // Read and display file contents
    char buf[256];
    UINT br;
    while (f_read(&fil, buf, sizeof(buf) - 1, &br) == FR_OK && br > 0) {
        buf[br] = '\0';  // Null-terminate buffer
        printf("%s", buf);
    }
    f_close(&fil);
    printf("\n");
    return 0;
}

/**
 * Write text to a file on SD card.
 * Usage: write <filename> <content>
 * Example: write test.txt Hello World
 */
int cmd_write(const char *args) {
    if (!fs_mounted) {
        printf("Error: SD card not mounted (use 'mount' first)\n");
        return 1;
    }

    if (!args || *args == '\0') {
        printf("Usage: write <filename> <content>\n");
        printf("Example: write test.txt Hello World\n");
        return 1;
    }

    // Parse filename (up to first space)
    char filename[256] = "0:/";
    const char *p = args;
    int i = 3;  // Start after "0:/"
    while (*p && *p != ' ' && i < 255) {
        filename[i++] = *p++;
    }
    filename[i] = '\0';

    // Skip spaces to get content
    while (*p == ' ') p++;
    if (*p == '\0') {
        printf("Error: No content provided\n");
        return 1;
    }
    const char *content = p;

    // Open file for writing (create or overwrite)
    FRESULT fr = f_open(&fil, filename, FA_WRITE | FA_CREATE_ALWAYS);
    if (fr != FR_OK) {
        printf("Failed to open file for writing: %s (%d)\n", FRESULT_str(fr), fr);
        return 1;
    }

    // Write content
    UINT bytes_written;
    fr = f_write(&fil, content, strlen(content), &bytes_written);
    if (fr != FR_OK) {
        printf("Failed to write to file: %s (%d)\n", FRESULT_str(fr), fr);
        f_close(&fil);
        return 1;
    }

    // Close file
    f_close(&fil);

    printf("Wrote %u bytes to %s\n", bytes_written, filename + 3);  // Skip "0:/"
    return 0;
}

/**
 * Copy a file on the SD card.
 * Usage: cp <source> <destination>
 * Example: cp readme.txt backup.txt
 */
int cmd_cp(const char *args) {
    if (!fs_mounted) {
        printf("Error: SD card not mounted (use 'mount' first)\n");
        return 1;
    }

    if (!args || *args == '\0') {
        printf("Usage: cp <source> <destination>\n");
        return 1;
    }

    // Parse source and destination
    const char *p = args;
    while (*p && *p != ' ') p++;
    if (*p == '\0') {
        printf("Usage: cp <source> <destination>\n");
        return 1;
    }
    size_t src_len = p - args;

    while (*p == ' ') p++;
    if (*p == '\0') {
        printf("Usage: cp <source> <destination>\n");
        return 1;
    }

    static char src[256], dst[256];
    snprintf(src, sizeof(src), "0:/%.*s", (int)src_len, args);
    snprintf(dst, sizeof(dst), "0:/%s", p);

    // Open source file (static to avoid stack overflow - each FIL has a 512-byte buffer)
    static FIL fsrc, fdst;
    FRESULT fr = f_open(&fsrc, src, FA_READ);
    if (fr != FR_OK) {
        printf("Cannot open '%.*s': %s (%d)\n", (int)src_len, args, FRESULT_str(fr), fr);
        return 1;
    }

    // Open destination file
    fr = f_open(&fdst, dst, FA_WRITE | FA_CREATE_ALWAYS);
    if (fr != FR_OK) {
        printf("Cannot create '%s': %s (%d)\n", p, FRESULT_str(fr), fr);
        f_close(&fsrc);
        return 1;
    }

    // Copy in chunks (32-bit aligned buffer for SDIO DMA)
    static uint8_t cpbuf[4096] __attribute__((aligned(4)));
    UINT br, bw;
    FSIZE_t total = 0;
    while ((fr = f_read(&fsrc, cpbuf, sizeof(cpbuf), &br)) == FR_OK && br > 0) {
        fr = f_write(&fdst, cpbuf, br, &bw);
        if (fr != FR_OK || bw < br) {
            printf("Write error: %d\n", fr);
            f_close(&fsrc);
            f_close(&fdst);
            return 1;
        }
        total += bw;
    }

    f_close(&fsrc);
    f_close(&fdst);

    if (fr != FR_OK) {
        printf("Read error: %d\n", fr);
        return 1;
    }

    printf("Copied %lu bytes: %.*s -> %s\n", (unsigned long)total, (int)src_len, args, p);
    return 0;
}

/**
 * Recursively delete all contents of a directory, then the directory itself.
 * path must be a full path (e.g. "0:/dirname").
 */
static int rm_recursive(char *path, size_t path_size) {
    DIR dir;
    FILINFO fno;
    FRESULT res;

    res = f_opendir(&dir, path);
    if (res != FR_OK) {
        printf("Failed to open directory '%s': %s (%d)\n", path, FRESULT_str(res), res);
        return 1;
    }

    size_t pathlen = strlen(path);
    while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0]) {
        // Build child path
        snprintf(path + pathlen, path_size - pathlen, "/%s", fno.fname);

        if (fno.fattrib & AM_DIR) {
            if (rm_recursive(path, path_size) != 0) {
                f_closedir(&dir);
                return 1;
            }
        } else {
            res = f_unlink(path);
            if (res != FR_OK) {
                printf("Failed to delete '%s': %s (%d)\n", path, FRESULT_str(res), res);
                f_closedir(&dir);
                return 1;
            }
        }

        // Restore path for next iteration
        path[pathlen] = '\0';
    }
    f_closedir(&dir);

    // Remove the now-empty directory
    res = f_unlink(path);
    if (res != FR_OK) {
        printf("Failed to delete directory '%s': %s (%d)\n", path, FRESULT_str(res), res);
        return 1;
    }
    return 0;
}

/**
 * Delete a file or directory from SD card.
 * Usage: rm <file>       - delete a file
 *        rm -r <path>    - recursively delete a directory
 *        rm -rf <path>   - same as -r (no confirmation prompt exists)
 */
int cmd_rm(const char *args) {
    if (!fs_mounted) {
        printf("Error: SD card not mounted\n");
        return 1;
    }

    if (!args || *args == '\0') {
        printf("Usage: rm <file> or rm -r <dir>\n");
        return 1;
    }

    // Parse -r or -rf flag
    bool recursive = false;
    const char *target = args;
    if (args[0] == '-') {
        if (strcmp(args, "-r") == 0 || strcmp(args, "-rf") == 0) {
            // Flag without target
            printf("Usage: rm -r <dir>\n");
            return 1;
        }
        if (strncmp(args, "-r ", 3) == 0 || strncmp(args, "-rf ", 4) == 0) {
            recursive = true;
            target = strchr(args, ' ');
            while (*target == ' ') target++;
        }
    }

    // Build full path
    char path[256];
    snprintf(path, sizeof(path), "0:/%s", target);

    // Check if target is a directory
    FILINFO fno;
    FRESULT fr = f_stat(path, &fno);
    if (fr != FR_OK) {
        if (fr == FR_NO_FILE) {
            printf("Not found: %s\n", target);
        } else {
            printf("Error accessing '%s': %d\n", target, fr);
        }
        return 1;
    }

    if (fno.fattrib & AM_DIR) {
        if (!recursive) {
            printf("Cannot remove '%s': Is a directory (use rm -r)\n", target);
            return 1;
        }
        if (rm_recursive(path, sizeof(path)) != 0) {
            return 1;
        }
        printf("Deleted: %s/\n", target);
    } else {
        fr = f_unlink(path);
        if (fr != FR_OK) {
            printf("Failed to delete '%s': %s (%d)\n", target, FRESULT_str(fr), fr);
            return 1;
        }
        printf("Deleted: %s\n", target);
    }
    return 0;
}

/**
 * Create a directory on SD card.
 * Usage: mkdir <dirname>
 * Example: mkdir logs
 */
int cmd_mkdir(const char *args) {
    if (!fs_mounted) {
        printf("Error: SD card not mounted\n");
        return 1;
    }

    if (!args || *args == '\0') {
        printf("Usage: mkdir <dirname>\n");
        return 1;
    }

    // Build full path
    char path[256];
    snprintf(path, sizeof(path), "0:/%s", args);

    // Create directory
    FRESULT fr = f_mkdir(path);
    if (fr != FR_OK) {
        printf("Failed to create directory: %s (%d)\n", FRESULT_str(fr), fr);
        if (fr == FR_EXIST) {
            printf("Directory already exists: %s\n", args);
        } else if (fr == FR_DENIED) {
            printf("Access denied\n");
        }
        return 1;
    }

    printf("Created directory: %s\n", args);
    return 0;
}

/**
 * Show SD card capacity via GET_SECTOR_COUNT ioctl.
 */
int cmd_info(const char *) {
    if (!fs_mounted) {
        printf("Error: SD card not mounted (use 'mount' first)\n");
        return 1;
    }

    DWORD sector_count = 0;
    DRESULT dr = disk_ioctl(0, GET_SECTOR_COUNT, &sector_count);
    if (dr != RES_OK) {
        printf("Failed to get sector count: error %d\n", dr);
        return 1;
    }

    uint32_t mb = (uint32_t)(sector_count / 2048);  // sectors * 512 / 1024 / 1024
    printf("SD card capacity:\n");
    printf("  Sectors: %lu\n", (unsigned long)sector_count);
    printf("  Size:    %lu MB (%.1f GB)\n", (unsigned long)mb, (float)mb / 1024.0f);
    return 0;
}

/**
 * Hex-dump raw CSD register for debugging CSD parsing.
 * CSD is populated by the carlk3 library during card initialization.
 */
int cmd_csd(const char *) {
    if (!fs_mounted) {
        printf("Error: SD card not mounted (use 'mount' first)\n");
        return 1;
    }

    sd_card_t *sd_card_p = sd_get_by_num(0);
    if (!sd_card_p) {
        printf("Error: no SD card configured\n");
        return 1;
    }

    const uint8_t *csd = sd_card_p->state.CSD;

    printf("CSD register (16 bytes):\n ");
    for (int i = 0; i < 16; i++) {
        printf(" %02x", csd[i]);
    }
    printf("\n\n");

    uint8_t csd_ver = csd[0] >> 6;
    printf("CSD_STRUCTURE: %d (CSD version %d.0)\n", csd_ver, csd_ver + 1);

    if (csd_ver == 1) {
        uint32_t c_size = ((uint32_t)(csd[7] & 0x3f) << 16) |
                          ((uint32_t)csd[8] << 8) |
                          (uint32_t)csd[9];
        uint32_t sectors = (c_size + 1) * 1024;
        uint32_t mb = sectors / 2048;
        printf("C_SIZE: %lu -> %lu sectors (%lu MB)\n",
               (unsigned long)c_size, (unsigned long)sectors, (unsigned long)mb);
    } else if (csd_ver == 0) {
        printf("CSD v1 (SDSC card)\n");
    } else {
        printf("WARNING: unexpected CSD version %d - byte layout may be wrong\n", csd_ver);
        printf("Expected byte[0] upper 2 bits = 01 (0x4x) for SDHC\n");
    }
    return 0;
}

/**
 * Format SD card as FAT32.
 * Usage: format yes
 * Requires explicit "yes" argument since this destroys all data.
 */
int cmd_format(const char *args) {
    if (!args || strcmp(args, "yes") != 0) {
        printf("WARNING: This will erase ALL data on the SD card.\n");
        printf("Usage: format yes\n");
        return 1;
    }

    /* Unmount filesystem but keep card initialized */
    if (fs_mounted) {
        f_mount(NULL, "0:", 0);
        fs_mounted = false;
    }

    /* Register volume without mounting (opt=0) so f_mkfs can work.
     * f_mkfs calls disk_initialize() internally if needed. */
    FRESULT fr = f_mount(&fs, "0:", 0);
    if (fr != FR_OK) {
        printf("Failed to register volume: %s (%d)\n", FRESULT_str(fr), fr);
        return 1;
    }

    printf("Formatting SD card (FAT32)...\n");
    static uint8_t mkfs_work[4096];
    MKFS_PARM opt = { FM_FAT32, 0, 0, 0, 0 };
    fr = f_mkfs("0:", &opt, mkfs_work, sizeof(mkfs_work));
    if (fr != FR_OK) {
        printf("Format failed: %s (%d)\n", FRESULT_str(fr), fr);
        f_mount(NULL, "0:", 0);
        return 1;
    }

    /* Remount the freshly formatted volume */
    f_mount(NULL, "0:", 0);
    fr = f_mount(&fs, "0:", 1);
    if (fr != FR_OK) {
        printf("Format succeeded but remount failed: %s (%d)\n", FRESULT_str(fr), fr);
        printf("Try 'mount' manually.\n");
        return 1;
    }

    fs_mounted = true;
    printf("Format complete. SD card mounted.\n");
    return 0;
}


/**
 * Clear the serial terminal screen (VT100 escape sequence).
 */
int cmd_cls(const char *) {
    printf("\033[2J\033[H");
    return 0;
}

/**
 * Add a command to the history ring buffer.
 * Skips empty commands and duplicates of the most recent entry.
 */
static void history_add(const char *cmd) {
    if (cmd[0] == '\0') return;
    if (history_count > 0) {
        int prev = (history_head - 1 + HISTORY_SIZE) % HISTORY_SIZE;
        if (strcmp(history[prev], cmd) == 0) return;
    }
    snprintf(history[history_head], MAX_COMMAND_LENGTH, "%s", cmd);
    history_head = (history_head + 1) % HISTORY_SIZE;
    if (history_count < HISTORY_SIZE) history_count++;
}

/**
 * Replace the current input line on the terminal.
 * Erases displayed text, copies new_text into buf, prints it.
 */
static void replace_line(char *buf, int *idx, const char *new_text) {
    while (*idx > 0) {
        printf("\b \b");
        (*idx)--;
    }
    snprintf(buf, MAX_COMMAND_LENGTH, "%s", new_text);
    *idx = (int)strlen(buf);
    printf("%s", buf);
    stdio_flush();
}

/**
 * Main function - CLI loop.
 */
int main(void) {
    // Initialize stdio - now that PICO_DEFAULT_UART_BAUD_RATE is defined, this works!
    stdio_init_all();

    // Brief delay for UART to stabilize
    sleep_ms(100);

    // Initialize LED (if available on this board)
#ifdef PICO_DEFAULT_LED_PIN
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
#endif

    // Wait for serial connection
    printf("\nSD Card CLI ready\n");
    printf("Type 'help' for available commands\n");
    printf("Use 'mount' to access SD card\n\n");

    // CLI loop
    int buffer_index = 0;
    command_buffer[0] = '\0';

    while (1) {
        printf("pico> ");
        stdio_flush();

        // Read input character by character
        buffer_index = 0;
        int history_nav = -1;
        char saved_line[MAX_COMMAND_LENGTH] = "";
        enum { ST_NORMAL, ST_ESC, ST_CSI } esc_state = ST_NORMAL;

        while (buffer_index < MAX_COMMAND_LENGTH - 1) {
            int c = getchar_timeout_us(100000);  // Wait 100ms for input
            if (c == PICO_ERROR_TIMEOUT) {
                continue;
            }

            // VT100 escape sequence state machine
            if (esc_state == ST_ESC) {
                esc_state = (c == '[') ? ST_CSI : ST_NORMAL;
                continue;
            }
            if (esc_state == ST_CSI) {
                esc_state = ST_NORMAL;
                if (c == 'A' && history_count > 0) {  // Up arrow
                    int next = history_nav + 1;
                    if (next < history_count) {
                        if (history_nav == -1) {
                            command_buffer[buffer_index] = '\0';
                            snprintf(saved_line, MAX_COMMAND_LENGTH, "%s", command_buffer);
                        }
                        history_nav = next;
                        int idx = (history_head - 1 - history_nav + HISTORY_SIZE) % HISTORY_SIZE;
                        replace_line(command_buffer, &buffer_index, history[idx]);
                    }
                } else if (c == 'B') {  // Down arrow
                    if (history_nav > 0) {
                        history_nav--;
                        int idx = (history_head - 1 - history_nav + HISTORY_SIZE) % HISTORY_SIZE;
                        replace_line(command_buffer, &buffer_index, history[idx]);
                    } else if (history_nav == 0) {
                        history_nav = -1;
                        replace_line(command_buffer, &buffer_index, saved_line);
                    }
                }
                continue;
            }

            if (c == 0x1B) {  // ESC
                esc_state = ST_ESC;
                continue;
            }
            if (c == '\r' || c == '\n') {  // Enter
                command_buffer[buffer_index] = '\0';
                break;
            }
            if (c == 8 || c == 127) {  // Backspace / delete
                if (buffer_index > 0) {
                    buffer_index--;
                    printf("\b \b");
                    stdio_flush();
                }
                continue;
            }
            if (c >= 32 && c <= 126) {  // Printable characters
                command_buffer[buffer_index++] = (char)c;
                putchar(c);
                stdio_flush();
            }
        }

        // Process command or handle empty input
        if (buffer_index > 0) {
            printf("\n");
            history_add(command_buffer);

            // Parse command and arguments
            char *cmd = command_buffer;
            char *args = strchr(command_buffer, ' ');
            if (args) {
                *args = '\0';  // Null-terminate command
                args++;
                while (*args == ' ') args++;  // Skip leading spaces
                if (*args == '\0') args = NULL;  // No args if only spaces
            }

            // Look up command in table
            bool found = false;
            for (int i = 0; commands[i].name != NULL; i++) {
                if (strcmp(cmd, commands[i].name) == 0) {
                    commands[i].func(args);
                    found = true;
                    break;
                }
            }
            if (!found) {
                printf("Unknown command. Type 'help' for commands.\n");
            }
        } else {
            printf("\n");
            stdio_flush();
        }
    }

    return 0;
}
