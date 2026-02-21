/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Automated test suite for pico-fatfs-sd (fork of carlk3's library).
 * Runs on RP2350 hardware (SDIO or SPI), prints results to UART, writes
 * verification files to SD card for independent checking by verify_sd_tests.py.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "ff.h"
#include "diskio.h"
#include "sd_card.h"
#include "hw_config.h"
#include "f_util.h"

/* ---------- Test framework ---------- */

static int tests_run, tests_passed, tests_failed;
static FIL results_fil;
static bool results_file_open = false;

static void log_result(const char *status, const char *name)
{
    printf("  %s: %s\n", status, name);
    if (results_file_open) {
        UINT bw;
        char line[256];
        int n = snprintf(line, sizeof(line), "%s: %s\n", status, name);
        f_write(&results_fil, line, n, &bw);
    }
}

#define TEST_ASSERT(name, cond) do { \
    tests_run++; \
    if (cond) { tests_passed++; log_result("PASS", name); } \
    else { tests_failed++; log_result("FAIL", name); } \
} while(0)

/* ---------- Test data patterns ---------- */

/* Deterministic pattern seeded by offset — catches byte-swap and offset bugs */
static void fill_pattern(uint8_t *buf, int size, uint8_t seed)
{
    for (int i = 0; i < size; i++) {
        buf[i] = (uint8_t)((i * 7 + seed) ^ (i >> 8));
    }
}

static bool verify_pattern(const uint8_t *buf, int size, uint8_t seed)
{
    for (int i = 0; i < size; i++) {
        uint8_t expected = (uint8_t)((i * 7 + seed) ^ (i >> 8));
        if (buf[i] != expected) return false;
    }
    return true;
}

/* Simple CRC32 for manifest checksums */
static uint32_t crc32_update(uint32_t crc, const uint8_t *buf, int len)
{
    crc = ~crc;
    for (int i = 0; i < len; i++) {
        crc ^= buf[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
        }
    }
    return ~crc;
}

/* ---------- Shared buffers (DMA-aligned) ---------- */

static uint8_t wbuf[16384] __attribute__((aligned(4)));
static uint8_t rbuf[16384] __attribute__((aligned(4)));

/* ---------- Helper: write pattern file and record in manifest ---------- */

typedef struct {
    const char *name;
    int size;
    uint8_t seed;
    uint32_t crc32;
} manifest_entry_t;

#define MAX_MANIFEST 16
static manifest_entry_t manifest[MAX_MANIFEST];
static int manifest_count = 0;

static void manifest_add(const char *path, int size, uint8_t seed, uint32_t crc)
{
    if (manifest_count < MAX_MANIFEST) {
        manifest[manifest_count].name = path;
        manifest[manifest_count].size = size;
        manifest[manifest_count].seed = seed;
        manifest[manifest_count].crc32 = crc;
        manifest_count++;
    }
}

static bool write_pattern_file(const char *path, int size, uint8_t seed)
{
    fill_pattern(wbuf, size, seed);

    FIL fil;
    FRESULT fr = f_open(&fil, path, FA_CREATE_ALWAYS | FA_WRITE);
    if (fr != FR_OK) return false;

    UINT bw;
    fr = f_write(&fil, wbuf, size, &bw);
    f_close(&fil);

    if (fr != FR_OK || (int)bw != size) return false;

    /* Record in manifest for verification script */
    manifest_add(path, size, seed, crc32_update(0, wbuf, size));
    return true;
}

/* Write a large pattern file in chunks, computing CRC32 as we go.
 * This exercises multi-sector CMD25 writes across many batches. */
static bool write_large_pattern_file(const char *path, int total_size,
                                     uint8_t seed, uint32_t *out_crc)
{
    FIL fil;
    FRESULT fr = f_open(&fil, path, FA_CREATE_ALWAYS | FA_WRITE);
    if (fr != FR_OK) return false;

    uint32_t crc = 0;
    int written = 0;
    while (written < total_size) {
        int chunk = total_size - written;
        if (chunk > (int)sizeof(wbuf)) chunk = (int)sizeof(wbuf);

        /* Generate pattern for this chunk, offset-aware */
        for (int i = 0; i < chunk; i++) {
            int pos = written + i;
            wbuf[i] = (uint8_t)((pos * 7 + seed) ^ (pos >> 8));
        }

        UINT bw;
        fr = f_write(&fil, wbuf, chunk, &bw);
        if (fr != FR_OK || (int)bw != chunk) {
            f_close(&fil);
            return false;
        }

        crc = crc32_update(crc, wbuf, chunk);
        written += chunk;
    }

    f_close(&fil);
    if (out_crc) *out_crc = crc;
    return true;
}

