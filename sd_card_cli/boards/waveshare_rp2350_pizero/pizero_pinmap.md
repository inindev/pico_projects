**Waveshare RP2350-PiZero Pinmap:**

| GPIO | Function | Notes |
|---|---|---|
| GP0 | ID_SDA | 40-pin header |
| GP1 | ID_SCL | 40-pin header |
| GP2 | I2C SDA | 40-pin header |
| GP3 | I2C SCL | 40-pin header |
| GP4 | UART TX | 40-pin header |
| GP5 | UART RX | 40-pin header |
| GP6 | External I/O | 40-pin header |
| GP7 | SPI CE1 | 40-pin header |
| GP8 | SPI CE0 | 40-pin header |
| GP9 | External I/O | 40-pin header |
| GP10 | SPI_SCLK | 40-pin header |
| GP11 | SPI_MOSI | 40-pin header |
| GP12 | SPI_MISO | 40-pin header |
| GP13 | External I/O | 40-pin header |
| GP14 | External I/O | 40-pin header |
| GP15 | External I/O | 40-pin header |
| GP16 | External I/O | 40-pin header |
| GP17 | External I/O | 40-pin header |
| GP18 | External I/O | 40-pin header |
| GP19 | External I/O | 40-pin header |
| GP20 | External I/O | 40-pin header |
| GP21 | External I/O | 40-pin header |
| GP22 | External I/O | 40-pin header |
| GP23 | External I/O | 40-pin header |
| GP24 | External I/O | 40-pin header |
| GP25 | External I/O | 40-pin header |
| GP26 | External I/O | 40-pin header |
| GP27 | External I/O | 40-pin header |
| GP28 | PIO-USB D+ | J2 USB host, 22R series |
| GP29 | PIO-USB D- | J2 USB host, 22R series |
| GP30 | SDIO_SCK / SD_SCK | SD card |
| GP31 | SDIO_CMD / SD_MOSI | SD card |
| GP32 | DVI_D2_P | HDMI connector |
| GP33 | DVI_D2_N | HDMI connector |
| GP34 | DVI_D1_P | HDMI connector |
| GP35 | DVI_D1_N | HDMI connector |
| GP36 | DVI_D0_P | HDMI connector |
| GP37 | DVI_D0_N | HDMI connector |
| GP38 | DVI_CLK_P | HDMI connector |
| GP39 | DVI_CLK_N | HDMI connector |
| GP40 | SDIO_D0 / SD_MISO / ADC0 | SD card |
| GP41 | SDIO_D1 / ADC1 | SD card |
| GP42 | SDIO_D2 / ADC2 | SD card |
| GP43 | SDIO_D3 / SD_CS / ADC3 | SD card |
| GP44 | DVI_SDA / ADC4 | HDMI DDC I2C |
| GP45 | DVI_SCL / ADC5 | HDMI DDC I2C |
| GP46 | DVI_CEC / ADC6 | HDMI CEC |
| GP47 | PSRAM_CS / ADC7 | PSRAM chip select |

**Other notes:**
- **No GPIO-controlled LED** — LED1 is a power indicator (3V3 → R13 → LED → GND)
- **Key1** = USB_BOOT (QSPI_SS), **Key2** = RUN (reset)
- **Flash**: W25Q128JVSI (16MB)
- **PSRAM pad**: reserved, CS on GPIO47
- **RP2350B** (not RP2350A)
- UART TX/RX on GPIO4/5 → UART1 (not UART0)
- SWCLK/SWDIO on dedicated pins, exposed on H1 (Header 3) with 100R series resistors

