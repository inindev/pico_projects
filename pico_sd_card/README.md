# Pico SD Card CLI

This project implements a command-line interface (CLI) for the Raspberry Pi Pico or Pico 2 to interact with an SD card via SPI. It demonstrates SPI communication, GPIO control, and the FatFs library for SD card operations. Users can list files, read file contents, and control the onboard LED using simple commands. This is part of the **Pico Projects** repository, designed for learning embedded programming.

## Features
- Commands: `help`, `led <on|off>`, `toggle`, `ls`, `ls -a`, `mount`, `umount`, `cat <filename>`
- Character-by-character input with Backspace/Delete support
- Supports both Raspberry Pi Pico (RP2040) and Pico 2 (RP2350)
- Uses FatFs for SD card file operations
- MIT License for open use and modification

## Hardware Requirements
- **Microcontroller**: Raspberry Pi Pico (RP2040) or Pico 2 (RP2350)
- **SD Card Module**: Any SPI-compatible SD card module (e.g., SparkFun MicroSD SPI Breakout)
- **SD Card**: Formatted as FAT32
- **Cables**: Jumper wires for SPI connections

### Pin Connections
Connect the SD card module to the Pico/Pico 2 using SPI1 (default pins):

| SD Card Module Pin | Pico/Pico 2 Pin    | GPIO   | Description |
|--------------------|--------------------|--------|-------------|
| VCC                | 3v3 (Pin 36)       | -      | 3.3v power  |
| GND                | GND (e.g., Pin 38) | -      | Ground      |
| CLK/SCK            | GP10 (Pin 14)      | GPIO10 | SPI Clock   |
| MISO               | GP12 (Pin 16)      | GPIO12 | SPI MISO    |
| MOSI               | GP11 (Pin 15)      | GPIO11 | SPI MOSI    |
| CS                 | GP13 (Pin 17)      | GPIO13 | Chip Select |

**Notes**:
- Format the SD card as FAT32 on a computer before use (e.g., `sudo mkfs.vfat -F 32 /dev/sdX` on Linux).

## Software Requirements
- **Pico SDK**: Version 2.0.0 or later
- **Toolchain**: CMake, GCC for ARM (e.g., `arm-none-eabi-gcc`)
- **Terminal Emulator**: `picocom` for CLI interaction
- **Operating System**: Linux recommended (instructions assume Linux)

## Build Instructions
1. **Clone the Repository**:
   ```bash
   git clone https://github.com/inindev/pico_projects.git
   cd pico_projects/pico_sd_card
   ```

2. **Set Up Pico SDK**:
   - Ensure the Pico SDK is installed.
     ```bash
     cd ~
     git clone --recurse-submodules https://github.com/raspberrypi/pico-sdk.git
     export PICO_SDK_PATH=$HOME/pico-sdk
     ```

3. **Configure Board**:
   - Edit `CMakeLists.txt` to set `PICO_BOARD`:
     - For Pico (RP2040): `set(PICO_BOARD pico)`
     - For Pico 2 (RP2350): `set(PICO_BOARD pico2)`
   - The project is configured for `pico` by default.

4. **Build**:
   ```bash
   mkdir build
   cd build
   cmake ..
   make
   ```
   This generates `pico_sd_card.uf2` in the `build` directory.

## Flash Instructions
1. Hold the BOOTSEL button on your Pico/Pico 2.
2. Connect the Pico/Pico 2 to your computer via USB.
3. Copy `pico_sd_card.uf2` to the Pico’s mass storage device:
   ```bash
   sudo mount /dev/sda1 /mnt
   sudo cp pico_sd_card.uf2 /mnt
   sudo umount /mnt
   ```
4. The Pico will reboot automatically.

## Usage
1. **Connect to the CLI**:
   - Use `picocom`:
     ```bash
     picocom /dev/ttyACM0 -b 115200
     ```

2. **Try Commands**:
   ```
   pico> help
   commands: help, led <on|off>, toggle, ls, ls -a, mount, umount, cat <filename>
   pico> led on
   led turned on
   pico> ls
   files in sd card:
   foo.txt
   bar.txt
   pico> cat foo.txt
   [contents of foo.txt]
   pico> toggle
   led toggled
   pico> umount
   sd card unmounted successfully
   pico> mount
   sd card initialized successfully
   sd card mounted successfully
   ```

## Files
- `sd_card_cli.c`: Main CLI implementation with command processing and SPI initialization.
- `sd_card_diskio.c`: FatFs disk I/O layer for SD card access.
- `CMakeLists.txt`: Build configuration for Pico/Pico 2.

## License
Licensed under the MIT License. See [LICENSE](../../LICENSE) in the repository root.
