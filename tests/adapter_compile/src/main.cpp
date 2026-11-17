#include <type_traits>

#include <Arduino.h>
#include <MadSSD1306/MadSSD1306.hpp>
#include <MadILI9341/MadILI9341.hpp>
#include <MadArduinoGFX/MadArduinoGFX.hpp>
#include <MadU8g2/MadU8g2.hpp>
#include <MadGrayOLED/MadGrayOLED.hpp>
#include <GxEPD2_BW.h>
#include <GxEPD2_3C.h>
#include <GxEPD2_7C.h>
#include <MadEPaper/MadEPaper.hpp>

void compileTFT() {
  Arduino_HWSPI bus(2,5);
  for (const auto& spec : mad::kPanelProfiles) if (spec.kind==mad::PanelKind::RGB && strcmp(spec.id,"custom")) {
    auto* driver=mad::createPanelDriver(&bus,spec.id,4);
    if (driver) { mad::ArduinoGFX panel(*driver,spec.id); panel.begin(); panel.present(); delete driver; }
  }
}
void compileDirect() {
  mad::SSD1306 mono; mono.begin(); mono.present();
  mad::ILI9341 rgb(5,2,4); rgb.begin(); rgb.present();
}
void compileMono() {
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C driver(U8G2_R0,U8X8_PIN_NONE);
  mad::U8g2Adapter<> panel(driver,"ssd1306-128x64"); panel.begin(); panel.present();
}
void compileGray() {
  Adafruit_SSD1327 driver(128,128,&Wire,-1);
  mad::SSD1327 panel(driver); panel.begin(); panel.pixel(1,1,mad::Color::Red); panel.present();
}
template<class Driver,int W,int H,int Bits>
void compileEPD(const char* id,uint8_t rotation=0) {
  Driver driver(typename std::remove_reference<decltype(std::declval<Driver>().epd2)>::type(5,2,4,15));
  mad::EPaper<Driver,W,H,Bits> panel(driver,id); panel.begin(rotation); panel.present();
}
#include <type_traits>
void compileEPDs() {
  compileEPD<GxEPD2_BW<GxEPD2_154_D67,16>,200,200,1>("epd-154-bw");
  compileEPD<GxEPD2_BW<GxEPD2_213_B74,16>,250,122,1>("epd-213-bw",1);
  compileEPD<GxEPD2_BW<GxEPD2_290_T94,16>,296,128,1>("epd-29-bw",1);
  compileEPD<GxEPD2_BW<GxEPD2_420_GDEY042T81,16>,400,300,1>("epd-42-bw");
  compileEPD<GxEPD2_BW<GxEPD2_750_GDEY075T7,16>,800,480,1>("epd-75-bw");
  compileEPD<GxEPD2_3C<GxEPD2_213_Z98c,16>,250,122,2>("epd-213-bwr",1);
  compileEPD<GxEPD2_3C<GxEPD2_420c_Z21,16>,400,300,2>("epd-42-bwr");
  compileEPD<GxEPD2_7C<GxEPD2_565c,16>,600,448,4>("epd-565-7c");
  compileEPD<GxEPD2_7C<GxEPD2_730c_GDEP073E01,16>,800,480,4>("epd-73-6c");
}
// Compile coverage only: no display or GPIO is touched if this firmware is flashed.
void setup() {}
void loop() {}
