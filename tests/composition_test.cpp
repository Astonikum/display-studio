#include <MadDisplayStudio/runtime.hpp>
#include <cassert>
#include <cstring>
#include <fstream>
#include <sstream>
#include <cstdio>
using namespace mad;
using namespace madstudio;

template<class Control> void caption(bool state,uint8_t scale,bool image) {
  Control first(state),second(state); Label textA("OPTION"),textB("OPTION");
  const Color pixels[] = {Color::Red,Color::White,Color::Green,Color::Blue};
  ImageComponent imageA(pixels,{2,2}),imageB(pixels,{2,2});
  first.setContent(image ? static_cast<Component&>(imageA) : static_cast<Component&>(textA));
  second.setContent(image ? static_cast<Component&>(imageB) : static_cast<Component&>(textB));
  first.setScale(scale); second.setScale(scale);
  VerticalLayout rootA,rootB; rootA.add(first); rootB.add(second);
  mad::Page pageA(rootA),pageB(rootB);
  RGB565Buffer<256,64> a,b; pageA.draw(a,{0,0,256,64});
  Compositor display; display.setSafeArea(Insets(0)); display.openPage(display.registerPage(pageB));
  display.render(b,0); assert(std::memcmp(a.pixels(),b.pixels(),a.byteCount()) == 0);
  display.render(b,500000); assert(std::memcmp(a.pixels(),b.pixels(),a.byteCount()) == 0);
  Component* targets[16]; uint8_t count = 0; rootB.collectFocusables(targets,count,16); assert(count == 1 && targets[0] == &second);
  const auto before = second.preferredSize(); assert(before.width > scale*2);
  assert(display.dispatch(Action::Use));
  Rect located; assert(second.locate(image ? static_cast<Component&>(imageB) : static_cast<Component&>(textB),located));
  assert(located.x >= second.bounds().x && located.y >= second.bounds().y);
}
int main() {
  for (auto scale : {1,2,3}) for (bool state : {false,true}) for (bool image : {false,true}) {
    caption<Checkbox>(state,scale,image); caption<RadioButton>(state,scale,image); caption<Switch>(state,scale,image);
  }
  // The string convenience constructor is pixel-identical to an explicit Label child.
  for (auto scale : {1,2,3}) for (bool filled : {false,true}) for (bool focused : {false,true}) {
    ButtonStyle style(Color::White,Color::Blue,scale,2,filled);
    Button legacy("OK",style); Label label("OK",LabelStyle(filled ? Color::Blue : Color::White)); Button composed(label,style);
    const Size size = legacy.preferredSize(); assert(size.width == composed.preferredSize().width && size.height == composed.preferredSize().height);
    RGB565Buffer<128,64> a,b; legacy.layout({2,3,size.width,size.height}); composed.layout({2,3,size.width,size.height});
    legacy.setFocused(focused); composed.setFocused(focused); legacy.draw(a); composed.draw(b);
    assert(std::memcmp(a.pixels(),b.pixels(),a.byteCount()) == 0);
  }
  Button copy("PLACEHOLDER");
  { Button temporary("COPIED"); copy = temporary; assert(copy.content() != temporary.content()); }
  Button same("COPIED"); assert(copy.preferredSize().width == same.preferredSize().width);
  Button cloned(copy); assert(cloned.content() != copy.content());
  Button outer,inner; assert(outer.setContent(inner) && !inner.setContent(outer));
  const Color icon[] = {Color::Red,Color::Green,Color::Blue,Color::White}; ImageComponent image(icon,{2,2});
  Label title("GO"); HorizontalLayout row; row.setGap(2); row.add(image); row.add(title);
  Button composite(row,ButtonStyle(Color::White,Color::Black,3,1));
  assert(composite.content() == &row && !composite.setContent(composite));
  composite.layout({4,5,100,40}); Rect child;
  assert(composite.locate(image,child) && child.width == 6 && child.height == 6);
  RGB565Buffer<128,64> pixels; composite.draw(pixels); assert(pixels.pixel(child.x,child.y) == Color::Red);
  composite.clearContent(); assert(!composite.content());

  // Studio stores a real Label, and a click on it triggers the owning control.
  Document doc = Document::createDefault(); auto& model = *doc.pages[0]->root->children[1];
  assert(model.type == NodeType::Button && model.text.empty() && model.children.size() == 1 && model.children[0]->type == NodeType::Label);
  model.textScale = 2;
  PreviewRuntime runtime(doc); RGB565Buffer<128,64> canvas; runtime.render(canvas,0);
  assert(runtime.activate(*model.children[0]));
  const auto bounds = model.children[0]->previewBounds;
  assert(bounds.x >= model.previewContentBounds.x && bounds.width <= model.previewContentBounds.width);
  assert(bounds.height == 14);
  auto toggle = doc.createNode(NodeType::Checkbox); auto content = doc.createNode(NodeType::Image); content->graphicsWidth = content->graphicsHeight = 2; content->imagePixels.assign(4,W);
  Node* iconModel = content.get(); Node* toggleModel = toggle.get(); toggle->textScale = 2; toggle->children.push_back(std::move(content));
  auto secondLabel = doc.createNode(NodeType::Label); secondLabel->text = "ICON"; toggle->children.push_back(std::move(secondLabel));
  doc.pages[0]->root->children.push_back(std::move(toggle));
  PreviewRuntime iconRuntime(doc); iconRuntime.render(canvas,0);
  assert(iconRuntime.activate(*iconModel) && toggleModel->checked);
  assert(iconModel->previewBounds.width == 4);
  Component* targets[16]; uint8_t count = 0; iconRuntime.find(*doc.pages[0]->root)->view().collectFocusables(targets,count,16); assert(count == 2);
  assert(iconRuntime.dispatch(Action::Previous) && !iconRuntime.dispatch(Action::Previous));
  toggleModel->enabled = false; PreviewRuntime disabled(doc); disabled.render(canvas,0); assert(!disabled.activate(*iconModel)); toggleModel->enabled = true;

  const char* path = "/tmp/mad-composition-roundtrip.mad"; std::string error;
  assert(saveMadUi(doc,path,&error)); Document loaded; assert(loadMadUi(path,loaded,&error));
  assert(loaded.pages[0]->root->children[1]->text.empty()); assert(loaded.pages[0]->root->children[1]->children[0]->text == "START");
  assert(loaded.pages[0]->root->children[2]->children.size() == 2);
  // Old text, including explicit empty text, becomes a child without changing scale or size.
  for (const char* text : {"OLD",""}) {
    { std::ofstream out(path); out << "{\"format\":\"mad\",\"version\":4,\"screen\":{\"width\":128,\"height\":64},\"pages\":[{\"root\":{\"type\":\"VerticalLayout\",\"children\":[{\"type\":\"Button\",\"text\":\"" << text << "\",\"textScale\":2,\"padding\":2,\"filled\":true}]}}]}"; }
    assert(loadMadUi(path,loaded,&error)); auto& old = *loaded.pages[0]->root->children[0];
    assert(old.text.empty() && old.children.size() == 1 && old.children[0]->text == text && old.children[0]->textScale == 1);
    assert(old.children[0]->foreground == Color::Black);
    PreviewRuntime migrated(loaded); migrated.render(canvas,0); Button reference(text,ButtonStyle(Color::White,Color::Black,2,2,true));
    assert(migrated.find(old)->component->preferredSize().width == reference.preferredSize().width);
  }
  { std::ofstream out(path); out << R"({"format":"mad","version":4,"screen":{"width":128,"height":64},"pages":[{"root":{"type":"VerticalLayout","children":[{"type":"Button","id":"node-1","text":"ONE"},{"type":"Button","id":"node-2","text":"TWO"}]}}]})"; }
  assert(loadMadUi(path,loaded,&error)); assert(loaded.pages[0]->root->children[0]->id == "node-1" && loaded.pages[0]->root->children[1]->id == "node-2");
  std::remove(path);
}
