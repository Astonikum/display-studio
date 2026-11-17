#include <MadDisplayStudio/hardware.hpp>
#include <MadDisplayStudio/runtime.hpp>
#include <MadDisplayStudio/preview.hpp>
#include <MadSSD1306/MadSSD1306.hpp>
#include <MadILI9341/MadILI9341.hpp>
#include <cassert>
#include <cmath>
#include <fstream>
#include <cstdio>
using namespace madstudio;
int main() {
  std::vector<HardwareProfile> boards; std::string error;
  assert(loadHardwareProfiles(MAD_TEST_HARDWARE,boards,error)); assert(boards.size() >= 7);
  assert(boards[0].cpuHz == 0 && boards[1].ramBytes == 2048);
  const char* path = "/tmp/mad-hardware-invalid.json";
  for (const char* invalid : {"[]", "[{}]", "[{\"id\":\"full\",\"name\":\"invalid\"}]",
      "[{\"id\":\"a\",\"name\":\"a\",\"cpuHz\":1e999,\"ramBytes\":8192}]",
      "[{\"id\":\"a\",\"name\":\"a\",\"cpuHz\":16000000,\"ramBytes\":8192,\"i2cHz\":\"400000\"}]"}) {
    { std::ofstream out(path); out << invalid; }
    assert(!loadHardwareProfiles(path,boards,error)); assert(boards.size() >= 7);
  }
  std::remove(path);
  HardwareClock clock; HardwareCost job; job.cpuUs = 10000; job.busUs = 30000;
  clock.submit(1000,job); assert(!clock.ready(40999) && clock.ready(41000));
  assert(clock.cpuOverlap(0,100000) == 10000 && clock.busOverlap(0,100000) == 30000);
  assert(clock.cpuOverlap(5000,15000) == 6000 && clock.busOverlap(5000,15000) == 4000);
  clock.submit(5e12,job); assert(!clock.ready(5e12+39999) && clock.ready(5e12+40000));
  clock.reset(); assert(clock.ready(0));

  Document doc = Document::createAnimated(); PreviewRuntime runtime(doc),physical(doc);
  PreviewCanvas host; host.configure(doc,true);
  mad::SSD1306 oled; assert(oled.begin());
  for (uint32_t time : {0u,16000u,300000u,900000u,900000u,1000000u}) {
    runtime.render(host,time); physical.render(oled,time);
    for (int y = 0; y < 64; ++y) for (int x = 0; x < 128; ++x) assert(host.isOn(x,y) == oled.isOn(x,y));
    for (size_t i = 0; i < 1024; ++i) assert(host.nativePixels()[i] == Wire.panel[i]);
    const auto transfer = host.transfer();
    if (oled.lastTransferBytes()) assert(transfer.bytes == oled.lastTransferBytes()+8+2*((oled.lastTransferBytes()+30)/31));
    else assert(transfer.bytes == 0);
  }
  mad::RenderWork work; work.pixels = 8192; work.points = 100;
  mad::TransferStats full; full.bytes = 1100; full.packets = 35;
  const auto cost = estimateHardware(boards[2],doc,work,full,true);
  assert(std::abs(cost.busUs-(1100*9.0/400000*1e6+70)) < .001);
  auto slow = boards[2]; slow.cpuHz /= 2;
  assert(estimateHardware(slow,doc,work,full,true).cpuUs == cost.cpuUs*2);
  auto free = estimateHardware(boards[0],doc,work,full,true); assert(free.cpuUs == 0 && free.busUs == 0);
  assert(estimateHardware(boards[1],doc,work,full,true).memoryExceeded);
  for (const auto& p : kDisplayProfiles) if (std::string(p.id) == "ili9341-320x240") applyDisplayProfile(doc,p);
  host.configure(doc,true); PreviewRuntime rgb(doc),rgbPhysical(doc);
  mad::ILI9341 ili(5,2); assert(ili.begin(1));
  for (uint32_t time : {0u,100000u,700000u,700000u}) {
    rgb.render(host,time); rgbPhysical.render(ili,time);
    assert(host.transfer().bytes == ili.lastTransferPixels()*2+ili.lastTransferWindows()*11);
    const auto& panel = Adafruit_ILI9341::trace().pixels;
    for (size_t i = 0; i < 76800; ++i) assert(host.rgbPixels()[i] == panel[i]);
  }
  auto pico = boards[5];
  assert(estimateHardware(pico,doc,{}, {},true).memoryExceeded);
  assert(!estimateHardware(pico,doc,{}, {},false).memoryExceeded);
  assert(runtime.work().pixels > 0);
}
