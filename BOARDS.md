# Board catalog

33 hardware boards (32 requested plus the retained Raspberry Pi Zero 2 W) and the
automatically inserted Full power mode. Values describe the named module variant.
Flash is storage, not framebuffer RAM. Rendering uses one core even on multicore chips.
CPU coefficients, runtime reserves and PSRAM penalty are estimates requiring board
calibration. SPI defaults are connection settings capped by the selected panel profile.

| ID | Board | CPU MHz | Architecture | Cores | SRAM KiB | PSRAM MiB | Flash MiB | Reserve KiB | Logic V | Radio |
|---|---|---:|---|---:|---:|---:|---:|---:|---:|---|
| `uno` | Arduino Uno R3 ATmega328P | 16 | AVR | 1 | 2 | 0 | 0.03125 | 0.5 | 5 | None |
| `mega` | Arduino Mega 2560 ATmega2560 | 16 | AVR | 1 | 8 | 0 | 0.25 | 1 | 5 | None |
| `esp32` | ESP32 DevKit V1 WROOM 32 | 240 | Xtensa LX6 | 2 | 520 | 0 | 4 | 128 | 3.3 | WiFi Bluetooth Classic BLE |
| `esp32-s3` | ESP32 S3 DevKitC 1 N8 | 240 | Xtensa LX7 | 2 | 512 | 0 | 8 | 128 | 3.3 | WiFi BLE |
| `rp2040` | Raspberry Pi Pico RP2040 | 133 | Cortex-M0+ | 2 | 264 | 0 | 2 | 16 | 3.3 | None |
| `rpi-zero2` | Raspberry Pi Zero 2 W | 1000 | Cortex-A53 | 4 | 524288 | 0 | 0 | 131072 | 3.3 | WiFi BLE |
| `nano` | Arduino Nano Classic ATmega328P | 16 | AVR | 1 | 2 | 0 | 0.03125 | 0.5 | 5 | None |
| `pro-mini-5v` | Arduino Pro Mini 5V ATmega328P | 16 | AVR | 1 | 2 | 0 | 0.03125 | 0.5 | 5 | None |
| `pro-mini-3v3` | Arduino Pro Mini 3.3V ATmega328P | 8 | AVR | 1 | 2 | 0 | 0.03125 | 0.5 | 3.3 | None |
| `leonardo` | Arduino Leonardo ATmega32U4 | 16 | AVR | 1 | 2.5 | 0 | 0.03125 | 0.75 | 5 | None |
| `micro` | Arduino Micro Pro Micro 5V ATmega32U4 | 16 | AVR | 1 | 2.5 | 0 | 0.03125 | 0.75 | 5 | None |
| `nano-every` | Arduino Nano Every ATmega4809 16MHz | 16 | AVR | 1 | 6 | 0 | 0.046875 | 1 | 5 | None |
| `uno-r4` | Arduino Uno R4 Minima RA4M1 | 48 | Cortex-M4F | 1 | 32 | 0 | 0.25 | 8 | 5 | None |
| `due` | Arduino Due SAM3X8E | 84 | Cortex-M3 | 1 | 96 | 0 | 0.5 | 12 | 3.3 | None |
| `nodemcu` | NodeMCU ESP8266 ESP 12E | 80 | Xtensa L106 | 1 | 80 | 0 | 4 | 40 | 3.3 | WiFi |
| `d1-mini` | LOLIN D1 Mini ESP8266 | 80 | Xtensa L106 | 1 | 80 | 0 | 4 | 40 | 3.3 | WiFi |
| `esp32-wrover` | ESP32 WROVER E 8MB PSRAM | 240 | Xtensa LX6 | 2 | 520 | 8 | 4 | 128 | 3.3 | WiFi Bluetooth Classic BLE |
| `s2-mini` | LOLIN S2 Mini ESP32 S2FN4R2 | 240 | Xtensa LX7 | 1 | 320 | 2 | 4 | 96 | 3.3 | WiFi |
| `esp32-s3-n8r8` | ESP32 S3 DevKitC 1 N8R8 | 240 | Xtensa LX7 | 2 | 512 | 8 | 8 | 128 | 3.3 | WiFi BLE |
| `esp32-c3` | ESP32 C3 SuperMini | 160 | RISC-V | 1 | 400 | 0 | 4 | 128 | 3.3 | WiFi BLE |
| `esp32-c6` | ESP32 C6 DevKitC 1 | 160 | RISC-V | 1 | 512 | 0 | 8 | 128 | 3.3 | WiFi 6 BLE 802.15.4 |
| `esp32-h2` | ESP32 H2 DevKitM 1 | 96 | RISC-V | 1 | 320 | 0 | 4 | 64 | 3.3 | BLE 802.15.4 |
| `pico-w` | Raspberry Pi Pico W RP2040 | 133 | Cortex-M0+ | 2 | 264 | 0 | 2 | 64 | 3.3 | WiFi BLE |
| `pico2` | Raspberry Pi Pico 2 RP2350 ARM | 150 | Cortex-M33 | 2 | 520 | 0 | 4 | 24 | 3.3 | None |
| `pico2-w` | Raspberry Pi Pico 2 W RP2350 ARM | 150 | Cortex-M33 | 2 | 520 | 0 | 4 | 72 | 3.3 | WiFi BLE |
| `blue-pill` | Blue Pill STM32F103C8T6 | 72 | Cortex-M3 | 1 | 20 | 0 | 0.0625 | 4 | 3.3 | None |
| `black-pill-f401` | Black Pill STM32F401CCU6 | 84 | Cortex-M4F | 1 | 64 | 0 | 0.25 | 8 | 3.3 | None |
| `black-pill-f411` | Black Pill STM32F411CEU6 | 100 | Cortex-M4F | 1 | 128 | 0 | 0.5 | 12 | 3.3 | None |
| `stm32f407` | STM32F407VGT6 Development Board | 168 | Cortex-M4F | 1 | 192 | 0 | 1 | 24 | 3.3 | None |
| `teensy40` | Teensy 4.0 iMXRT1062 | 600 | Cortex-M7 | 1 | 1024 | 0 | 2 | 64 | 3.3 | None |
| `teensy41` | Teensy 4.1 iMXRT1062 | 600 | Cortex-M7 | 1 | 1024 | 0 | 8 | 64 | 3.3 | None |
| `xiao-samd21` | Seeed XIAO SAMD21 | 48 | Cortex-M0+ | 1 | 32 | 0 | 0.25 | 8 | 3.3 | None |
| `xiao-nrf52840` | Seeed XIAO nRF52840 | 64 | Cortex-M4F | 1 | 256 | 0 | 1 | 64 | 3.3 | BLE |