/* Read back a large file in chunks, computing CRC32 and verifying pattern */
static bool verify_large_pattern_file(const char *path, int expected_size,
                                      uint8_t seed, uint32_t expected_crc)
{
    FIL fil;
    FRESULT fr = f_open(&fil, path, FA_READ);
    if (fr != FR_OK) return false;

    uint32_t crc = 0;
    int total_read = 0;
    bool pattern_ok = true;
    while (total_read < expected_size) {
        int chunk = expected_size - total_read;
        if (chunk > (int)sizeof(rbuf)) chunk = (int)sizeof(rbuf);

        UINT br;
        fr = f_read(&fil, rbuf, chunk, &br);
        if (fr != FR_OK || (int)br != chunk) {
            f_close(&fil);
            return false;
        }

        crc = crc32_update(crc, rbuf, chunk);

        /* Verify pattern */
        if (pattern_ok) {
            for (int i = 0; i < chunk; i++) {
                int pos = total_read + i;
                uint8_t expected = (uint8_t)((pos * 7 + seed) ^ (pos >> 8));
                if (rbuf[i] != expected) {
                    pattern_ok = false;
                    break;
                }
            }
        }

        total_read += chunk;
    }
    f_close(&fil);

    return pattern_ok && (crc == expected_crc);
}

static bool read_and_verify_pattern(const char *path, int expected_size, uint8_t seed)
{
    FIL fil;
    FRESULT fr = f_open(&fil, path, FA_READ);
    if (fr != FR_OK) return false;

    UINT br;
    fr = f_read(&fil, rbuf, expected_size, &br);
    f_close(&fil);

    if (fr != FR_OK || (int)br != expected_size) return false;
    return verify_pattern(rbuf, expected_size, seed);
}

/* ---------- Helper: recursive delete ---------- */

static int rm_recursive(char *path)
{
    DIR dir;
    FILINFO fno;
    FRESULT res = f_opendir(&dir, path);
    if (res != FR_OK) return -1;

    size_t pathlen = strlen(path);
    while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0]) {
        snprintf(path + pathlen, 256 - pathlen, "/%s", fno.fname);
        if (fno.fattrib & AM_DIR) {
            rm_recursive(path);
        } else {
            f_unlink(path);
        }
        path[pathlen] = '\0';
    }
    f_closedir(&dir);
    return (f_unlink(path) == FR_OK) ? 0 : -1;
}

/* ================================================================ */
/* Test categories                                                   */
/* ================================================================ */

static void test_api_queries(sd_card_t *sd_card_p)
{
    printf("\n[API Queries]\n");

    TEST_ASSERT("card initialized (m_Status clear)",
                !(sd_card_p->state.m_Status & STA_NOINIT));

    uint32_t sectors = sd_card_p->get_num_sectors(sd_card_p);
    TEST_ASSERT("sector count > 0", sectors > 0);
    printf("    (card has %lu sectors = %lu MB)\n",
           (unsigned long)sectors, (unsigned long)(sectors / 2048));

    const uint8_t *csd = sd_card_p->state.CSD;
    uint8_t csd_ver = csd[0] >> 6;
    TEST_ASSERT("CSD version is 0 or 1", csd_ver <= 1);
}

