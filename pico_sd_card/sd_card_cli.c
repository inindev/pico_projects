// pico_projects/pico_sd_card/sd_card_cli.c
//
// command-line interface for raspberry pi pico or pico 2 to interact with an sd card
// via spi. supports commands to list files, read file contents, and control the onboard
// led. input is processed character by character with backspace/delete support.
// part of the pico projects repository.
// license: mit (see license file in repository root).

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "ff.h"        // fatfs filesystem library
#include "diskio.h"    // fatfs disk i/o

// buffer for reading commands
#define MAX_COMMAND_LENGTH 64
char command_buffer[MAX_COMMAND_LENGTH];

// fatfs variables
FATFS fs;
FIL fil;
FRESULT fr;
bool fs_mounted = false;  // track sd card mount status

// sd card spi configuration
#define SPI_PORT spi1
#define PIN_MISO 12  // gp12 (miso)
#define PIN_CS   13  // gp13 (chip select)
#define PIN_MOSI 11  // gp11 (mosi)
#define PIN_SCK  10  // gp10 (clock)

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
int cmd_toggle(const char *);
int cmd_ls(const char *args);
int cmd_mount(const char *);
int cmd_umount(const char *);
int cmd_cat(const char *args);

// command table (supports help, led, toggle, ls, mount, umount, cat)
static const command_t commands[] = {
    {"help", cmd_help},
    {"led", cmd_led},
    {"toggle", cmd_toggle},
    {"ls", cmd_ls},
    {"mount", cmd_mount},
    {"umount", cmd_umount},
    {"cat", cmd_cat},
    {NULL, NULL}  // sentinel to mark end of table
};

// initialize spi for sd card communication
void init_spi(void) {
    spi_init(SPI_PORT, 400000);  // start at 400 khz for stable initialization
    spi_set_format(SPI_PORT, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);  // 8-bit, mode 0, msb first
    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);  // configure miso pin
    gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);   // configure clock pin
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);  // configure mosi pin
    gpio_init(PIN_CS);                           // initialize chip select pin
    gpio_set_dir(PIN_CS, GPIO_OUT);              // set chip select as output
    gpio_put(PIN_CS, 1);                         // set chip select high (inactive)
}

// command implementations
// display available commands
int cmd_help(const char *) {
    printf("commands: help, led <on|off>, toggle, ls, ls -a, mount, umount, cat <filename>\n");
    return 0;
}

// control onboard led (on/off/true/false)
int cmd_led(const char *args) {
    if (!args || *args == '\0') {
        printf("error: no argument specified. usage: led <on|off>\n");
        return 1;
    }
    if (strcmp(args, "on") == 0 || strcmp(args, "true") == 0) {
        gpio_put(PICO_DEFAULT_LED_PIN, 1);  // turn led on
        printf("led turned on\n");
        return 0;
    } else if (strcmp(args, "off") == 0 || strcmp(args, "false") == 0) {
        gpio_put(PICO_DEFAULT_LED_PIN, 0);  // turn led off
        printf("led turned off\n");
        return 0;
    } else {
        printf("error: invalid argument '%s'. usage: led <on|off>\n", args);
        return 1;
    }
}

// toggle onboard led state
int cmd_toggle(const char *) {
    gpio_put(PICO_DEFAULT_LED_PIN, !gpio_get(PICO_DEFAULT_LED_PIN));  // toggle led
    printf("led toggled\n");
    return 0;
}

// list files on sd card (ls -a shows hidden files)
int cmd_ls(const char *args) {
    if (!fs_mounted) {
        printf("error: sd card not mounted. use 'mount' command.\n");
        return 1;
    }
    bool show_hidden = (args && strcmp(args, "-a") == 0);  // check for -a flag
    DIR dir;
    FILINFO fno;
    fr = f_opendir(&dir, "/");  // open root directory
    if (fr != FR_OK) {
        printf("error listing files: %d\n", fr);
        switch (fr) {
            case FR_INVALID_DRIVE: printf("  reason: invalid drive number\n"); break;
            case FR_DISK_ERR: printf("  reason: disk i/o error\n"); break;
            case FR_NOT_READY: printf("  reason: disk not ready\n"); break;
            case FR_NO_FILESYSTEM: printf("  reason: no valid fat filesystem\n"); break;
            default: printf("  reason: unknown error\n"); break;
        }
        return 1;
    }
    printf("files in sd card:\n");
    bool has_files = false;
    while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0]) {
        if (show_hidden || fno.fname[0] != '.') {  // show all or non-hidden files
            printf("%s\n", fno.fname);
            has_files = true;
        }
    }
    if (!has_files) {
        printf("no %sfiles found\n", show_hidden ? "" : "visible ");
    }
    f_closedir(&dir);
    return 0;
}

