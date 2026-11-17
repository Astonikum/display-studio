# Display catalog and physical adapters

The catalog has 36 requested physical panel variants, explicit SPI alternatives for
switchable I2C OLED modules, and four retained legacy/custom profiles (48 entries).
Names use spaces. Stable IDs are saved in `.mad` documents; old IDs still load.

## Physical mapping

Each adapter is a separate Arduino/PlatformIO repository; the core has no hardware dependencies.
Shared panel profiles and transfer planners live in `adapter-common-lib`. Drivers
and buses must outlive adapters. Board selection does not promise every screen
fits that board's RAM. See the adapter READMEs and working PlatformIO examples.

| Profile ID | Selector name | Controller / revision | Adapter |
|---|---|---|---|
| `ssd1306-128x64` | SSD1306 OLED 0.96 White 128x64 I2C | SSD1306 | MadU8g2 or MadSSD1306 |
| `ssd1306-blue-128x64` | SSD1306 OLED 0.96 Blue 128x64 I2C | SSD1306 | MadU8g2 or MadSSD1306 |
| `ssd1306-zoned-128x64` | SSD1306 OLED 0.96 Yellow Blue 128x64 I2C | SSD1306 | MadU8g2 or MadSSD1306 |
| `ssd1306-128x32` | SSD1306 OLED 0.91 White 128x32 I2C | SSD1306 | MadU8g2 or MadSSD1306 |
| `ssd1306-64x48` | SSD1306 OLED 0.66 White 64x48 I2C | SSD1306 | MadU8g2 |
| `sh1106-128x64` | SH1106 OLED 1.3 Blue 128x64 I2C | SH1106 | MadU8g2 |
| `ssd1309-128x64` | SSD1309 OLED 2.42 White 128x64 SPI | SSD1309 | MadU8g2 |
| `ssd1327-128x128` | SSD1327 OLED 1.5 Grayscale 128x128 I2C | SSD1327 | MadGrayOLED |
| `ssd1351-128x128` | SSD1351 RGB OLED 1.5 128x128 SPI | SSD1351 | MadArduinoGFX |
| `ssd1331-96x64` | SSD1331 RGB OLED 0.95 96x64 SPI | SSD1331 | MadArduinoGFX |
| `ssd1305-128x32` | SSD1305 OLED 2.23 White 128x32 SPI | SSD1305 | MadU8g2 |
| `st7735-80x160` | ST7735S TFT 0.96 80x160 SPI | ST7735S | MadArduinoGFX |
| `st7735-128x128` | ST7735 TFT 1.44 128x128 SPI | ST7735 | MadArduinoGFX |
| `st7735-128x160` | ST7735 TFT 1.8 128x160 SPI | ST7735 | MadArduinoGFX |
| `st7789-135x240` | ST7789 TFT 1.14 135x240 SPI | ST7789 | MadArduinoGFX |
| `st7789-240x240` | ST7789 TFT 1.3 240x240 SPI | ST7789 | MadArduinoGFX |
| `st7789-154-240x240` | ST7789 TFT 1.54 240x240 SPI | ST7789 | MadArduinoGFX |
| `st7789-172x320` | ST7789 TFT 1.47 172x320 SPI | ST7789 | MadArduinoGFX |
| `st7789-240x280` | ST7789 TFT 1.69 240x280 SPI | ST7789 | MadArduinoGFX |
| `st7789-170x320` | ST7789 TFT 1.9 170x320 SPI | ST7789 | MadArduinoGFX |
| `st7789-240x320` | ST7789 TFT 2.0 240x320 SPI | ST7789 | MadArduinoGFX |
| `ili9341-24-240x320` | ILI9341 TFT 2.4 240x320 SPI | ILI9341 | MadArduinoGFX |
| `ili9341-240x320` | ILI9341 TFT 2.8 240x320 SPI | ILI9341 | MadArduinoGFX |
| `ili9341-32-240x320` | ILI9341 TFT 3.2 240x320 SPI | ILI9341 | MadArduinoGFX |
| `gc9a01-240x240` | GC9A01 TFT 1.28 240x240 SPI | GC9A01 | MadArduinoGFX |
| `st7796-320x480` | ST7796S TFT 3.5 320x480 SPI | ST7796S | MadArduinoGFX |
| `ili9488-320x480` | ILI9488 TFT 3.5 320x480 SPI | ILI9488 | MadArduinoGFX |
| `epd-154-bw` | E Paper 1.54 Black White 200x200 SPI | SSD1681 GDEH0154D67 | MadEPaper |
| `epd-213-bw` | E Paper 2.13 Black White 250x122 SPI | SSD1680 GDEM0213B74 | MadEPaper |
| `epd-29-bw` | E Paper 2.9 Black White 296x128 SPI | SSD1680 GDEM029T94 | MadEPaper |
| `epd-42-bw` | E Paper 4.2 Black White 400x300 SPI | SSD1683 GDEY042T81 | MadEPaper |
| `epd-75-bw` | E Paper 7.5 Black White 800x480 SPI | UC8179 GDEY075T7 | MadEPaper |
| `epd-213-bwr` | E Paper 2.13 Black White Red 250x122 SPI | SSD1680 GDEY0213Z98 | MadEPaper |
| `epd-42-bwr` | E Paper 4.2 Black White Red 400x300 SPI | UC8276 GDEQ042Z21 | MadEPaper |
| `epd-565-7c` | E Paper 5.65 7 Colors 600x448 SPI | Waveshare ACeP 5.65 F | MadEPaper |
| `epd-73-6c` | E Paper 7.3 Spectra 6 800x480 SPI | GDEP073E01 Spectra 6 palette | MadEPaper |
| `ssd1306-128x64-spi` | SSD1306 OLED 0.96 White 128x64 SPI | SSD1306 | MadU8g2 |
| `ssd1306-blue-128x64-spi` | SSD1306 OLED 0.96 Blue 128x64 SPI | SSD1306 | MadU8g2 |
| `ssd1306-zoned-128x64-spi` | SSD1306 OLED 0.96 Yellow Blue 128x64 SPI | SSD1306 | MadU8g2 |
| `ssd1306-128x32-spi` | SSD1306 OLED 0.91 White 128x32 SPI | SSD1306 | MadU8g2 |
| `ssd1306-64x48-spi` | SSD1306 OLED 0.66 White 64x48 SPI | SSD1306 | MadU8g2 |
| `sh1106-128x64-spi` | SH1106 OLED 1.3 Blue 128x64 SPI | SH1106 | MadU8g2 |
| `ssd1327-128x128-spi` | SSD1327 OLED 1.5 Grayscale 128x128 SPI | SSD1327 | MadGrayOLED |
| `ssd1306-96x16-spi` | SSD1306 OLED 96x16 SPI | SSD1306 | MadU8g2 |
| `ssd1306-96x16` | SSD1306 OLED 96x16 I2C | SSD1306 | MadU8g2 |
| `ili9341-320x240` | ILI9341 TFT 2.8 320x240 SPI Landscape | ILI9341 | MadArduinoGFX or MadILI9341 |
| `palette-128x64` | Palette display 128x64 | Custom | Custom |
| `custom` | Custom display | Custom | Custom |