static void test_single_block_write(void)
{
    printf("\n[Single-Block Write]\n");

    const char *path = "0:/__test__/small.txt";
    const char *content = "Hello from sd_card_tests!";
    int len = strlen(content);

    FIL fil;
    UINT bw, br;

    FRESULT fr = f_open(&fil, path, FA_CREATE_ALWAYS | FA_WRITE);
    TEST_ASSERT("create small file", fr == FR_OK);
    if (fr != FR_OK) return;

    fr = f_write(&fil, content, len, &bw);
    TEST_ASSERT("write small file", fr == FR_OK && (int)bw == len);
    f_close(&fil);

    fr = f_open(&fil, path, FA_READ);
    TEST_ASSERT("reopen small file for read", fr == FR_OK);
    if (fr != FR_OK) return;

    char readback[64] = {0};
    fr = f_read(&fil, readback, sizeof(readback), &br);
    TEST_ASSERT("read back small file", fr == FR_OK && (int)br == len);
    TEST_ASSERT("content matches", memcmp(readback, content, len) == 0);
    f_close(&fil);

    fr = f_unlink(path);
    TEST_ASSERT("delete small file", fr == FR_OK);

    FILINFO fno;
    fr = f_stat(path, &fno);
    TEST_ASSERT("file gone after delete", fr == FR_NO_FILE);
}

static void test_large_file_write(void)
{
    printf("\n[Large File Write (>512 bytes)]\n");

    const char *path = "0:/__test__/verify_large.bin";
    int size = 8192;
    uint8_t seed = 0x5A;

    bool ok = write_pattern_file(path, size, seed);
    TEST_ASSERT("write 8KB pattern file", ok);

    ok = read_and_verify_pattern(path, size, seed);
    TEST_ASSERT("read back and verify 8KB pattern", ok);
}

static void test_multi_block_copy(void)
{
    printf("\n[Multi-Block Copy]\n");

    const char *src = "0:/__test__/verify_large.bin";
    const char *dst = "0:/__test__/verify_copy.bin";

    FIL fsrc, fdst;
    FRESULT fr = f_open(&fsrc, src, FA_READ);
    TEST_ASSERT("open source for copy", fr == FR_OK);
    if (fr != FR_OK) return;

    fr = f_open(&fdst, dst, FA_CREATE_ALWAYS | FA_WRITE);
    TEST_ASSERT("create destination for copy", fr == FR_OK);
    if (fr != FR_OK) { f_close(&fsrc); return; }

    static uint8_t cpbuf[4096] __attribute__((aligned(4)));
    UINT br, bw;
    FSIZE_t total = 0;
    bool copy_ok = true;
    while ((fr = f_read(&fsrc, cpbuf, sizeof(cpbuf), &br)) == FR_OK && br > 0) {
        fr = f_write(&fdst, cpbuf, br, &bw);
        if (fr != FR_OK || bw < br) { copy_ok = false; break; }
        total += bw;
    }
    f_close(&fsrc);
    f_close(&fdst);

    TEST_ASSERT("copy completes", copy_ok && total == 8192);

    /* Verify copy matches pattern */
    bool ok = read_and_verify_pattern(dst, 8192, 0x5A);
    TEST_ASSERT("copied file matches original pattern", ok);

    /* Record copy in manifest */
    {
        FIL fil;
        if (f_open(&fil, dst, FA_READ) == FR_OK) {
            UINT br2;
            f_read(&fil, rbuf, 8192, &br2);
            f_close(&fil);
            manifest_add(dst, 8192, 0x5A, crc32_update(0, rbuf, 8192));
        }
    }
}

static void test_directory_operations(void)
{
    printf("\n[Directory Operations]\n");

    const char *subdir = "0:/__test__/subdir";
    FRESULT fr = f_mkdir(subdir);
    TEST_ASSERT("mkdir subdir", fr == FR_OK);

    const char *subfile = "0:/__test__/subdir/file.txt";
    FIL fil;
    UINT bw;
    fr = f_open(&fil, subfile, FA_CREATE_ALWAYS | FA_WRITE);
    TEST_ASSERT("create file in subdir", fr == FR_OK);
    if (fr == FR_OK) {
        f_write(&fil, "subdir test", 11, &bw);
        f_close(&fil);
    }

    /* Verify file appears in directory listing */
    DIR dir;
    FILINFO fno;
    fr = f_opendir(&dir, subdir);
    TEST_ASSERT("opendir subdir", fr == FR_OK);
    bool found = false;
    if (fr == FR_OK) {
        while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0]) {
            if (strcmp(fno.fname, "file.txt") == 0 ||
                strcmp(fno.fname, "FILE.TXT") == 0) {
                found = true;
            }
        }
        f_closedir(&dir);
    }
    TEST_ASSERT("file appears in subdir listing", found);

    /* Recursive delete */
    char pathbuf[256];
    snprintf(pathbuf, sizeof(pathbuf), "%s", subdir);
    int rc = rm_recursive(pathbuf);
    TEST_ASSERT("rm -r subdir", rc == 0);

    fr = f_stat(subdir, &fno);
    TEST_ASSERT("subdir gone after rm -r", fr == FR_NO_FILE);
}

