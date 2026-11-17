cmake_minimum_required(VERSION 3.16)
project(MadStudio LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(cmake/Dependencies.cmake)
include_directories("${MAD_CORE}/src" "${MAD_ADAPTERS}/src")
FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG v1.91.9b-docking
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(imgui)

set(ROBOTO_MONO_FONT "${CMAKE_CURRENT_SOURCE_DIR}/assets/roboto-mono/RobotoMono-Regular.ttf")

find_package(OpenGL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)

add_library(madstudio_model
  src/json.cpp
  src/madui.cpp
  src/runtime.cpp
  src/hardware.cpp
  src/image_import.cpp
  src/unicode_text.cpp
  src/unicode_native.cpp
)
target_include_directories(madstudio_model PUBLIC include ${MAD_CORE}/src)
target_compile_definitions(madstudio_model PRIVATE
  MAD_PIXEL_FONT_PATH="${CMAKE_CURRENT_SOURCE_DIR}/assets/pixel-font/MadPixel.otf"
  MAD_PIXEL_UPPER_FONT_PATH="${CMAKE_CURRENT_SOURCE_DIR}/assets/pixel-font/unifont_upper-17.0.03.otf"
)
if(APPLE)
  target_link_libraries(madstudio_model PUBLIC "-framework CoreText" "-framework CoreGraphics" "-framework CoreFoundation")
endif()
target_include_directories(madstudio_model SYSTEM PRIVATE third_party)

add_executable(MadStudio
  src/main.cpp
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_demo.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl2.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
  ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp
)
if(APPLE)
  enable_language(OBJCXX)
  target_sources(MadStudio PRIVATE src/file_dialog.mm)
  target_link_libraries(MadStudio PRIVATE "-framework AppKit" "-framework UniformTypeIdentifiers")
else()
  target_sources(MadStudio PRIVATE src/file_dialog.cpp)
endif()

target_include_directories(MadStudio PRIVATE
  include ${MAD_CORE}/src ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends
  ${SDL2_INCLUDE_DIRS}
)
target_compile_options(MadStudio PRIVATE ${SDL2_CFLAGS_OTHER})
target_compile_definitions(MadStudio PRIVATE IMGUI_USE_WCHAR32)
target_compile_definitions(MadStudio PRIVATE
  MADDISPLAY_STUDIO_HARDWARE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/assets/hardware.json"
  MADDISPLAY_STUDIO_FONT_PATH="${ROBOTO_MONO_FONT}"
  MADDISPLAY_STUDIO_ICON_FONT_PATH="${CMAKE_CURRENT_SOURCE_DIR}/assets/phosphor/Phosphor.ttf"
)
target_link_directories(MadStudio PRIVATE ${SDL2_LIBRARY_DIRS})
target_link_libraries(MadStudio PRIVATE madstudio_model ${SDL2_LIBRARIES} OpenGL::GL)

add_executable(madui_test tests/madui_test.cpp)
target_link_libraries(madui_test PRIVATE madstudio_model)
target_include_directories(madui_test PRIVATE include ${MAD_CORE}/src)
enable_testing()
add_test(NAME madui_roundtrip COMMAND madui_test)

add_executable(maddisplay_core_test ${MAD_CORE}/tests/core_test.cpp)
target_include_directories(maddisplay_core_test PRIVATE ${MAD_CORE}/src)
add_test(NAME maddisplay_core COMMAND maddisplay_core_test)

add_executable(MadDisplaySimulator src/simulator.cpp)
target_include_directories(MadDisplaySimulator PRIVATE ${MAD_CORE}/examples/animation_showcase ${MAD_CORE}/src ${SDL2_INCLUDE_DIRS})
target_compile_options(MadDisplaySimulator PRIVATE ${SDL2_CFLAGS_OTHER})
target_link_directories(MadDisplaySimulator PRIVATE ${SDL2_LIBRARY_DIRS})
target_link_libraries(MadDisplaySimulator PRIVATE ${SDL2_LIBRARIES})

add_executable(maddisplay_showcase_test ${MAD_CORE}/tests/showcase_test.cpp)
target_include_directories(maddisplay_showcase_test PRIVATE ${MAD_CORE}/src)
add_test(NAME maddisplay_showcase COMMAND maddisplay_showcase_test)

add_executable(maddisplay_oled_test ${MAD_SSD1306}/tests/oled_test.cpp)
target_include_directories(maddisplay_oled_test PRIVATE ${MAD_SSD1306}/tests/stubs ${MAD_ILI9341}/tests/stubs ${MAD_CORE}/src ${MAD_SSD1306}/src)
add_test(NAME maddisplay_oled COMMAND maddisplay_oled_test)

add_executable(madstudio_runtime_test tests/runtime_test.cpp)
target_link_libraries(madstudio_runtime_test PRIVATE madstudio_model)
add_test(NAME madstudio_runtime COMMAND madstudio_runtime_test)

add_executable(madstudio_navigation_test tests/navigation_test.cpp)
target_link_libraries(madstudio_navigation_test PRIVATE madstudio_model)
add_test(NAME madstudio_navigation COMMAND madstudio_navigation_test)

add_executable(madstudio_control_focus_test tests/control_focus_test.cpp)
target_link_libraries(madstudio_control_focus_test PRIVATE madstudio_model)
add_test(NAME madstudio_control_focus COMMAND madstudio_control_focus_test)

add_executable(madstudio_graphics_test tests/graphics_test.cpp)
target_link_libraries(madstudio_graphics_test PRIVATE madstudio_model)
add_test(NAME madstudio_graphics COMMAND madstudio_graphics_test)

add_executable(madstudio_scroll_test tests/scroll_test.cpp)
target_link_libraries(madstudio_scroll_test PRIVATE madstudio_model)
add_test(NAME madstudio_scroll COMMAND madstudio_scroll_test)

add_executable(maddisplay_color_test ${MAD_CORE}/tests/color_test.cpp)
target_include_directories(maddisplay_color_test PRIVATE ${MAD_CORE}/src)
add_test(NAME maddisplay_color COMMAND maddisplay_color_test)

add_executable(maddisplay_ili9341_test ${MAD_ILI9341}/tests/ili9341_test.cpp)
target_include_directories(maddisplay_ili9341_test PRIVATE ${MAD_SSD1306}/tests/stubs ${MAD_ILI9341}/tests/stubs ${MAD_CORE}/src ${MAD_ILI9341}/src)
add_test(NAME maddisplay_ili9341 COMMAND maddisplay_ili9341_test)

add_executable(madstudio_color_test tests/color_test.cpp)
target_link_libraries(madstudio_color_test PRIVATE madstudio_model)
add_test(NAME madstudio_color COMMAND madstudio_color_test)

add_executable(maddisplay_arduino_macro_test ${MAD_CORE}/tests/arduino_macro_test.cpp)
target_include_directories(maddisplay_arduino_macro_test PRIVATE ${MAD_CORE}/src)
add_test(NAME maddisplay_arduino_macros COMMAND maddisplay_arduino_macro_test)

add_executable(maddisplay_benchmark EXCLUDE_FROM_ALL ${MAD_CORE}/tests/render_benchmark.cpp)
target_include_directories(maddisplay_benchmark PRIVATE ${MAD_CORE}/src)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(maddisplay_benchmark PRIVATE -O2)
endif()

add_executable(madstudio_hardware_test tests/hardware_test.cpp)
target_link_libraries(madstudio_hardware_test PRIVATE madstudio_model)
target_include_directories(madstudio_hardware_test PRIVATE ${MAD_SSD1306}/tests/stubs ${MAD_ILI9341}/tests/stubs ${MAD_SSD1306}/src ${MAD_ILI9341}/src)
target_compile_definitions(madstudio_hardware_test PRIVATE MAD_TEST_HARDWARE="${CMAKE_CURRENT_SOURCE_DIR}/assets/hardware.json")
add_test(NAME madstudio_hardware COMMAND madstudio_hardware_test)

add_executable(maddisplay_button_scale_test ${MAD_CORE}/tests/button_scale_test.cpp)
target_include_directories(maddisplay_button_scale_test PRIVATE ${MAD_CORE}/src)
add_test(NAME maddisplay_button_scale COMMAND maddisplay_button_scale_test)

add_executable(madstudio_grid_test tests/grid_test.cpp)
target_link_libraries(madstudio_grid_test PRIVATE madstudio_model)
add_test(NAME madstudio_grid COMMAND madstudio_grid_test)

add_executable(madstudio_image_import_test tests/image_import_test.cpp)
target_link_libraries(madstudio_image_import_test PRIVATE madstudio_model)
target_compile_definitions(madstudio_image_import_test PRIVATE MAD_IMAGE_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
add_test(NAME madstudio_image_import COMMAND madstudio_image_import_test)

add_executable(madstudio_composition_test tests/composition_test.cpp)
target_link_libraries(madstudio_composition_test PRIVATE madstudio_model)
add_test(NAME madstudio_composition COMMAND madstudio_composition_test)

add_executable(madstudio_unicode_test tests/unicode_test.cpp)
target_link_libraries(madstudio_unicode_test PRIVATE madstudio_model)
add_test(NAME madstudio_unicode COMMAND madstudio_unicode_test)

add_executable(madstudio_panel_catalog_test tests/panel_catalog_test.cpp)
target_link_libraries(madstudio_panel_catalog_test PRIVATE madstudio_model)
target_include_directories(madstudio_panel_catalog_test PRIVATE ${MAD_EPAPER}/src)
target_compile_definitions(madstudio_panel_catalog_test PRIVATE MAD_TEST_HARDWARE="${CMAKE_CURRENT_SOURCE_DIR}/assets/hardware.json")
add_test(NAME madstudio_panel_catalog COMMAND madstudio_panel_catalog_test)

add_executable(maddisplay_layout_model_test ${MAD_CORE}/tests/layout_model_test.cpp)
target_include_directories(maddisplay_layout_model_test PRIVATE ${MAD_CORE}/src)
add_test(NAME maddisplay_layout_model COMMAND maddisplay_layout_model_test)

add_executable(madstudio_layout_switch_test tests/layout_switch_test.cpp)
target_link_libraries(madstudio_layout_switch_test PRIVATE madstudio_model)
add_test(NAME madstudio_layout_switch COMMAND madstudio_layout_switch_test)

add_executable(maddisplay_declarative_test ${MAD_CORE}/tests/declarative_test.cpp)
target_include_directories(maddisplay_declarative_test PRIVATE ${MAD_CORE}/src)
add_test(NAME maddisplay_declarative COMMAND maddisplay_declarative_test)