## E-paper driver types

Use the exact revision, not just the diagonal. These classes were compiled against
GxEPD2 1.6.9. `mad::EPaper<Driver,W,H,Bits>` requires a 16-row vendor page.

| Profile | Concrete Driver | begin rotation | Stored bits/pixel |
|---|---|---:|---:|
| `epd-154-bw` | `GxEPD2_BW<GxEPD2_154_D67,16>` | 0 | 1 |
| `epd-213-bw` | `GxEPD2_BW<GxEPD2_213_B74,16>` | 1 | 1 |
| `epd-29-bw` | `GxEPD2_BW<GxEPD2_290_T94,16>` | 1 | 1 |
| `epd-42-bw` | `GxEPD2_BW<GxEPD2_420_GDEY042T81,16>` | 0 | 1 |
| `epd-75-bw` | `GxEPD2_BW<GxEPD2_750_GDEY075T7,16>` | 0 | 1 |
| `epd-213-bwr` | `GxEPD2_3C<GxEPD2_213_Z98c,16>` | 1 | 2 |
| `epd-42-bwr` | `GxEPD2_3C<GxEPD2_420c_Z21,16>` | 0 | 2 |
| `epd-565-7c` | `GxEPD2_7C<GxEPD2_565c,16>` | 0 | 4 |
| `epd-73-6c` | `GxEPD2_7C<GxEPD2_730c_GDEP073E01,16>` | 0 | 4 |

