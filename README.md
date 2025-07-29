# Pico Projects

**Pico Projects** is a collection of educational tutorials demonstrating the capabilities of the Raspberry Pi Pico (RP2040) and Pico 2 (RP2350) microcontrollers. Each project focuses on a specific feature or peripheral, such as SD card access, GPIO control, or future additions like Ethernet or I2C. Written in C with the Pico SDK, these projects provide clear, well-documented examples for beginners and enthusiasts learning embedded programming.

## Purpose
- Showcase practical applications of the Pico/Pico 2.
- Teach embedded programming concepts (e.g., SPI, GPIO, USB stdio).
- Provide modular, reusable code for experimentation.
- Encourage community contributions to expand the collection.

## Repository Structure
The repository is organized into subdirectories, each containing a standalone project with its own source code, build configuration, and documentation:

- **pico_sd_card**: A command-line interface (CLI) to interact with an SD card via SPI, with commands like `ls`, `cat`, and `led` to manage files and control the onboard LED. Compatible with Pico (RP2040) and Pico 2 (RP2350).
- *More projects coming soon (e.g., pico_ethernet, pico_i2c_sensor)!*

Each project includes:
- Source files (e.g., `.c` files for main logic and hardware interfaces).
- `CMakeLists.txt` for building with the Pico SDK.
- `README.md` with hardware requirements, pin connections, build instructions, and usage examples.

## Getting Started
### Prerequisites
- **Hardware**: Raspberry Pi Pico (RP2040) or Pico 2 (RP2350), plus peripherals (e.g., SD card module for `pico_sd_card`).
- **Software**:
  - Pico SDK version 2.0.0 or later
  - CMake and GCC for ARM (e.g., `arm-none-eabi-gcc`).
  - Terminal emulator: `picocom` for interactive projects.
- **Operating System**: Linux recommended (instructions assume Linux).

### Setup
1. **Clone the Repository**:
   ```bash
   git clone https://github.com/inindev/pico_projects.git
   cd pico_projects
   ```

2. **Install Pico SDK**:
   - Ensure the Pico SDK is installed:
     ```bash
     cd ~
     git clone --recurse-submodules https://github.com/raspberrypi/pico-sdk.git
     export PICO_SDK_PATH=$HOME/pico-sdk
     ```

3. **Explore Projects**:
   - Navigate to a project directory (e.g., `pico_sd_card`).
   - Follow the project’s `README.md` for specific hardware setup, pin connections, build, and usage instructions.
   - Set `PICO_BOARD` in `CMakeLists.txt`:
     - `pico` for Raspberry Pi Pico (RP2040).
     - `pico2` for Raspberry Pi Pico 2 (RP2350).
     - `pico_w` for Pico with Wi-Fi (RP2040, if applicable).

## License
This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.