static void test_file_overwrite(void)
{
    printf("\n[File Overwrite]\n");

    const char *path = "0:/__test__/overwrite.txt";
    FIL fil;
    UINT bw, br;

    /* Write first content */
    FRESULT fr = f_open(&fil, path, FA_CREATE_ALWAYS | FA_WRITE);
    TEST_ASSERT("create file for overwrite", fr == FR_OK);
    if (fr == FR_OK) {
        f_write(&fil, "FIRST", 5, &bw);
        f_close(&fil);
    }

    /* Overwrite with different content */
    fr = f_open(&fil, path, FA_CREATE_ALWAYS | FA_WRITE);
    TEST_ASSERT("reopen for overwrite", fr == FR_OK);
    if (fr == FR_OK) {
        f_write(&fil, "SECOND", 6, &bw);
        f_close(&fil);
    }

    /* Verify new content */
    fr = f_open(&fil, path, FA_READ);
    TEST_ASSERT("reopen overwritten file", fr == FR_OK);
    if (fr == FR_OK) {
        char buf[16] = {0};
        f_read(&fil, buf, sizeof(buf), &br);
        f_close(&fil);
        TEST_ASSERT("overwritten content is SECOND", br == 6 && memcmp(buf, "SECOND", 6) == 0);
    }

    f_unlink(path);
}

static void test_stress_write_delete(void)
{
    printf("\n[Stress: Write/Delete x20]\n");

    bool all_ok = true;
    for (int i = 0; i < 20; i++) {
        char path[64];
        snprintf(path, sizeof(path), "0:/__test__/stress_%02d.tmp", i);

        FIL fil;
        UINT bw;
        FRESULT fr = f_open(&fil, path, FA_CREATE_ALWAYS | FA_WRITE);
        if (fr != FR_OK) { all_ok = false; break; }
        f_write(&fil, "stress", 6, &bw);
        f_close(&fil);

        fr = f_unlink(path);
        if (fr != FR_OK) { all_ok = false; break; }
    }
    TEST_ASSERT("20 write/delete cycles", all_ok);
}

static void test_large_sequential_writes(void)
{
    printf("\n[Large Sequential Writes (3 x 16KB)]\n");

    const char *paths[] = {
        "0:/__test__/verify_multi_1.bin",
        "0:/__test__/verify_multi_2.bin",
        "0:/__test__/verify_multi_3.bin",
    };
    uint8_t seeds[] = { 0xAA, 0xBB, 0xCC };
    int size = 16384;

    for (int i = 0; i < 3; i++) {
        bool ok = write_pattern_file(paths[i], size, seeds[i]);
        char name[64];
        snprintf(name, sizeof(name), "write verify_multi_%d.bin (16KB)", i + 1);
        TEST_ASSERT(name, ok);
    }

    /* Read back and verify all three */
    for (int i = 0; i < 3; i++) {
        bool ok = read_and_verify_pattern(paths[i], size, seeds[i]);
        char name[64];
        snprintf(name, sizeof(name), "verify verify_multi_%d.bin", i + 1);
        TEST_ASSERT(name, ok);
    }
}

static void test_small_pattern_file(void)
{
    printf("\n[Small Pattern File]\n");

    const char *path = "0:/__test__/verify_small.bin";
    int size = 64;
    uint8_t seed = 0x42;

    bool ok = write_pattern_file(path, size, seed);
    TEST_ASSERT("write 64-byte pattern file", ok);

    ok = read_and_verify_pattern(path, size, seed);
    TEST_ASSERT("read back and verify 64-byte pattern", ok);
}