// mount sd card filesystem
int cmd_mount(const char *) {
    if (fs_mounted) {
        printf("sd card already mounted\n");
        return 0;
    }
    fr = f_mount(&fs, "", 1);  // mount immediately
    if (fr != FR_OK) {
        printf("failed to mount sd card: %d\n", fr);
        switch (fr) {
            case FR_INVALID_DRIVE: printf("  reason: invalid drive number\n"); break;
            case FR_DISK_ERR: printf("  reason: disk i/o error\n"); break;
            case FR_NOT_READY: printf("  reason: disk not ready\n"); break;
            case FR_NO_FILESYSTEM: printf("  reason: no valid fat filesystem\n"); break;
            default: printf("  reason: unknown error\n"); break;
        }
        return 1;
    }
    printf("sd card mounted successfully\n");
    fs_mounted = true;
    return 0;
}

// unmount sd card filesystem
int cmd_umount(const char *) {
    if (!fs_mounted) {
        printf("sd card not mounted\n");
        return 0;
    }
    fr = f_mount(NULL, "", 0);  // unmount by passing null
    if (fr != FR_OK) {
        printf("failed to unmount sd card: %d\n", fr);
        switch (fr) {
            case FR_INVALID_DRIVE: printf("  reason: invalid drive number\n"); break;
            case FR_DISK_ERR: printf("  reason: disk i/o error\n"); break;
            case FR_NOT_READY: printf("  reason: disk not ready\n"); break;
            default: printf("  reason: unknown error\n"); break;
        }
        return 1;
    }
    printf("sd card unmounted successfully\n");
    fs_mounted = false;
    return 0;
}

// display file contents (root directory only)
int cmd_cat(const char *args) {
    if (!fs_mounted) {
        printf("error: sd card not mounted. use 'mount' command.\n");
        return 1;
    }
    if (!args || *args == '\0') {
        printf("error: no filename specified. usage: cat <filename>\n");
        return 1;
    }
    // prepend '/' for fatfs root directory path
    char full_path[64] = "/";
    strncat(full_path, args, sizeof(full_path) - 2);
    fr = f_open(&fil, full_path, FA_READ);  // open file for reading
    if (fr != FR_OK) {
        printf("error opening file '%s': %d\n", args, fr);
        switch (fr) {
            case FR_INVALID_NAME: printf("  reason: invalid filename\n"); break;
            case FR_NO_FILE: printf("  reason: file not found\n"); break;
            case FR_DISK_ERR: printf("  reason: disk i/o error\n"); break;
            case FR_NOT_READY: printf("  reason: disk not ready\n"); break;
            default: printf("  reason: unknown error\n"); break;
        }
        return 1;
    }
    char buf[128];
    UINT br;
    while (f_read(&fil, buf, sizeof(buf) - 1, &br) == FR_OK && br > 0) {
        buf[br] = '\0';  // null-terminate buffer
        printf("%s", buf);  // print file contents
    }
    f_close(&fil);  // close file
    printf("\n");  // newline after file contents
    return 0;
}

// main function for cli
int main(void) {
    stdio_init_all();  // initialize usb stdio

    // initialize onboard led (gp25)
    const uint LED_PIN = PICO_DEFAULT_LED_PIN;
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    // wait for usb connection (timeout after 5 seconds)
    uint32_t start_time = time_us_32();
    while (!stdio_usb_connected() && (time_us_32() - start_time) < 5000000) {
        sleep_ms(100);
    }

    if (!stdio_usb_connected()) {
        printf("failed to connect to usb. using uart instead.\n");
    } else {
        printf("raspberry pi pico cli. type 'help' for commands.\n");
    }

    // initialize spi for sd card
    init_spi();

    // mount sd card at startup
    cmd_mount(NULL);

    int buffer_index = 0;
    command_buffer[0] = '\0';

    while (1) {
        printf("pico> ");  // display prompt
        stdio_flush();     // ensure prompt is sent

        // read input character by character
        buffer_index = 0;
        while (buffer_index < MAX_COMMAND_LENGTH - 1) {
            int c = getchar_timeout_us(100000);  // wait 100ms for input
            if (c == PICO_ERROR_TIMEOUT) {
                continue;
            }
            if (c == '\r' || c == '\n') {  // handle enter
                command_buffer[buffer_index] = '\0';
                break;
            }
            if (c == 8 || c == 127) {  // handle backspace or delete
                if (buffer_index > 0) {
                    buffer_index--;
                    printf("\b \b");  // move cursor back, overwrite with space, move back
                    stdio_flush();
                }
                continue;
            }
            if (c >= 32 && c <= 126) {  // handle printable characters
                command_buffer[buffer_index++] = (char)c;
                putchar(c);
                stdio_flush();
            }
        }

        // process command or handle empty input
        if (buffer_index > 0) {
            printf("\n");
            // parse command and arguments
            char *cmd = command_buffer;
            char *args = strchr(command_buffer, ' ');
            if (args) {
                *args = '\0';  // null-terminate command
                args++;
                while (*args == ' ') args++;  // skip leading spaces
                if (*args == '\0') args = NULL;  // no args if only spaces
            }

            // look up command in table
            bool found = false;
            for (int i = 0; commands[i].name != NULL; i++) {
                if (strcmp(cmd, commands[i].name) == 0) {
                    commands[i].func(args);
                    found = true;
                    break;
                }
            }
            if (!found) {
                printf("unknown command. type 'help' for commands.\n");
            }
        } else if (buffer_index == 0) {
            printf("\n");  // print newline for empty input
            stdio_flush();
        }
    }

    return 0;
}
