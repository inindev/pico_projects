# SMPTE Color Bars Display with PicoDVI

This project demonstrates the use of the Raspberry Pi Pico (RP2040) to generate SMPTE color bars on an HDMI display using the PicoDVI library and an Adafruit HDMI sock. The project renders a test pattern with three rows of color bars, utilizing the DVI output capabilities of the Pico.

## Purpose
- Showcase DVI output using the PicoDVI library.
- Demonstrate RGB565 color rendering on an HDMI display.
- Provide a reusable example for video output projects.

## Hardware Requirements
- **Raspberry Pi Pico (RP2040)**: Tested with RP2040; Pico 2 (RP2350) may work but is untested.
- **Adafruit HDMI Sock (DVI-D)**: Provides DVI output for connecting to an HDMI display.
- **Jumper wires** for connecting the Pico to the HDMI sock.
- **Optional**: LED for status indication (connected to GPIO 21).

## Pin Connections
The project uses the `pico_sock_cfg` configuration from the PicoDVI library for DVI output. Below are the pin mappings for the Raspberry Pi Pico to the Adafruit HDMI sock:

| Pico Pin | Function         | HDMI Sock Pin |
|----------|------------------|---------------|
| GPIO 12  | DVI Data Channel 0+ | TMDS0+       |
| GPIO 13  | DVI Data Channel 0- | TMDS0-       |
| GPIO 14  | DVI Data Channel 1+ | TMDS1+       |
| GPIO 15  | DVI Data Channel 1- | TMDS1-       |
| GPIO 16  | DVI Data Channel 2+ | TMDS2+       |
| GPIO 17  | DVI Data Channel 2- | TMDS2-       |
| GPIO 18  | DVI Clock+       | TMDS CLK+    |
| GPIO 19  | DVI Clock-       | TMDS CLK-    |
| GPIO 21  | LED (optional)   | N/A          |
| GND      | Ground           | GND          |

**Note**: Ensure proper grounding between the Pico and HDMI sock. Verify pin assignments with the Adafruit HDMI sock documentation.

## Software Requirements
- **Pico SDK**: Version 2.0.0 or later.
- **PicoDVI Library**: Included in the project under `pico_dvi/`.
- **CMake** and **GCC for ARM** (`arm-none-eabi-gcc`).
- **Operating System**: Linux recommended (instructions assume Linux).

## Setup Instructions
1. **Clone the Repository**:
   ```bash
   git clone https://github.com/inindev/pico_projects.git
   cd pico_projects/smpte_color_bars
   ```

2. **Install Pico SDK**:
   ```bash
   cd ~
   git clone --recurse-submodules https://github.com/raspberrypi/pico-sdk.git
   export PICO_SDK_PATH=$HOME/pico-sdk
   ```

3. **Build the Project**:
   - Ensure `PICO_SDK_PATH` is set.
   - Set `PICO_BOARD` to `pico` in `CMakeLists.txt` for RP2040.
   - Run:
     ```bash
     mkdir build
     cd build
     cmake ..
     make
     ```

4. **Flash the Pico**:
   - Copy the generated `.uf2` file (e.g., `smpte_color_bars.uf2`) to the Pico’s USB mass storage device.

5. **Connect Hardware**:
   - Wire the Pico to the Adafruit HDMI sock as per the pin connections table.
   - Connect the HDMI sock to an HDMI-compatible display.
   - Power the Pico via USB.

6. **Run the Project**:
   - The Pico will output SMPTE color bars to the connected display.
   - The optional LED on GPIO 21 blinks every ~30 frames to indicate activity.

## Project Details
- **Video Mode**: Configurable in `main.c`. Default is 640x480@60Hz (320x240 framebuffer). Other modes (e.g., 800x600, 1280x720) can be enabled by uncommenting the desired `#define MODE_...` in `main.c`.
- **Color Bars**: Implements SMPTE ECR-1-1978 test pattern with three rows:
  - Top: Light gray, yellow, cyan, green, magenta, red, blue.
  - Middle: Blue, dark gray, magenta, dark gray, cyan, dark gray, light gray.
  - Bottom: Dark teal, white, dark purple, dark gray, black, dark gray, medium gray, dark gray.
- **Performance**: Uses dual-core rendering (Core 0 and Core 1) for efficient scanline generation and TMDS encoding.

## Usage
- Connect the Pico to the HDMI sock and display.
- Power on the Pico to display the color bars.
- Modify `main.c` to experiment with different resolutions or color patterns.

## Troubleshooting
- **No Display Output**: Check pin connections, ensure the display supports the selected resolution, and verify the HDMI sock is functional.
- **LED Not Blinking**: Confirm GPIO 21 is correctly wired (if used).
- **Build Errors**: Ensure Pico SDK and PicoDVI library paths are correct in `CMakeLists.txt`.

## License
This project is licensed under the MIT License. See the [LICENSE](../../LICENSE) file for details.