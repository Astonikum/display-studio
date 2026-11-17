#include <MadDisplayStudio/runtime.hpp>
#include <MadDisplayStudio/image_import.hpp>
#include <MadDisplayStudio/unicode_text.hpp>

namespace madstudio {
namespace {
void drawCommands(mad::Canvas& canvas, mad::Size, uint32_t, void* context) {
  for (const auto& command : static_cast<Node*>(context)->commands) command.draw(canvas);
}
std::unique_ptr<mad::Component> makeComponent(Node& node) {
  switch (node.type) {
    case NodeType::GridLayout: return std::unique_ptr<mad::Component>(new mad::GridLayout(node.columns));
    case NodeType::VerticalLayout: return std::unique_ptr<mad::Component>(new mad::VerticalLayout());
    case NodeType::HorizontalLayout: return std::unique_ptr<mad::Component>(new mad::HorizontalLayout());
    case NodeType::Container: return std::unique_ptr<mad::Component>(new mad::Container());
    case NodeType::Panel: return std::unique_ptr<mad::Component>(new mad::Panel(node.foreground, node.background));
    case NodeType::FreeLayout: return std::unique_ptr<mad::Component>(new mad::FreeLayout(node.hasFrame ? node.frame : mad::Frame::max()));
    case NodeType::Label: return std::unique_ptr<mad::Component>(new mad::Label(node.text.c_str(), mad::LabelStyle(node.foreground, node.textScale, node.horizontalAlign, node.verticalAlign)));
    case NodeType::Button: return std::unique_ptr<mad::Component>(new mad::Button(mad::ButtonStyle(node.foreground, node.background, node.textScale, node.padding, node.filled)));
    case NodeType::Checkbox: return std::unique_ptr<mad::Component>(new mad::Checkbox(node.checked, mad::CheckboxStyle(node.background, node.foreground)));
    case NodeType::Switch: return std::unique_ptr<mad::Component>(new mad::Switch(node.on, mad::SwitchStyle(node.background, node.background, node.foreground, node.foreground)));
    case NodeType::Image:
      if (!node.imageColors.empty()) return std::unique_ptr<mad::Component>(new mad::ImageComponent(node.imageColors.data(),{static_cast<int16_t>(node.graphicsWidth),static_cast<int16_t>(node.graphicsHeight)},node.imageCoverage.empty() ? nullptr : node.imageCoverage.data()));
      return std::unique_ptr<mad::Component>(new mad::ImageComponent(node.imagePixels.data(),{static_cast<int16_t>(node.graphicsWidth),static_cast<int16_t>(node.graphicsHeight)},node.background,node.foreground));
    case NodeType::Canvas: return std::unique_ptr<mad::Component>(new mad::CanvasComponent({static_cast<int16_t>(node.graphicsWidth),static_cast<int16_t>(node.graphicsHeight)},drawCommands,&node));
    case NodeType::Rectangle:
    case NodeType::Ellipse:
    case NodeType::Line: return std::unique_ptr<mad::Component>(new mad::ShapeComponent(
        node.type == NodeType::Rectangle ? mad::DrawingKind::Rectangle : node.type == NodeType::Ellipse ? mad::DrawingKind::Ellipse : mad::DrawingKind::Line,
        {static_cast<int16_t>(node.graphicsWidth),static_cast<int16_t>(node.graphicsHeight)},node.foreground,node.filled));
    case NodeType::RadioButton: return std::unique_ptr<mad::Component>(new mad::RadioButton(node.selected, mad::RadioButtonStyle(node.background, node.foreground)));
  }
  return std::unique_ptr<mad::Component>();
}

std::unique_ptr<RuntimeNode> makeRuntime(Node& node,RuntimeNode* parent = nullptr) {
  std::unique_ptr<RuntimeNode> runtime(new RuntimeNode());
  runtime->model = &node; runtime->parent = parent;
  runtime->component = makeComponent(node);
  if (!runtime->component) return std::unique_ptr<RuntimeNode>();
  if (node.textAsset) if (auto* label = dynamic_cast<mad::Label*>(runtime->component.get())) label->setTextLayout(node.textAsset->layout);
  runtime->component->setVisible(node.visible);
  runtime->component->setEnabled(node.enabled);
  if (auto* layout = dynamic_cast<mad::Layout*>(runtime->component.get())) {
    layout->setGap(node.gap); layout->setScrollable(node.scrollable);
  }
  if (auto* grid = dynamic_cast<mad::GridLayout*>(runtime->component.get())) grid->setColumnGap(node.columnGap);
  if (auto* control = dynamic_cast<mad::Switch*>(runtime->component.get())) control->setAnimationDuration(node.switchDurationMs);
  auto* owner = dynamic_cast<mad::ContentControl*>(runtime->component.get());
  if (owner) {
    owner->setScale(node.textScale); owner->setGap(node.gap);
    if (node.children.size() > 1) {
      runtime->contentHost.reset(new mad::HorizontalLayout()); runtime->contentHost->setGap(node.gap);
      owner->setContent(*runtime->contentHost);
    }
  }
  for (size_t i = 0; i < node.children.size(); ++i) {
    std::unique_ptr<RuntimeNode> child = makeRuntime(*node.children[i],runtime.get());
    if (!child) return std::unique_ptr<RuntimeNode>();
    bool added = false;
    if (mad::Layout* layout = dynamic_cast<mad::Layout*>(runtime->component.get())) added = layout->add(child->view());
    else if (mad::FreeLayout* freeLayout = dynamic_cast<mad::FreeLayout*>(runtime->component.get())) {
      added = freeLayout->add(child->view(), node.children[i]->hasFrame ? node.children[i]->frame : mad::Frame::max());
    }
    else if (owner) added = runtime->contentHost ? runtime->contentHost->add(child->view()) : owner->setContent(child->view());
    if (!added) return std::unique_ptr<RuntimeNode>();
    runtime->children.push_back(std::move(child));
  }
  if (node.animated) {
    runtime->motion.reset(new mad::Motion(*runtime->component, node.entrance, node.durationMs, node.delayMs));
    runtime->motion->setEasing(node.easing);
    runtime->component->setFrame(mad::Frame::max());
  }
  mad::Component& view = runtime->view();
  if (node.hasFrame) view.setFrame(node.frame);
  else if (node.type == NodeType::FreeLayout) view.setFrame(mad::Frame::max());
  if (!node.hasFrame || node.widthMode == mad::SizeMode::Max) view.setWidthMode(node.widthMode);
  if (!node.hasFrame || node.heightMode == mad::SizeMode::Max) view.setHeightMode(node.heightMode);
  if (node.scrollable) {
    if (auto* layout = dynamic_cast<mad::Layout*>(runtime->component.get())) {
      if (layout->direction() == mad::LayoutDirection::Vertical) {
        if (!node.hasFrame || node.frame.height.unit == mad::Unit::Auto) view.setHeightMode(mad::SizeMode::Max);
      } else if (!node.hasFrame || node.frame.width.unit == mad::Unit::Auto) view.setWidthMode(mad::SizeMode::Max);
    }
  }
  view.setVisible(node.visible); view.setEnabled(node.enabled);
  return runtime;
}


}  // namespace

void RuntimeNode::replay() {
  if (motion) motion->replay();
  for (auto& child : children) child->replay();
}
void RuntimeNode::syncState() {
  if (auto* checkbox = dynamic_cast<mad::Checkbox*>(component.get())) model->checked = checkbox->checked();
  if (auto* control = dynamic_cast<mad::Switch*>(component.get())) model->on = control->on();
  if (auto* radio = dynamic_cast<mad::RadioButton*>(component.get())) model->selected = radio->selected();
  for (auto& child : children) child->syncState();
}
void RuntimeNode::copyBounds(mad::Component& root,mad::Point camera) {
  const mad::Rect local = view().bounds(); mad::Rect world = local; root.locate(view(),world);
  mad::Rect clip = local;
  if (auto* layout = dynamic_cast<mad::Layout*>(component.get())) clip = layout->scrollViewport();
  else if (auto* owner = dynamic_cast<mad::ContentControl*>(component.get())) clip = owner->contentBounds();
  const auto convert = [](int64_t value) { return static_cast<int16_t>(value > 32767 ? 32767 : value < -32768 ? -32768 : value); };
  const int width = local.width ? local.width : 1,height = local.height ? local.height : 1;
  model->previewContentBounds = {convert(world.x+int64_t(clip.x-local.x)*world.width/width-camera.x),
      convert(world.y+int64_t(clip.y-local.y)*world.height/height-camera.y),
      convert(int64_t(clip.width)*world.width/width),convert(int64_t(clip.height)*world.height/height)};
  world.x -= camera.x; world.y -= camera.y; model->previewBounds = world;
  for (auto& child : children) child->copyBounds(root,camera);
}
RuntimeNode* RuntimeNode::find(const Node& node) {
  if (model == &node) return this;
  for (auto& child : children) if (auto* found = child->find(node)) return found;
  return nullptr;
}

PreviewRuntime::PreviewRuntime(Document& document) : document_(document),compositor_(target_) {
  normalizeContent(document_);
  refreshImportedImages(document_);
  refreshTextAssets(document_);
  compositor_.setSafeArea(document.safeArea);
  compositor_.setFps(document.fps);
  compositor_.setTransition(document.transitionMs, document.transitionEasing);
  compositor_.setNavigationAnimation(document.focusMs,document.scrollMs);
  for (auto& model : document.pages) {
    std::unique_ptr<RuntimePage> page(new RuntimePage(model->background));
    page->root = makeRuntime(*model->root);
    if (!page->root) { pages_.clear(); return; }
    page->host.setSizeMode(mad::SizeMode::Max, mad::SizeMode::Max);
    page->host.add(page->root->view());
    compositor_.registerPage(page->page);
    pages_.push_back(std::move(page));
  }
  if (!pages_.empty()) compositor_.openPage(static_cast<mad::Compositor::PageId>(document.currentPage), false);
}
bool PreviewRuntime::openPage(size_t index) {
  if (index >= pages_.size()) return false;
  if (!compositor_.openPage(static_cast<mad::Compositor::PageId>(index))) return false;
  document_.currentPage = index;
  pages_[index]->root->replay();
  return true;
}
void PreviewRuntime::syncState() { for (auto& page : pages_) page->root->syncState(); }
bool PreviewRuntime::dispatch(mad::Action action) {
  const bool handled = compositor_.dispatch(action);
  if (handled) {
    syncState();
    if (action == mad::Action::Back) {
      document_.currentPage = compositor_.currentPageId();
      replay();
    }
  }
  return handled;
}
bool PreviewRuntime::scrollAt(mad::Point position, mad::Point delta) {
  return compositor_.scrollAt(position,delta);
}
bool PreviewRuntime::activate(Node& node) {
  RuntimeNode* target = find(node);
  if (!target || !node.visible || !node.enabled) return false;
  mad::Component* focusables[mad::Page::kMaxComponents];
  uint8_t count = 0;
  if (document_.currentPage >= pages_.size()) return false;
  pages_[document_.currentPage]->root->view().collectFocusables(focusables, count, mad::Page::kMaxComponents);
  RuntimeNode* owner = nullptr;
  for (RuntimeNode* candidate = target; candidate && !owner; candidate = candidate->parent)
    for (uint8_t i = 0; i < count; ++i) if (focusables[i] == candidate->component.get()) { owner = candidate; break; }
  if (!owner) return false;
  target = owner;
  pages_[document_.currentPage]->page.focus(*target->component);
  const bool handled = target->component->handle(mad::Action::Use);
  if (handled) syncState();
  return handled;
}
bool PreviewRuntime::setLayoutType(Node& node, NodeType type) {
  if (!isLayout(node.type) || !isLayout(type) || node.type == type) return false;
  RuntimeNode* current = find(node);
  if (!current) return false;
  if (type == NodeType::FreeLayout) {
    const mad::Rect parent = current->component->bounds();
    for (auto& child : current->children) {
      if (!child->model->visible) continue;
      const mad::Rect bounds = child->view().bounds();
      child->model->hasFrame = true;
      child->model->frame = mad::Frame::px(bounds.x-parent.x,bounds.y-parent.y,bounds.width,bounds.height);
      child->model->widthMode = child->model->heightMode = mad::SizeMode::Fit;
    }
    node.scrollable = false;
  }
  node.type = type;
  return true;
}
RuntimeNode* PreviewRuntime::find(const Node& node) {
  for (auto& page : pages_) if (auto* found = page->root->find(node)) return found;
  return nullptr;
}
void PreviewRuntime::replay() {
  if (document_.currentPage < pages_.size()) pages_[document_.currentPage]->root->replay();
}
void PreviewRuntime::render(mad::Canvas& canvas, uint32_t nowUs) {
  target_.canvas = &canvas;
  compositor_.setColorProfile(document_.displayColors());
  compositor_.renderFrame(nowUs,&work_);
  for (auto& page : pages_) page->root->copyBounds(page->host,page->page.scrollPosition());
}

}  // namespace madstudio