## Simulation

- RGB TFT/OLED: RGB565 storage, row differential transfers, controller command
  overhead and physical bus limits. ILI9488 SPI consumes 24 wire bits/pixel.
- Mono OLED: one-bit logical storage, physical white/blue emission, fixed
  yellow/blue zones. Existing 128-wide SSD1306 I2C uses MadSSD1306's differential
  page planner; SPI and other mono controllers model U8g2 full-frame transfers.
- SSD1327: luminance quantization to 16 gray levels, four-bit native storage.
- GC9A01: round aperture. Rounded ST7789 variants have an approximate corner mask.
- E-paper: packed palettes, no refresh for unchanged images, aligned partial
  updates on fast monochrome revisions, cleaning after seven partial refreshes,
  paper tint, approximate residual ghosting and flashing during full refresh.
  Controller BUSY time is separate from SPI occupancy and remains active with
  Full power selected. Global pause freezes transfers and panel refresh progress.
- RAM estimates count canonical and native/history frames plus scene and vendor
  pages; PSRAM does not increase internal memory available to the scene. PSRAM
  framebuffer work uses an explicit estimated 1.5x CPU penalty.

Panel command counts outside the existing SSD1306/ILI9341 transports are estimates.
E-paper timings follow the chosen GxEPD2 revision's reference values, not measured
local hardware. Temperature, voltage, vendor waveform revisions, analog contrast,
OLED aging, LCD response/tearing and touch-controller input are not calibrated.
I2C/SPI are the catalog's simulated transports; parallel buses can be provided to
Arduino_GFX physically but do not yet have calibrated Studio transport profiles.

The 6-color GDEP073E01 profile limits the driver's palette to six spectral colors.
Other GDEP/ACeP variants may expose orange too. Gray/color gamut, corner radii and
paper tint are visual approximations. Vendor initialization and offsets come from
upstream examples; modules with different offsets need a matching driver constructor.

## Verification

`cmake --build /tmp/mad-studio-build -j4` and
`ctest --test-dir /tmp/mad-studio-build --output-on-failure` cover the catalog,
roundtrips, memory limits, formats and shared refresh policy. E-paper page output is
also checked with an in-memory paged driver for all four rotations.

`pio run -d tests/adapter_compile` (from this repository) compiles all TFT driver factories,
mono/grayscale adapters and all nine e-paper types with the real vendor libraries
for ESP32. This compile-only firmware has an empty setup and never touches GPIO.
Each adapter directory additionally has a runnable example and its own `platformio.ini`.
No panel or board was physically connected during these checks.

## Sources

- [Arduino_GFX source and module constructors](https://github.com/moononournation/Arduino_GFX)
- [U8g2 full-buffer driver constructors](https://github.com/olikraus/u8g2)
- [Adafruit SSD1327 driver](https://github.com/adafruit/Adafruit_SSD1327)
- [GxEPD2 panel definitions and reference refresh times](https://github.com/ZinggJM/GxEPD2)
- [Waveshare display specifications](https://www.waveshare.com/wiki/1.69inch_LCD_Module)

## Published packages

| Package | GitHub repository |
|---|---|
| Core UI | [display-lib](https://github.com/Astonikum/display-lib) |
| Shared adapter support | [adapter-common-lib](https://github.com/Astonikum/adapter-common-lib) |
| SSD1306 I2C differential | [ssd1306-lib](https://github.com/Astonikum/ssd1306-lib) |
| ILI9341 RGB565 differential | [ili9341-lib](https://github.com/Astonikum/ili9341-lib) |
| SSD1327 grayscale | [ssd1327-lib](https://github.com/Astonikum/ssd1327-lib) |
| Mono OLED family | [u8g2-lib](https://github.com/Astonikum/u8g2-lib) |
| Color TFT / OLED family | [arduino-gfx-lib](https://github.com/Astonikum/arduino-gfx-lib) |
| Exact e-paper revisions | [epaper-lib](https://github.com/Astonikum/epaper-lib) |

Controller identity and dimensions are necessary but do not establish module
compatibility. Match the listed bus, offsets, rotation and vendor driver revision.
The two Custom profiles have no built-in physical driver.
