#include <MadDisplayStudio/hardware.hpp>
#include <MadDisplayStudio/json.hpp>
#include <MadDisplayStudio/unicode_text.hpp>
#include <fstream>
#include <sstream>
#include <cmath>
#include <set>
#include <algorithm>

namespace madstudio {
bool loadHardwareProfiles(const std::string& path,std::vector<HardwareProfile>& profiles,std::string& error) {
  std::ifstream stream(path); if (!stream) { error = "Cannot open hardware profiles: "+path; return false; }
  std::ostringstream text; text << stream.rdbuf();
  if (text.str().size() > 256*1024) { error = "Hardware profile file too large"; return false; }
  json::Value root;
  if (!json::parse(text.str(),root,error)) return false;
  if (root.type != json::Value::Array || root.array.empty() || root.array.size() > 128) { error = "Expected 1..128 hardware profiles"; return false; }
  std::vector<HardwareProfile> parsed(1); std::set<std::string> ids{"full"};
  for (const auto& item : root.array) {
    HardwareProfile p;
    p.id = json::stringField(item,"id",""); p.name = json::stringField(item,"name","");
    p.architecture = json::stringField(item,"architecture",""); p.radio = json::stringField(item,"radio","");
    p.notes = json::stringField(item,"notes","");
    if (item.type != json::Value::Object || p.id.empty() || p.name.empty() || p.name.size() > 100 || !ids.insert(p.id).second) {
      error = "Invalid or duplicate hardware name/id"; return false;
    }
    auto number = [&](const char* key,double& value,double min,double max) {
      const auto* field = json::field(item,key);
      if (field) { if (field->type != json::Value::Number) return false; value = field->number; }
      return std::isfinite(value) && value >= min && value <= max;
    };
    if (!number("psramBytes",p.psramBytes,0,1e12) || !number("flashBytes",p.flashBytes,0,1e12) ||
        !number("cores",p.cores,1,64) || p.cores != std::floor(p.cores) || !number("logicVolts",p.logicVolts,1,5.5) ||
        !number("cpuHz",p.cpuHz,100000,10000000000.0) || !number("ramBytes",p.ramBytes,128,1e12) ||
        !number("reservedBytes",p.reservedBytes,0,p.ramBytes) || !number("i2cHz",p.i2cHz,1000,400000) ||
        !number("spiHz",p.spiHz,1000,80000000) || !number("pixelCycles",p.pixelCycles,.01,10000) ||
        !number("pointCycles",p.pointCycles,.01,10000) || !number("spanCycles",p.spanCycles,.01,10000) ||
        !number("inspectCycles",p.inspectCycles,.01,10000) || !number("nodeCycles",p.nodeCycles,1,1e7) ||
        !number("byteCycles",p.byteCycles,0,10000) || !number("packetUs",p.packetUs,0,1e6) ||
        !number("nodeBytes",p.nodeBytes,16,65536)) { error = "Invalid hardware limits: "+p.id; return false; }
    parsed.push_back(p);
  }
  profiles.swap(parsed); error.clear(); return true;
}
namespace {
size_t count(const Node& n) { size_t result = 1+(isContentControl(n.type) && n.children.size() > 1 ? 1 : 0); for (const auto& child : n.children) result += count(*child); return result; }
double memory(const Node& n,double nodeBytes) {
  double result = nodeBytes+n.text.size()+1+n.imagePixels.size()+n.imageColors.size()*4+n.imageCoverage.size()+n.commands.size()*sizeof(mad::DrawCommand);
  if (n.animated) result += nodeBytes;
  if (n.textAsset) result += n.textAsset->byteCount();
  if (isContentControl(n.type) && n.children.size() > 1) result += nodeBytes;
  for (const auto& child : n.children) result += memory(*child,nodeBytes);
  return result;
}
}
size_t sceneNodeCount(const Document& document) { size_t result = 0; for (const auto& p : document.pages) result += count(*p->root); return result; }
double sceneMemory(const Document& document,const HardwareProfile& profile) {
  double bytes = profile.reservedBytes;
  for (const auto& p : document.pages) bytes += memory(*p->root,profile.nodeBytes)+profile.nodeBytes*2;
  return bytes;
}
HardwareCost estimateHardware(const HardwareProfile& p,const Document& doc,const mad::RenderWork& work,const mad::TransferStats& transfer,bool differential) {
  HardwareCost cost; cost.differential = differential; cost.wireBytes = transfer.bytes;
  auto panel = mad::panelSpec(doc.displayProfile.c_str()); panel.width=doc.screenWidth; panel.height=doc.screenHeight;
  const double pixels = double(doc.screenWidth)*doc.screenHeight;
  const double packed = panel.packedBytes();
  const double buffer = panel.kind == mad::PanelKind::RGB ? pixels*2*(differential ? 2 : 1) :
      panel.epaper() ? packed*2+std::ceil(panel.width*panel.bits()/8.0)*16 : packed*2;
  const double scene = sceneMemory(doc,p);
  cost.ramBytes = buffer+scene;
  cost.panelUs = transfer.refreshMs*1000.0;
  if (!p.cpuHz) return cost;
  cost.memoryExceeded = scene > p.ramBytes || cost.ramBytes > p.ramBytes+p.psramBytes;
  // Work counters come from the shared renderer; coefficients are explicit estimates.
  cost.cpuUs = ((panel.bits()<8 ? work.pixels*panel.bits()/8.0 : work.pixels)*p.pixelCycles+work.points*p.pointCycles+work.spans*p.spanCycles+
      transfer.inspectedPixels*p.inspectCycles+sceneNodeCount(doc)*p.nodeCycles+transfer.bytes*p.byteCycles)/p.cpuHz*1e6;
  const bool i2c = panel.bus == mad::PanelBus::I2C;
  const double hz = std::min(i2c ? p.i2cHz : p.spiHz,double(panel.maxBusHz()));
  cost.busUs = transfer.bytes*(i2c ? 9.0 : 8.0)/hz*1e6+transfer.packets*p.packetUs;
  // External framebuffer accesses are slower; this factor is an explicit uncalibrated estimate.
  if (cost.ramBytes > p.ramBytes && p.psramBytes) cost.cpuUs *= 1.5;
  return cost;
}
} // namespace madstudio
