# Package split and compatibility verification

## Package boundaries

- `display-lib/src`: UI tree, layout, drawing, colors, framebuffer storage and the abstract presentation hook. No hardware profiles, controller commands, bus implementations or vendor includes.
- `adapter-common-lib`: physical panel descriptions, dirty-region planning and e-paper refresh planning. No vendor dependency.
- Each of the six adapter packages initializes the actual upstream driver, owns or attaches pixel buffers, and sends the rendered pixels through I2C/SPI. U8g2 and Arduino_GFX remain family adapters because their vendor interfaces are shared across controllers.
- Studio consumes the same UI core and the shared physical profiles/refresh policy. Custom profiles intentionally require an application-provided driver.

The `MadDisplay/oled096.hpp` compatibility include now belongs to `ssd1306-lib`. Hardware test stubs moved out of the UI core and into the matching adapter's `tests/stubs`; production library paths contain no stubs.

## Checks performed on 2026-11-17

| Check | Result |
|---|---|
| Core-only CMake Debug build, no Arduino or vendor dependencies | 7/7 tests passed |
| Workspace Studio Debug build | 23/23 tests passed |
| Standalone Studio source copy, dependencies fetched from public GitHub tags | Build passed, 30/30 tests passed (includes 7 core tests) |
| Simulator `--smoke` | Passed |
| Real-library ESP32 integration compile | Passed for all six adapters, all color/mono factory branches, all nine e-paper template types |
| Six runnable adapter examples | Passed: SSD1306/U8g2 on Mega 2560; ILI9341/SSD1327/Arduino_GFX/GxEPD2 on ESP32 |
| Fresh external PlatformIO consumer, only SSD1306 Git URL in lib_deps | Dependencies resolved automatically; Mega firmware built |
| Arduino CLI ZIP installation: core + adapter support + SSD1306 | Installed; Mega example compiled |

The standalone Studio check reused a local Dear ImGui source cache; all MadDisplay package dependencies were downloaded from their public repositories. The core package uses Arduino discovery headers (`MadDisplay.h`) in sketches. Other packages expose corresponding top-level `.h` wrappers; namespaced `.hpp` headers remain available after a package is discovered.

Vendor versions used by the integration compile: Arduino_GFX 1.6.9, U8g2 2.36.18, Adafruit SSD1327 1.0.4, GxEPD2 1.6.9, Adafruit ILI9341 1.7.0, Adafruit SSD1306 2.5.17, Adafruit GFX 1.13.0 and BusIO 1.17.4.

## Compatibility corrections

The direct SSD1306 adapter now accepts only the 128x64, 128x32 and 96x16 layouts configured by its vendor initializer. Other dimensions need a matching U8g2 driver; arbitrary page-aligned dimensions do not establish controller compatibility. A presentation attempt before initialization now reports failure instead of a successful transfer. The host test covers that result.

## Physical limits

No board or display was attached. Compilation establishes API/toolchain compatibility, not electrical or optical behavior. Module pinout, supply voltage, controller revision, offsets/inversion, real refresh timings, BUSY behavior and RAM/PSRAM allocation still require hardware validation. See DISPLAYS.md for exact panel/driver mappings and documented simulation approximations.

SPI vendor begin functions do not reliably identify disconnected or incorrect panels. A true begin result can mean only that initialization commands were issued and memory was allocated.

## Distribution

All packages have standalone public GitHub repositories and version tags. Embedded libraries include `library.json`, `library.properties`, `src` headers and runnable examples. PlatformIO installs from the repository URL; Arduino IDE installs GitHub ZIPs plus the dependencies listed in each README. They have not been submitted to either public library registry.
