#pragma once

#include "madui.hpp"
#include <MadDisplay/display.hpp>

namespace madstudio {

struct RuntimeNode {
  Node* model = nullptr;
  RuntimeNode* parent = nullptr;
  std::unique_ptr<mad::Component> component;
  std::unique_ptr<mad::Motion> motion;
  std::unique_ptr<mad::HorizontalLayout> contentHost;
  std::vector<std::unique_ptr<RuntimeNode>> children;
  mad::Component& view() { return motion ? static_cast<mad::Component&>(*motion) : *component; }
  void replay();
  void syncState();
  void copyBounds(mad::Component& root,mad::Point camera);
  RuntimeNode* find(const Node& node);
};

// Studio renders the actual library components; it has no separate animation engine.
class PreviewTarget final : public mad::Canvas {
 public:
  mad::Canvas* canvas = nullptr;
  mad::Size size() const override { return canvas->size(); }
  mad::PixelFormat format() const override { return canvas->format(); }
  mad::Rect viewport() const override { return canvas->viewport(); }
  void clear(mad::Color c = mad::Color::Off) override { canvas->clear(c); }
  void pixel(int16_t x,int16_t y,mad::Color c) override { canvas->pixel(x,y,c); }
  void fixedPixel(int16_t x,int16_t y,mad::Color c) override { canvas->fixedPixel(x,y,c); }
  void fillSpan(int16_t x,int16_t y,int16_t n,mad::Color c) override { canvas->fillSpan(x,y,n,c); }
  void fillRect(mad::Rect r,mad::Color c) override { canvas->fillRect(r,c); }
  void text(int16_t x,int16_t y,const char* s,uint8_t scale,mad::Color c) override { canvas->text(x,y,s,scale,c); }
  void present() override { canvas->present(); }
};

class PreviewRuntime {
 public:
  explicit PreviewRuntime(Document& document);
  bool valid() const { return !pages_.empty(); }
  bool openPage(size_t index);
  bool dispatch(mad::Action action);
  bool scrollAt(mad::Point position, mad::Point delta);
  bool activate(Node& node);
  bool setLayoutType(Node& node, NodeType type);
  void replay();
  void render(mad::Canvas& canvas, uint32_t nowUs);
  const mad::RenderWork& work() const { return work_; }
  RuntimeNode* find(const Node& node);
  mad::Compositor& compositor() { return compositor_; }
 private:
  struct RuntimePage {
    std::unique_ptr<RuntimeNode> root;
    mad::VerticalLayout host;
    mad::Page page;
    explicit RuntimePage(mad::Color background) : page(host, background) {}
  };
  void syncState();
  Document& document_;
  std::vector<std::unique_ptr<RuntimePage>> pages_;
  PreviewTarget target_;
  mad::Display<PreviewTarget> compositor_;
  mad::RenderWork work_;
};

}  // namespace madstudio
