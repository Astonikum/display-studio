#pragma once
#include "madui.hpp"
#include <MadDisplay/panel_presentation.hpp>

namespace madstudio {
class PreviewCanvas final : public mad::RGB565Surface {
 public:
  void configure(const Document& doc,bool differential) {
    const auto& spec = mad::panelSpec(doc.displayProfile.c_str());
    const bool changed = size().width != doc.screenWidth || size().height != doc.screenHeight ||
        profileId_ != doc.displayProfile || differential != differential_;
    colors_ = doc.displayColors();
    if (!changed) return;
    spec_ = spec; spec_.width=doc.screenWidth; spec_.height=doc.screenHeight;
    profileId_=doc.displayProfile; differential_=differential;
    mono_=spec_.mono(); legacyMono_=mono_ && spec_.bus==mad::PanelBus::I2C && spec_.width==128 && std::strcmp(spec_.controller,"SSD1306")==0;
    storage_.assign(size_t(doc.screenWidth)*doc.screenHeight,0);
    setStorage({static_cast<int16_t>(doc.screenWidth),static_cast<int16_t>(doc.screenHeight)},storage_.data(),storage_.size());
    previous_.assign(spec_.kind == mad::PanelKind::RGB && differential_ ? storage_.size() : 0,0);
    native_.assign(spec_.kind == mad::PanelKind::RGB ? 0 : spec_.packedBytes(),0);
    packedPrevious_.assign(spec_.epaper() ? native_.size() : 0,0);
    visible_.clear(); shown_.clear(); invalidate();
  }
  mad::PixelFormat format() const override { return mono_ ? mad::PixelFormat::Mono1 : mad::PixelFormat::RGB565; }
  void clear(mad::Color color = mad::Color::Off) override { mad::RGB565Surface::clear(logical(color)); }
  void pixel(int16_t x,int16_t y,mad::Color color) override { mad::RGB565Surface::pixel(x,y,logical(color)); }
  void fillSpan(int16_t x,int16_t y,int16_t count,mad::Color color) override { mad::RGB565Surface::fillSpan(x,y,count,logical(color)); }
  void fillRect(mad::Rect rect,mad::Color color) override { mad::RGB565Surface::fillRect(rect,logical(color)); }
  void present() override {
    if (legacyMono_) { monoPresenter_.present(*this,native_.data(),*this); transfer_=monoPresenter_.stats(); }
    else if (spec_.kind == mad::PanelKind::RGB) {
      rgbPresenter_.present(*this,previous_.empty() ? nullptr : previous_.data(),*this,spec_.wireBits()/8,spec_.windowBytes());
      transfer_=rgbPresenter_.stats();
    } else {
      mad::packPanel(*this,spec_,native_.data()); transfer_={};
      if (spec_.epaper()) {
        const auto update=epaper_.plan(spec_,native_.data(),packedPrevious_.data());
        transfer_=update.transfer; epaper_.committed(update);
        packedPrevious_=native_;
      } else {
        transfer_.inspectedPixels=storage_.size();
        transfer_.bytes=native_.size()+8; transfer_.windows=1;
        transfer_.packets=spec_.bus == mad::PanelBus::I2C ? (native_.size()+30)/31+1 : 1;
        if (spec_.bus == mad::PanelBus::I2C) transfer_.bytes+=2*(transfer_.packets-1);
      }
    }
  }
  const uint8_t* nativePixels() const { return native_.data(); }
  void invalidate() { rgbPresenter_.invalidate(); monoPresenter_.invalidate(); epaper_.invalidate(); }
  const mad::TransferStats& transfer() const { return transfer_; }
  const mad::PanelSpec& spec() const { return spec_; }
  const uint16_t* viewPixels() {
    if (spec_.kind == mad::PanelKind::RGB && !spec_.cornerRadius) return rgbPixels();
    const auto old=visible_; visible_.resize(storage_.size());
    for (int y=0;y<height();++y) for (int x=0;x<width();++x) {
      const size_t i=size_t(y)*width()+x;
      mad::Color c=mono_ ? colors_.map(mad::RGB565Surface::pixel(x,y),x,y) : mad::RGB565Surface::pixel(x,y);
      if (spec_.epaper()) {
        // Approximate paper tint and residual charge after partial erases; full refresh cleans it.
        if (c == mad::Color::White) c=mad::rgb(242,241,228);
        if (!transfer_.fullRefresh && shown_.size()==storage_.size() && old.size()==storage_.size()) {
          if (shown_[i] == storage_[i]) c=mad::fromRGB565(old[i]);
          else if (c == mad::rgb(242,241,228)) c=mad::rgb(219,219,207);
        }
      }
      visible_[i]=spec_.visible(x,y) ? mad::rgb565(c) : mad::rgb565(mad::rgb(28,28,30));
    }
    shown_=storage_; return visible_.data();
  }
  void startWrite() {} void endWrite() {}
  void setAddrWindow(int,int,int,int) {}
  void writePixels(uint16_t*,size_t,bool,bool) {}
  bool send(const uint8_t*,int16_t,int16_t,int16_t,int16_t,int16_t,uint8_t) { return true; }
 private:
  mad::Color logical(mad::Color c) const {
    if (mono_) return mad::monochromeOn(c) ? mad::Color::White : mad::Color::Black;
    if (spec_.kind == mad::PanelKind::Gray) {
      const uint8_t gray=((uint32_t(mad::red(c))*77+uint32_t(mad::green(c))*150+uint32_t(mad::blue(c))*29+128)/256+8)/17;
      return mad::rgb(gray*17,gray*17,gray*17);
    }
    return spec_.epaper() ? mad::panelPalette(spec_).map(c) : c;
  }
  bool mono_=false, legacyMono_=false, differential_=true;
  mad::PanelSpec spec_=mad::panelSpec("custom"); std::string profileId_;
  mad::ColorProfile colors_;
  std::vector<uint16_t> storage_,previous_,visible_,shown_;
  std::vector<uint8_t> native_,packedPrevious_;
  mad::RGB565Presenter rgbPresenter_;
  mad::MonoPagePresenter monoPresenter_;
  mad::EpaperPlanner epaper_;
  mad::TransferStats transfer_;
};
} // namespace madstudio
