# fatfs-pico-sdio Test Suite

Automated tests for the SDIO driver and FatFs integration, targeting RP2350 hardware (Adafruit Fruit Jam).

## What It Tests

- **Destructive format**: `f_mkfs()` formats the card FAT32 before tests begin (clean slate)
- **API queries**: `sd_is_initialized`, `sd_get_bus_width`, `sd_get_sector_count`, `sd_get_csd_raw`
- **Single-block writes**: Create, read back, verify, delete
- **Large file writes**: 8KB pattern file (multi-sector, exercises CMD25)
- **Multi-block copy**: Copy via FatFs, verify copy matches
- **Directory operations**: mkdir, write in subdir, readdir, recursive delete
- **File overwrite**: Write, overwrite, verify new content
- **Stress**: 20 create/delete cycles
- **Large sequential writes**: 3 x 16KB files with distinct patterns
- **Large file with checksum**: 256KB file written in 16KB chunks, CRC32 verified, copied and re-verified (exercises many CMD25 batches)
- **Error handling**: Bad parameters return correct error codes
- **Deinit/reinit**: Full `sd_deinit()` + remount cycle

## Build

```bash
cmake --build build --target sd_card_tests
```

This produces `build/tests/sd_card_tests.uf2`.

## Run

1. Flash `sd_card_tests.uf2` to the RP2350 (drag to USB mass storage, or use `picotool`)
2. Connect UART (GPIO 44/45 at 115200 baud)
3. Watch test output on the serial console
4. Tests take ~30-60 seconds depending on the SD card (includes format + 256KB file writes)

## Verify on Host

After the tests complete, move the SD card to a Linux/macOS card reader:

```bash
python3 tests/verify_sd_tests.py /path/to/sd/mount
```

The script independently regenerates the expected data patterns and compares them against what the firmware wrote to the SD card. This catches issues that the on-device verification might miss (e.g., data written correctly to the SDIO controller but not actually persisted to flash).

## Output

### On UART

```
========================================
  fatfs-pico-sdio Test Suite
========================================

SD card mounted successfully
Formatting SD card (FAT32)...
Format complete

[API Queries]
  PASS: sd_is_initialized returns true
  PASS: sd_get_bus_width returns 4
  ...

========================================
  Results: 35 run, 35 passed, 0 failed
========================================
```

### On SD Card

The tests write to a `__test__/` directory on the SD card:

- `results.txt` — PASS/FAIL log matching UART output
- `manifest.txt` — file list with sizes, seeds, and CRC32s for verification
- `verify_small.bin` — 64-byte pattern file
- `verify_large.bin` — 8KB pattern file
- `verify_copy.bin` — copy of verify_large.bin
- `verify_multi_1.bin` through `verify_multi_3.bin` — 16KB pattern files
- `verify_big.bin` — 256KB pattern file
- `verify_big_copy.bin` — copy of verify_big.bin

## Data Pattern

Test files use a deterministic pattern seeded by byte offset:

```c
buf[i] = (uint8_t)((i * 7 + seed) ^ (i >> 8));
```

Both the firmware and the Python script generate patterns identically, enabling independent verification.