static void test_large_file_with_checksum(void)
{
    printf("\n[Large File (256KB) with Checksum]\n");

    const char *path = "0:/__test__/verify_big.bin";
    const char *copy_path = "0:/__test__/verify_big_copy.bin";
    int total_size = 256 * 1024;  /* 256 KB = 512 sectors */
    uint8_t seed = 0xDE;

    /* Write 256KB in 16KB chunks */
    uint32_t write_crc = 0;
    bool ok = write_large_pattern_file(path, total_size, seed, &write_crc);
    TEST_ASSERT("write 256KB pattern file", ok);
    if (!ok) return;

    printf("    (CRC32: 0x%08lX)\n", (unsigned long)write_crc);

    /* Read back and verify pattern + CRC */
    ok = verify_large_pattern_file(path, total_size, seed, write_crc);
    TEST_ASSERT("verify 256KB pattern + CRC", ok);

    /* Copy via FatFs in 4KB chunks — exercises multi-block read+write path */
    {
        FIL fsrc, fdst;
        FRESULT fr = f_open(&fsrc, path, FA_READ);
        TEST_ASSERT("open 256KB source for copy", fr == FR_OK);
        if (fr != FR_OK) return;

        fr = f_open(&fdst, copy_path, FA_CREATE_ALWAYS | FA_WRITE);
        TEST_ASSERT("create 256KB copy destination", fr == FR_OK);
        if (fr != FR_OK) { f_close(&fsrc); return; }

        static uint8_t cpbuf[4096] __attribute__((aligned(4)));
        UINT br, bw;
        FSIZE_t total = 0;
        bool copy_ok = true;
        while ((fr = f_read(&fsrc, cpbuf, sizeof(cpbuf), &br)) == FR_OK && br > 0) {
            fr = f_write(&fdst, cpbuf, br, &bw);
            if (fr != FR_OK || bw < br) { copy_ok = false; break; }
            total += bw;
        }
        f_close(&fsrc);
        f_close(&fdst);

        TEST_ASSERT("copy 256KB file", copy_ok && (int)total == total_size);
    }

    /* Verify copy matches */
    ok = verify_large_pattern_file(copy_path, total_size, seed, write_crc);
    TEST_ASSERT("verify 256KB copy matches original", ok);

    /* Record both in manifest */
    manifest_add(path, total_size, seed, write_crc);
    manifest_add(copy_path, total_size, seed, write_crc);
}

static void test_deinit_reinit(sd_card_t *sd_card_p, FATFS *fs)
{
    printf("\n[Deinit/Reinit Cycle]\n");

    /* Write a marker file before deinit */
    const char *marker = "0:/__test__/reinit_marker.txt";
    FIL fil;
    UINT bw, br;
    FRESULT fr = f_open(&fil, marker, FA_CREATE_ALWAYS | FA_WRITE);
    if (fr == FR_OK) {
        f_write(&fil, "BEFORE", 6, &bw);
        f_close(&fil);
    }
    TEST_ASSERT("write marker before deinit", fr == FR_OK);

    /* Close the results file before unmount */
    if (results_file_open) {
        f_close(&results_fil);
        results_file_open = false;
    }

    /* Unmount */
    fr = f_mount(NULL, "0:", 0);
    TEST_ASSERT("unmount", fr == FR_OK);

    /* Deinit hardware via deinit function pointer */
    sd_card_p->deinit(sd_card_p);
    TEST_ASSERT("deinit sets STA_NOINIT",
                (sd_card_p->state.m_Status & STA_NOINIT) != 0);

    /* Reinit and remount — f_mount with opt=1 triggers disk_initialize()
       which calls sd_card_p->init() internally. */
    fr = f_mount(fs, "0:", 1);
    TEST_ASSERT("remount after reinit", fr == FR_OK);
    if (fr != FR_OK) {
        printf("  FATAL: cannot remount: %s (%d), skipping remaining reinit tests\n",
               FRESULT_str(fr), fr);
        return;
    }

    TEST_ASSERT("initialized after remount",
                !(sd_card_p->state.m_Status & STA_NOINIT));

    /* Reopen results file for append */
    fr = f_open(&results_fil, "0:/__test__/results.txt", FA_OPEN_APPEND | FA_WRITE);
    if (fr == FR_OK) {
        results_file_open = true;
    } else {
        printf("  WARNING: could not reopen results.txt: %s (%d)\n", FRESULT_str(fr), fr);
    }

    /* Verify marker file survived */
    fr = f_open(&fil, marker, FA_READ);
    TEST_ASSERT("open marker after reinit", fr == FR_OK);
    if (fr == FR_OK) {
        char buf[16] = {0};
        f_read(&fil, buf, sizeof(buf), &br);
        f_close(&fil);
        TEST_ASSERT("marker content intact", br == 6 && memcmp(buf, "BEFORE", 6) == 0);
    }

    /* Write new file after reinit */
    const char *post = "0:/__test__/reinit_post.txt";
    fr = f_open(&fil, post, FA_CREATE_ALWAYS | FA_WRITE);
    TEST_ASSERT("write after reinit", fr == FR_OK);
    if (fr == FR_OK) {
        f_write(&fil, "AFTER", 5, &bw);
        f_close(&fil);
    }

    /* Clean up */
    f_unlink(marker);
    f_unlink(post);
}

