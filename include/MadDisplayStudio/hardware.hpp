#pragma once
#include "madui.hpp"
#include <MadDisplay/display.hpp>
#include <MadDisplay/presentation.hpp>

namespace madstudio {
struct HardwareProfile {
  std::string id = "full", name = "Full power";
  double cpuHz = 0, ramBytes = 0, reservedBytes = 0;
  double psramBytes = 0, flashBytes = 0, cores = 1, logicVolts = 3.3;
  std::string architecture, radio, notes;
  double i2cHz = 400000, spiHz = 24000000;
  double pixelCycles = 8, pointCycles = 50, spanCycles = 40;
  double inspectCycles = 8, nodeCycles = 1000, byteCycles = 2, packetUs = 2;
  double nodeBytes = 256;
};
// Atomic validation: a broken override never replaces the working list.
bool loadHardwareProfiles(const std::string& path,std::vector<HardwareProfile>& profiles,std::string& error);
struct HardwareCost {
  double cpuUs = 0, busUs = 0, panelUs = 0, ramBytes = 0, wireBytes = 0;
  bool memoryExceeded = false, differential = true;
};
size_t sceneNodeCount(const Document& document);
double sceneMemory(const Document& document,const HardwareProfile& profile);
HardwareCost estimateHardware(const HardwareProfile& profile,const Document& document,
    const mad::RenderWork& work,const mad::TransferStats& transfer,bool differential);

// Serial, blocking display pipeline; animation time remains wall time while busy.
class HardwareClock {
 public:
  bool ready(double nowUs) const { return nowUs >= readyUs_; }
  void reset() { readyUs_ = cpuEndUs_ = busEndUs_ = startUs_ = 0; }
  void submit(double nowUs,const HardwareCost& cost) {
    startUs_ = nowUs; cpuEndUs_ = nowUs+cost.cpuUs; busEndUs_ = cpuEndUs_+cost.busUs; readyUs_ = busEndUs_+cost.panelUs;
  }
  double cpuOverlap(double from,double to) const { return overlap(from,to,startUs_,cpuEndUs_); }
  double busOverlap(double from,double to) const { return overlap(from,to,cpuEndUs_,busEndUs_); }
  double panelStartUs() const { return busEndUs_; }
  double readyUs() const { return readyUs_; }
 private:
  static double overlap(double from,double to,double start,double end) {
    const double a = from > start ? from : start, b = to < end ? to : end; return b > a ? b-a : 0;
  }
  double startUs_ = 0, cpuEndUs_ = 0, busEndUs_ = 0, readyUs_ = 0;
};
} // namespace madstudio
