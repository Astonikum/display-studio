#include <MadDisplayStudio/preview.hpp>
#include <MadDisplayStudio/runtime.hpp>
#include <MadDisplayStudio/hardware.hpp>
#include <MadDisplay/panel_presentation.hpp>
#include <MadEPaper/MadEPaper.hpp>
#include <cassert>
#include <set>
#include <cmath>
#include <cstdio>
using namespace madstudio;

struct FakeEPD {
  struct { bool hasFastPartialUpdate=true; } epd2;
  std::vector<uint16_t> image=std::vector<uint16_t>(40000,0);
  int page=0, phase=0, rotation=0, attempts=0;
  mad::Rect window{0,0,200,200};
  int pageHeight() const { return 16; } int pages() const { return 13; }
  int width() const { return 200; } int height() const { return 200; }
  void init(int,bool,int,bool) {} void setRotation(int r) { rotation=r; }
  int getRotation() const { return rotation; }
  void setFullWindow() { window={0,0,200,200}; }
  void setPartialWindow(int x,int y,int w,int h) { window={int16_t(x),int16_t(y),int16_t(w),int16_t(h)}; }
  void firstPage() { page=phase=attempts=0; }
  void fillScreen(uint16_t) {}
  void drawPixel(int x,int y,uint16_t c) {
    ++attempts;
    const int nativeY=rotation==0 ? y : rotation==1 ? x : rotation==2 ? 199-y : 199-x;
    if (window.width==200 && window.height==200 && nativeY/16!=page) return;
    image[y*200+x]=c;
  }
  bool nextPage() { if (++page==pages()) { page=0; if (++phase==2) return false; } return true; }
};

int main() {
  std::set<std::string> ids;
  for (const auto& spec : kDisplayProfiles) {
    assert(ids.insert(spec.id).second); assert(spec.width>0 && spec.height>0);
    Document doc=Document::createDefault(); applyDisplayProfile(doc,spec);
    PreviewCanvas canvas; canvas.configure(doc,true); PreviewRuntime runtime(doc);
    runtime.render(canvas,0); const auto first=canvas.transfer();
    assert(first.bytes>0); assert(canvas.viewPixels());
    assert(canvas.width()==spec.width && canvas.height()==spec.height);
    std::string error; Document restored;
    assert(saveMadUi(doc,"/tmp/mad-panel-catalog-test.mad",&error));
    assert(loadMadUi("/tmp/mad-panel-catalog-test.mad",restored,&error));
    assert(restored.displayProfile==doc.displayProfile && restored.palette.count==doc.palette.count);
    for (unsigned i=0;i<doc.palette.count;++i) assert(restored.palette.colors[i]==doc.palette.colors[i]);
    if (spec.epaper()) {
      assert(first.fullRefresh && first.refreshMs==spec.fullRefreshMs);
      const auto unlimited=estimateHardware(HardwareProfile{},doc,{},first,true);
      assert(unlimited.panelUs==spec.fullRefreshMs*1000.0 && unlimited.busUs==0);
      HardwareClock clock; clock.submit(0,unlimited);
      assert(!clock.ready(unlimited.panelUs-1)); assert(clock.busOverlap(0,unlimited.panelUs)==0);
      canvas.present(); assert(canvas.transfer().bytes==0 && canvas.transfer().refreshMs==0);
    }
    if (!strcmp(spec.controller,"ILI9488")) assert(first.bytes==uint32_t(spec.width)*spec.height*3+11);
  }
  assert(ids.size()>=40);
  assert(!mad::panelSpec("gc9a01-240x240").visible(0,0));
  assert(mad::panelSpec("gc9a01-240x240").visible(120,120));
  assert(!mad::panelSpec("st7789-240x280").visible(0,0));
  auto spec=mad::panelSpec("epd-213-bw");
  std::vector<uint8_t> frame(spec.packedBytes(),0), previous(frame);
  mad::EpaperPlanner planner;
  auto update=planner.plan(spec,frame.data(),previous.data()); assert(update.full);
  planner.committed(update); assert(!planner.plan(spec,frame.data(),previous.data()).transfer.bytes);
  for (unsigned i=0;i<8;++i) {
    frame[100]^=1; update=planner.plan(spec,frame.data(),previous.data());
    assert(update.full==(i==7)); assert(update.bounds.width>0 && update.bounds.height>0);
    assert(update.bounds.x>=0 && update.bounds.x+update.bounds.width<=spec.width);
    previous=frame; planner.committed(update);
  }
  auto gray=mad::panelPalette(mad::panelSpec("ssd1327-128x128"));
  mad::PaletteBuffer<9,7,4> packed(gray); packed.clear(mad::Color::White); packed.fillSpan(2,3,5,mad::rgb(119,119,119));
  assert(packed.pixel(1,3)==mad::Color::White && packed.pixel(2,3)==mad::rgb(119,119,119));
  assert(packed.pixel(6,3)==mad::rgb(119,119,119) && packed.pixel(7,3)==mad::Color::White);
  packed.clear(mad::Color::Black); assert(packed.pixel(6,3)==mad::Color::Black);
  std::vector<HardwareProfile> boards; std::string error;
  assert(loadHardwareProfiles(MAD_TEST_HARDWARE,boards,error)); assert(boards.size()>=34);
  bool psram=false;
  for (const auto& board : boards) {
    if (!board.cpuHz) continue;
    assert(!board.architecture.empty() && board.cores>=1 && board.reservedBytes<board.ramBytes);
    if (board.psramBytes) {
      psram=true; Document doc=Document::createDefault(); applyDisplayProfile(doc,mad::panelSpec("ili9488-320x480"));
      const auto cost=estimateHardware(board,doc,{}, {},true);
      assert(!cost.memoryExceeded);
      auto noExternal=board; noExternal.psramBytes=0;
      assert(estimateHardware(noExternal,doc,{}, {},true).memoryExceeded);
    }
  }
  for (unsigned rotation=0;rotation<4;++rotation) {
    FakeEPD driver;
    mad::EPaper<FakeEPD,200,200,1> panel(driver,"epd-154-bw");
    std::vector<uint8_t> history(5000);
    assert(!panel.setHistoryBuffer(history.data(),4999));
    assert(panel.setHistoryBuffer(history.data(),history.size()));
    assert(!panel.setHistoryBuffer(history.data(),history.size()));
    assert(panel.begin(rotation)); panel.clear(mad::Color::White); panel.pixel(10,10,mad::Color::Black); panel.present();
    assert(driver.attempts==80000);
    for (unsigned i=0;i<40000;++i) assert(driver.image[i]==(i==2010 ? 0 : 0xffff));
    panel.present(); assert(panel.transfer().bytes==0);
    panel.pixel(11,10,mad::Color::Black); panel.present(); assert(!panel.transfer().fullRefresh);
    assert(driver.image[2010]==0 && driver.image[2011]==0 && driver.image[2012]==0xffff);
  }
  assert(psram); std::remove("/tmp/mad-panel-catalog-test.mad");
}