static void test_error_handling(void)
{
    printf("\n[Error Handling]\n");

    /* Open nonexistent file */
    FIL fil;
    FRESULT fr = f_open(&fil, "0:/__test__/nonexistent.xyz", FA_READ);
    TEST_ASSERT("open nonexistent file returns error", fr == FR_NO_FILE);
}

/* ================================================================ */
/* Manifest and results                                              */
/* ================================================================ */

static void write_manifest(void)
{
    FIL fil;
    FRESULT fr = f_open(&fil, "0:/__test__/manifest.txt", FA_CREATE_ALWAYS | FA_WRITE);
    if (fr != FR_OK) {
        printf("WARNING: could not write manifest.txt: %s (%d)\n", FRESULT_str(fr), fr);
        return;
    }

    UINT bw;
    char line[256];

    /* Header */
    int n = snprintf(line, sizeof(line), "# Test manifest - verify with verify_sd_tests.py\n");
    f_write(&fil, line, n, &bw);
    n = snprintf(line, sizeof(line), "# format: path size seed crc32\n");
    f_write(&fil, line, n, &bw);

    for (int i = 0; i < manifest_count; i++) {
        /* Strip the "0:/" prefix for the path stored on the SD card */
        const char *relpath = manifest[i].name;
        if (strncmp(relpath, "0:/", 3) == 0) relpath += 3;

        n = snprintf(line, sizeof(line), "%s %d 0x%02X 0x%08lX\n",
                     relpath, manifest[i].size, manifest[i].seed,
                     (unsigned long)manifest[i].crc32);
        f_write(&fil, line, n, &bw);
    }

    f_close(&fil);
    printf("\nManifest written with %d entries\n", manifest_count);
}

/* ================================================================ */
/* Main                                                              */
/* ================================================================ */

