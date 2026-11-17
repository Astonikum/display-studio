# Shared rendering and hardware emulation

Studio uses `mad::Display` from MadDisplay, the same components, layout, focus,
color mapping and motion as firmware. `renderFrame(timestamp, &work)` is an
explicit frame boundary for an external scheduler; `render(timestamp)` retains
its normal FPS gate. Optional work counters are also available on the board.

MadDisplay owns RGB565 and SSD1306 page packing / dirty-window planning in
`presentation.hpp`. Adapters provide storage, initialization and hardware I/O.
The Studio transport calls the same presenters and consumes their transfer
plans. It does not ask Adafruit GFX to draw controls. ILI9341 sends explicit
11-byte address windows, avoiding the vendor driver's address cache shared
between multiple displays. SSD1306 retries a full update after a failed write.

## Profiles

The hardware dropdown offers Full power and 33 boards, including all 32 requested
variants and the retained Raspberry Pi Zero 2 W. See [BOARDS.md](BOARDS.md) for
CPU, memory, voltage, radio and variant details. Full power removes board limits,
while retaining panel refresh time, target FPS and host refresh limits. Display
formats and physical adapter mappings are in [DISPLAYS.md](DISPLAYS.md).

Defaults: `assets/hardware.json`. A workspace `madstudio.hardware.json` replaces
that list; use the cube button / Reload profiles after editing. Full power is
always inserted automatically. IDs must be unique. Invalid overrides preserve
the last working list and show an error. Selection is saved in the local session,
independently of the display/design. Popup edits are temporary; JSON edits persist.

Each profile specifies a **single rendering core**, CPU frequency, RAM budget,
reserved RAM, I2C/SPI clock, and calibration coefficients. Extra cores do not
magically accelerate the single-threaded library. Stock bus clocks are connection
settings, not advertised maximum chip performance. SSD1306 I2C is capped at
400 kHz; tune SPI for the actual controller, wiring and driver.

## Throttling

The actual shared renderer produces the frame and counts raster spans, fill
pixels and individual points. The common presenter counts inspected pixels,
changed payload, commands, address/control bytes and packets. Device CPU time
is estimated from those counts and editable cycle costs, plus scene traversal. Packed mono fills are charged per byte rather than per
RGB pixel; span setup and individual bit edits are charged separately.
Bus time includes 9 clock bits per I2C byte (ACK included), or 8 per SPI byte,
and configurable packet/transaction overhead.

Rendering and transfer are serial and blocking, matching the current adapters.
A frame is displayed only after its estimated completion, including a separate
e-paper BUSY interval that does not occupy the SPI bus. Busy frames create
backpressure; the queue cannot grow without bound. Animations keep wall time
and skip intermediate samples when the device cannot keep up. Global pause
freezes animation, in-flight work and graph history. Display refresh is also
bounded by the host UI's refresh rate.

RAM includes the actual target-format frame sizes and an estimated scene plus
configured runtime/stack reserve. SSD1306 needs both canonical bits and native
page bytes. RGB565 needs two bytes per pixel, optionally another frame for exact
diffs. If two frames do not fit, Studio uses the same full-transfer strategy as
an adapter without history. If one frame plus scene/reserve does not fit,
simulation stops with a RAM-limit message. Host textures and editor data are
excluded. Scene object sizes, dynamic allocations and asset placement are
estimates; firmware memory layout/fragmentation can still reject an allocation.

## Graphs and accuracy

Six traces cover 20 seconds at 5 Hz: modeled CPU rendering/driver load, modeled
bus occupancy, estimated target RAM, completed FPS, planned wire throughput and
**measured host render time**. CPU and bus integrate overlap with each sampling
interval; byte rate follows transfer progress. They freeze on pause. Idle frames
can have zero traffic while still consuming rendering/comparison CPU.

This is workload-based simulation, **not instruction-accurate MCU emulation**.
The initial cycle coefficients are uncalibrated estimates, not board benchmark
results. Host milliseconds are never scaled into pretend MCU milliseconds.
Calibrate coefficients with `micros()` around rendering and transport on the
actual board/build, then save them in its JSON profile. CPU arithmetic before
raster calls, allocator behavior, RTOS/Wi-Fi contention, cache/PSRAM effects,
DMA overlap, OS scheduling, panel scanout/tearing and electrical bus errors are
not individually emulated. No watts, battery life, GPU percentage or device
telemetry is invented.

## Specification sources

Clock/SRAM presets refer to official documentation; reserves and operation costs
are explicit simulation assumptions:

- [Arduino Uno R3](https://docs.arduino.cc/hardware/uno-rev3/)
- [Arduino Mega 2560 datasheet](https://docs.arduino.cc/resources/datasheets/A000067-datasheet.pdf)
- [ESP32 datasheet](https://documentation.espressif.com/esp32_datasheet_en.html)
- [ESP32-S3 datasheet](https://documentation.espressif.com/esp32_s3_datasheet_en.pdf)
- [RP2040 specifications](https://www.raspberrypi.com/products/rp2040/specifications/)
- [Raspberry Pi Zero 2 W](https://www.raspberrypi.com/products/raspberry-pi-zero-2-w/)

`madstudio_hardware` tests compare Studio and physical-adapter mock pixels and
transfers for mono/RGB animation, validate profiles atomically, verify RAM
fallback/failure, bus arithmetic and timing/backpressure beyond 32-bit uptime.
