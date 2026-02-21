#!/usr/bin/env python3
"""
Verify fatfs-pico-sdio test results from an SD card.

After running sd_card_tests firmware on the RP2350, move the SD card
to a Linux/macOS card reader and run:

    python3 verify_sd_tests.py /path/to/sd/mount

This independently regenerates the expected data patterns and compares
them against what the firmware wrote, providing end-to-end verification
that data survived the write -> card storage -> read-on-host pipeline.
"""

import sys
import os
import struct


def fill_pattern(size: int, seed: int) -> bytes:
    """Regenerate the same deterministic pattern the firmware writes."""
    buf = bytearray(size)
    for i in range(size):
        buf[i] = ((i * 7 + seed) ^ (i >> 8)) & 0xFF
    return bytes(buf)


def crc32(data: bytes) -> int:
    """CRC32 matching the firmware's implementation."""
    crc = 0xFFFFFFFF
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1))
    return crc ^ 0xFFFFFFFF


def main():
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} /path/to/sd/mount")
        print()
        print("Point this at the mount point of the SD card after running")
        print("the sd_card_tests firmware on the RP2350.")
        sys.exit(2)

    sd_root = sys.argv[1]

    if not os.path.isdir(sd_root):
        print(f"Error: {sd_root} is not a directory")
        sys.exit(2)

    test_dir = os.path.join(sd_root, "__test__")
    if not os.path.isdir(test_dir):
        # Try case-insensitive lookup
        for entry in os.listdir(sd_root):
            if entry.lower() == "__test__":
                test_dir = os.path.join(sd_root, entry)
                break
        if not os.path.isdir(test_dir):
            print(f"Error: __test__ directory not found in {sd_root}")
            print("Did the test firmware run successfully?")
            sys.exit(1)

    passes = 0
    failures = 0

    def check(name, ok, detail=""):
        nonlocal passes, failures
        if ok:
            passes += 1
            print(f"  PASS: {name}")
        else:
            failures += 1
            msg = f"  FAIL: {name}"
            if detail:
                msg += f" - {detail}"
            print(msg)

    # --- Check results.txt ---
    print("\n[Firmware Results]")
    results_path = os.path.join(test_dir, "results.txt")
    if not os.path.exists(results_path):
        # FAT filesystem may uppercase the name
        for f in os.listdir(test_dir):
            if f.lower() == "results.txt":
                results_path = os.path.join(test_dir, f)
                break

    if os.path.exists(results_path):
        with open(results_path, "r") as f:
            content = f.read()
        fail_count = content.count("FAIL:")
        pass_count = content.count("PASS:")
        check(f"results.txt has {pass_count} passes", True)
        check("no failures in results.txt", fail_count == 0,
              f"found {fail_count} failures" if fail_count else "")
        if fail_count:
            print("\n  Failed tests from firmware:")
            for line in content.splitlines():
                if "FAIL:" in line:
                    print(f"    {line.strip()}")
            print()
    else:
        check("results.txt exists", False, "file not found")

    # --- Check manifest and verify data files ---
    print("\n[Data File Verification]")
    manifest_path = os.path.join(test_dir, "manifest.txt")
    if not os.path.exists(manifest_path):
        for f in os.listdir(test_dir):
            if f.lower() == "manifest.txt":
                manifest_path = os.path.join(test_dir, f)
                break

    if not os.path.exists(manifest_path):
        check("manifest.txt exists", False, "file not found")
        print("\nCannot verify data files without manifest.")
    else:
        check("manifest.txt exists", True)

        with open(manifest_path, "r") as f:
            lines = f.readlines()

        for line in lines:
            line = line.strip()
            if not line or line.startswith("#"):
                continue

            parts = line.split()
            if len(parts) != 4:
                check(f"parse manifest line: {line}", False, "wrong format")
                continue

            relpath, size_str, seed_str, crc_str = parts
            expected_size = int(size_str)
            expected_seed = int(seed_str, 16)
            expected_crc = int(crc_str, 16)

            # Find the file (handle FAT case folding)
            filepath = os.path.join(sd_root, relpath)
            if not os.path.exists(filepath):
                # Try case-insensitive search
                parts_path = relpath.replace("\\", "/").split("/")
                current = sd_root
                found = True
                for part in parts_path:
                    if not os.path.isdir(current):
                        found = False
                        break
                    entries = {e.lower(): e for e in os.listdir(current)}
                    if part.lower() in entries:
                        current = os.path.join(current, entries[part.lower()])
                    else:
                        found = False
                        break
                if found:
                    filepath = current
                else:
                    check(f"{relpath} exists", False, "file not found")
                    continue

            fname = os.path.basename(relpath)

            # Check file exists and size
            actual_size = os.path.getsize(filepath)
            check(f"{fname} size={expected_size}", actual_size == expected_size,
                  f"got {actual_size}" if actual_size != expected_size else "")

            if actual_size != expected_size:
                continue

            # Read and verify
            with open(filepath, "rb") as f:
                data = f.read()

            # CRC check
            actual_crc = crc32(data)
            check(f"{fname} CRC32", actual_crc == expected_crc,
                  f"expected 0x{expected_crc:08X}, got 0x{actual_crc:08X}"
                  if actual_crc != expected_crc else "")

            # Pattern check (independent regeneration)
            expected_data = fill_pattern(expected_size, expected_seed)
            if data == expected_data:
                check(f"{fname} pattern match", True)
            else:
                # Find first mismatch
                for i in range(min(len(data), len(expected_data))):
                    if data[i] != expected_data[i]:
                        check(f"{fname} pattern match", False,
                              f"first mismatch at byte {i}: "
                              f"got 0x{data[i]:02X}, expected 0x{expected_data[i]:02X}")
                        break

    # --- Summary ---
    print()
    print("=" * 40)
    print(f"  Verification: {passes} passed, {failures} failed")
    print("=" * 40)

    if failures == 0:
        print("\nAll verifications passed.")
    else:
        print(f"\n{failures} VERIFICATION(S) FAILED.")

    sys.exit(0 if failures == 0 else 1)


if __name__ == "__main__":
    main()