int main(void)
{
    stdio_init_all();
    sleep_ms(1000);  /* 1s startup delay */

    printf("\n");
    printf("===================================\n");
    printf("  FatFS SD Card Test Suite\n");
    printf("===================================\n");
    printf("\nPress ENTER to begin tests...");
    while (getchar() != '\r') tight_loop_contents();
    printf("\n");

    uint64_t t_start = time_us_64();

    /* Get the configured SD card object */
    sd_card_t *sd_card_p = sd_get_by_num(0);
    if (!sd_card_p) {
        printf("FATAL: sd_get_by_num(0) returned NULL\n");
        while (1) tight_loop_contents();
    }

    /* Card-detect GPIO diagnostics */
    if (sd_card_p->use_card_detect) {
        printf("Card-detect GPIO: pin %d\n", sd_card_p->card_detect_gpio);
        gpio_init(sd_card_p->card_detect_gpio);
        gpio_set_dir(sd_card_p->card_detect_gpio, GPIO_IN);
        gpio_pull_up(sd_card_p->card_detect_gpio);
        sleep_ms(10);
        bool raw = gpio_get(sd_card_p->card_detect_gpio);
        printf("Card-detect raw GPIO value: %d\n", raw);
        if (raw != (bool)sd_card_p->card_detected_true) {
            printf("\nWARNING: Card detect suggests no card (expected %d, got %d)\n",
                   sd_card_p->card_detected_true, raw);
            printf("Continuing anyway (card detect may not be wired)...\n");
        }
    } else {
        printf("Card-detect GPIO: not configured (skipping GPIO check)\n");
    }

    /* Format the card to start with a clean filesystem.
     * Register volume without mounting (opt=0) so f_mkfs can work
     * even when the card has no valid filesystem yet. */
    FATFS fs;
    {
        FRESULT fr2 = f_mount(&fs, "0:", 0);
        if (fr2 != FR_OK) {
            printf("FATAL: f_mount (register) failed: %s (%d)\n", FRESULT_str(fr2), fr2);
            while (1) tight_loop_contents();
        }
        printf("Formatting SD card (FAT32)...\n");
        static uint8_t mkfs_work[4096];
        MKFS_PARM opt = { FM_FAT32, 0, 0, 0, 0 };
        fr2 = f_mkfs("0:", &opt, mkfs_work, sizeof(mkfs_work));
        if (fr2 != FR_OK) {
            printf("FATAL: f_mkfs failed: %s (%d)\n", FRESULT_str(fr2), fr2);
            while (1) tight_loop_contents();
        }
        printf("Format complete\n");

        /* Unmount and remount to pick up the fresh filesystem.
         * No deinit needed — the card is already initialized from f_mkfs. */
        f_mount(NULL, "0:", 0);
    }
    FRESULT fr = f_mount(&fs, "0:", 1);
    if (fr != FR_OK) {
        printf("FATAL: f_mount failed: %s (%d)\n", FRESULT_str(fr), fr);
        while (1) tight_loop_contents();
    }
    printf("SD card mounted successfully\n");

    /* Clean up any previous test directory */
    {
        char pathbuf[256];
        snprintf(pathbuf, sizeof(pathbuf), "0:/__test__");
        rm_recursive(pathbuf);  /* OK if it doesn't exist */
    }

    /* Create test workspace */
    fr = f_mkdir("0:/__test__");
    if (fr != FR_OK) {
        printf("FATAL: cannot create __test__ directory: %s (%d)\n", FRESULT_str(fr), fr);
        while (1) tight_loop_contents();
    }

    /* Open results file */
    fr = f_open(&results_fil, "0:/__test__/results.txt", FA_CREATE_ALWAYS | FA_WRITE);
    if (fr == FR_OK) {
        results_file_open = true;
        UINT bw;
        const char *hdr = "FatFS SD card test results\n\n";
        f_write(&results_fil, hdr, strlen(hdr), &bw);
    }

    /* Run tests */
    test_api_queries(sd_card_p);
    test_single_block_write();
    test_small_pattern_file();
    test_large_file_write();
    test_multi_block_copy();
    test_directory_operations();
    test_file_overwrite();
    test_stress_write_delete();
    test_large_sequential_writes();
    test_large_file_with_checksum();
    test_error_handling();
    test_deinit_reinit(sd_card_p, &fs);

    /* Write summary to results file */
    if (results_file_open) {
        UINT bw;
        char summary[128];
        int n = snprintf(summary, sizeof(summary),
                         "\nSummary: %d run, %d passed, %d failed\n",
                         tests_run, tests_passed, tests_failed);
        f_write(&results_fil, summary, n, &bw);
        f_close(&results_fil);
        results_file_open = false;
    }

    /* Write manifest for verification script */
    write_manifest();

    /* Sync filesystem */
    f_mount(NULL, "0:", 0);

    /* Print summary */
    uint32_t elapsed_ms = (uint32_t)((time_us_64() - t_start) / 1000);
    printf("\n========================================\n");
    printf("  Results: %d run, %d passed, %d failed\n",
           tests_run, tests_passed, tests_failed);
    printf("  Elapsed: %lu.%03lu s\n",
           (unsigned long)(elapsed_ms / 1000),
           (unsigned long)(elapsed_ms % 1000));
    printf("========================================\n");

    if (tests_failed == 0) {
        printf("\nAll tests passed.\n");
    } else {
        printf("\n%d TEST(S) FAILED.\n", tests_failed);
    }

    printf("\nTest files written to __test__/ on SD card.\n");
    printf("Run: python3 tests/verify_sd_tests.py /path/to/sd/mount\n");
    printf("\nDone. Halting.\n");

    while (1) tight_loop_contents();
    return 0;
}