## Variant notes

- **ESP32 DevKit V1 WROOM 32**: 520 KiB physical SRAM; reserve approximates SDK and non-heap regions.
- **Raspberry Pi Zero 2 W**: Linux baseline only; scheduler jitter and OS processes are not modeled.
- **Arduino Nano Every ATmega4809 16MHz**: Arduino megaAVR core default 16 MHz; chip supports 20 MHz with matching firmware settings.
- **NodeMCU ESP8266 ESP 12E**: 80 KiB data RAM budget, not total instruction plus data SRAM; heap depends on WiFi SDK. 160 MHz requires matching firmware setting.
- **LOLIN D1 Mini ESP8266**: 80 KiB data RAM budget, not total instruction plus data SRAM; heap depends on WiFi SDK. 160 MHz requires matching firmware setting.
- **Raspberry Pi Pico 2 RP2350 ARM**: ARM firmware baseline; Hazard3 RISC-V mode needs separate CPU calibration.
- **STM32F407VGT6 Development Board**: Includes 64 KiB CCM, inaccessible to DMA; the adapters use blocking transfers, not DMA.
- **Teensy 4.0 iMXRT1062**: RAM1/RAM2 placement is linker dependent; only a combined blocking-transfer budget is modeled.
- **Teensy 4.1 iMXRT1062**: RAM1/RAM2 placement is linker dependent; only a combined blocking-transfer budget is modeled.
- **Seeed XIAO nRF52840**: Additional external flash is not RAM.

ESP32 physical SRAM is not equivalent to a single allocatable Arduino heap. The
reserved budget approximates SDK allocations and inaccessible regions; firmware
linker maps, stack size and fragmentation can still prevent allocation. Radio/OS
activity is represented by reserve, not cycle-accurate scheduling. Raspberry Pi
Zero 2 W remains a rough Linux baseline; these Arduino adapters are not Linux drivers.

## Sources

- [Arduino hardware](https://docs.arduino.cc/hardware/)
- [Espressif SoC comparison](https://docs.espressif.com/projects/esp-idf/en/release-v5.0/esp32c2/hw-reference/chip-series-comparison.html)
- [LOLIN S2 Mini](https://www.wemos.cc/en/latest/s2/s2_mini.html)
- [Raspberry Pi microcontroller specifications](https://www.raspberrypi.com/documentation/microcontrollers/microcontroller-chips.html)
- [STM32F103C8](https://www.st.com/en/microcontrollers-microprocessors/stm32f103c8)
- [STM32F401CC](https://www.st.com/en/microcontrollers-microprocessors/stm32f401cc.html)
- [STM32F411CE](https://www.st.com/en/microcontrollers-microprocessors/stm32f411ce.html)
- [Teensy specifications](https://www.pjrc.com/teensy/techspecs.html)
- [Seeed XIAO specifications](https://files.seeedstudio.com/wiki/XIAO/Seeed-Studio-XIAO-Series-SOM-Datasheet.pdf)
