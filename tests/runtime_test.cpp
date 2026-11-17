#include <MadDisplayStudio/runtime.hpp>
#include <cassert>
#include <cstring>
#include <fstream>
#include <sstream>

using namespace madstudio;

int main() {
  Document document = Document::createAnimated();
  PreviewRuntime preview(document);
  assert(preview.valid());
  mad::BareBuffer<128,64> canvas, first, settled;
  preview.render(canvas, 0);
  std::memcpy(first.pixels(), canvas.pixels(), canvas.byteCount());
  preview.render(canvas, 400000);
  assert(std::memcmp(first.pixels(), canvas.pixels(), canvas.byteCount()) != 0);
  preview.render(canvas, 1400000);
  std::memcpy(settled.pixels(), canvas.pixels(), canvas.byteCount());
  // Repeated timestamps are a true pause, including every nested Motion.
  preview.render(canvas, 1400000);
  assert(std::memcmp(settled.pixels(), canvas.pixels(), canvas.byteCount()) == 0);
  preview.replay(); preview.render(canvas, 1400000);
  assert(std::memcmp(first.pixels(), canvas.pixels(), canvas.byteCount()) == 0);
  preview.render(canvas, 3000000);
  Node& control = *document.pages[0]->root->children[2]->children[0];
  assert(preview.find(control)->component->preferredSize().width == mad::Switch::kPreferredWidth);
  assert(preview.activate(control) && control.on);
  preview.render(canvas, 3000000);
  std::memcpy(first.pixels(), canvas.pixels(), canvas.byteCount());
  preview.render(canvas, 3110000);
  assert(std::memcmp(first.pixels(), canvas.pixels(), canvas.byteCount()) != 0);
  assert(preview.openPage(1));
  preview.render(canvas, 3200000);
  assert(preview.compositor().currentPageId() == 1);
  preview.render(canvas, 4500000);
  for (int i = 0; i < 40; ++i) assert(preview.openPage(i % 3));
  assert(!preview.openPage(100));
  const size_t last = document.currentPage;
  assert(preview.dispatch(mad::Action::Back));
  assert(document.currentPage != last);
  preview.render(canvas, 4600000);

  for (auto direction : {NodeType::VerticalLayout,NodeType::HorizontalLayout}) {
    Document scroll = Document::createDefault();
    scroll.pages[0]->root->children.clear();
    auto layout = scroll.createNode(direction);
    layout->scrollable = true; layout->widthMode = layout->heightMode = mad::SizeMode::Max;
    Node* model = layout.get();
    for (int i = 0; i < 12; ++i) layout->children.push_back(scroll.createNode(NodeType::Button));
    scroll.pages[0]->root->widthMode = scroll.pages[0]->root->heightMode = mad::SizeMode::Max;
    scroll.pages[0]->root->children.push_back(std::move(layout));
    PreviewRuntime view(scroll); view.render(canvas,0);
    auto* actualLayout = dynamic_cast<mad::Layout*>(view.find(*model)->component.get());
    assert(actualLayout && actualLayout->scrollable());
    actualLayout->scrollBy(10); assert(actualLayout->scrollOffset() > 0);
  }

  // The simulator output equals a direct library setup, including switch style.
  Document simple;
  simple.transitionMs = 0; simple.safeArea = mad::Insets(0,0,0,0);
  simple.pages.emplace_back(new Page());
  auto sw = simple.createNode(NodeType::Switch);
  simple.pages[0]->root->children.push_back(std::move(sw));
  PreviewRuntime actual(simple);
  actual.render(canvas, 0);
  mad::VerticalLayout libraryRoot;
  mad::Switch librarySwitch;
  libraryRoot.add(librarySwitch);
  mad::Page libraryPage(libraryRoot);
  mad::Compositor reference;
  reference.setSafeArea(mad::Insets(0,0,0,0));
  reference.openPage(reference.registerPage(libraryPage));
  reference.render(settled,0);
  assert(std::memcmp(settled.pixels(), canvas.pixels(), canvas.byteCount()) == 0);

  simple.pages[0]->root->enabled = false;
  PreviewRuntime disabled(simple); disabled.render(canvas, 0);
  assert(!disabled.activate(*simple.pages[0]->root->children[0]));

  // Every entry in the visible control catalog has a real library runtime.
  for (auto type : kNodeTypes) {
    Document catalog = Document::createDefault();
    catalog.pages[0]->root->children.clear();
    auto entry = catalog.createNode(type);
    Node* added = entry.get();
    catalog.pages[0]->root->children.push_back(std::move(entry));
    PreviewRuntime runtime(catalog); assert(runtime.valid());
    runtime.render(canvas,0); assert(runtime.find(*added));
    if (type == NodeType::Image)
      assert((dynamic_cast<mad::ImageComponent*>(runtime.find(*added)->component.get())));
  }

  const char* path = "/tmp/mad-studio-animation.madui";
  std::string error;
  document.transitionMs = 765;
  document.transitionEasing = mad::Easing::BounceOut;
  document.fps = 45; document.safeArea = mad::Insets(2,3,4,5);
  Node& node = *document.pages[0]->root->children[1];
  node.scrollable = true; node.durationMs = 890; node.delayMs = 123; node.switchDurationMs = 345;
  auto artwork = document.createNode(NodeType::Image);
  artwork->imagePixels[10] = mad::WF;
  document.pages[0]->root->children.push_back(std::move(artwork));
  assert(saveMadUi(document, path, &error));
  Document restored;
  assert(loadMadUi(path, restored, &error));
  const Node& copy = *restored.pages[0]->root->children[1];
  assert(copy.animated && copy.entrance == mad::Entrance::Slide && copy.easing == mad::Easing::BackOut);
  assert(copy.durationMs == 890 && copy.delayMs == 123 && copy.switchDurationMs == 345 && copy.scrollable);
  assert(restored.pages[0]->root->children.back()->type == NodeType::Image);
  assert(restored.pages[0]->root->children.back()->imagePixels[10] == mad::WF);
  assert(restored.transitionMs == 765 && restored.transitionEasing == mad::Easing::BounceOut);
  assert(restored.fps == 45 && restored.safeArea.top == 3 && restored.safeArea.right == 4);
  PreviewRuntime fromFile(restored); assert(fromFile.valid());

  // Bad animation values fail without replacing the current document.
  std::ifstream file(path); std::ostringstream stream; stream << file.rdbuf();
  const std::string valid = stream.str();
  for (const std::string replacement : {"-1", "65536", "1.5", "\"bad\""}) {
    std::string invalid = valid;
    const auto at = invalid.find("\"durationMs\": 900"); assert(at != std::string::npos);
    invalid.replace(at, std::strlen("\"durationMs\": 900"), "\"durationMs\": " + replacement);
    { std::ofstream output(path); output << invalid; }
    assert(!loadMadUi(path, restored, &error)); assert(restored.name == "Motion playground");
  }
  // v1 assets load with no entrance effect and remain editable.
  { std::ofstream output(path); output << R"({"format":"madui","version":1,"screen":{"width":128,"height":64},"pages":[{"root":{"type":"VerticalLayout","children":[{"type":"Switch"}]}}]})"; }
  assert(loadMadUi(path, restored, &error));
  assert(!restored.pages[0]->root->children[0]->animated);
  std::remove(path);
}
