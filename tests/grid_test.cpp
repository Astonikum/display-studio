#include <MadDisplayStudio/runtime.hpp>
#include <cassert>
#include <fstream>
#include <sstream>
#include <cstdio>
using namespace mad;
struct Tile : Component {
  Tile(int16_t w,int16_t h) : size{w,h} {}
  Size preferredSize() const override { return size; }
  void draw(Canvas& canvas) const override { canvas.fillRect(bounds_,Color::On); }
  Size size;
};
int main() {
  GridLayout grid(2); Tile a(10,5),b(20,7),c(8,6),d(15,8),e(9,4);
  grid.setGap(2); grid.setColumnGap(3);
  assert(grid.add(a) && grid.add(b) && grid.add(c) && grid.add(d) && grid.add(e));
  assert(!grid.add(a) && !grid.add(grid));
  auto preferred = grid.preferredSize(); assert(preferred.width == 43 && preferred.height == 23);
  grid.layout({5,7,100,100});
  assert(a.bounds().x == 5 && b.bounds().x == 28 && a.bounds().y == 7);
  assert(c.bounds().y == 16 && d.bounds().y == 16 && e.bounds().y == 26);
  b.setVisible(false); grid.layout({5,7,100,100});
  assert(c.bounds().x == 23 && c.bounds().y == 7 && d.bounds().y == 15);
  assert(b.bounds().width == 0 && grid.preferredSize().height == 16);
  grid.setColumns(0); assert(grid.columns() == 1); grid.setColumns(255); assert(grid.columns() == 16);

  GridLayout fill(3); Tile f(2,3),g(2,5),h(2,4),i(2,7);
  f.setSizeMode(SizeMode::Max,SizeMode::Max); g.setSizeMode(SizeMode::Max,SizeMode::Max);
  h.setSizeMode(SizeMode::Max,SizeMode::Max); i.setSizeMode(SizeMode::Max,SizeMode::Max);
  fill.add(f); fill.add(g); fill.add(h); fill.add(i); fill.setSizeMode(SizeMode::Max,SizeMode::Max);
  fill.layout({0,0,101,31});
  assert(f.bounds().width+g.bounds().width+h.bounds().width+2 == 101);
  assert(f.bounds().height == g.bounds().height && i.bounds().y+ i.bounds().height == 31);
  assert(g.bounds().x == f.bounds().width+1 && h.bounds().x == g.bounds().x+g.bounds().width+1);

  GridLayout narrow(2); Button longA("LONG LABEL"),longB("LONG LABEL");
  narrow.add(longA); narrow.add(longB); narrow.setSizeMode(SizeMode::Max,SizeMode::Max);
  narrow.layout({0,0,21,11}); RGB565Buffer<32,16> narrowCanvas;
  narrow.draw(narrowCanvas);
  assert(longA.bounds().width == 10 && longB.bounds().x == 11 && longB.bounds().width == 10);
  for (int y = 0; y < 16; ++y) assert(!narrowCanvas.isOn(10,y) && !narrowCanvas.isOn(21,y));

  GridLayout scrolled(2); Button buttons[6] = {Button("A"),Button("B"),Button("C"),Button("D"),Button("E"),Button("F")};
  for (auto& button : buttons) { button.setWidthMode(SizeMode::Max); assert(scrolled.add(button)); }
  scrolled.setScrollable(true); scrolled.setSizeMode(SizeMode::Max,SizeMode::Max);
  VerticalLayout root; root.setSizeMode(SizeMode::Max,SizeMode::Max); root.add(scrolled);
  Page page(root); Compositor display; display.setSafeArea(Insets(0)); display.setNavigationAnimation(0,0);
  display.openPage(display.registerPage(page)); RGB565Buffer<50,20> canvas;
  display.render(canvas,0); assert(scrolled.scrollContentExtent() == 35 && scrolled.scrollViewport().width == 47);
  assert(buttons[0].focused()); assert(display.dispatch(Action::Right)); assert(buttons[1].focused());
  assert(display.dispatch(Action::Down)); assert(buttons[3].focused());
  assert(display.dispatch(Action::Down)); assert(buttons[5].focused()); display.render(canvas,1000);
  assert(scrolled.scrollOffset() == 15 && buttons[5].bounds().y+buttons[5].bounds().height <= 20);
  assert(display.dispatch(Action::Previous)); assert(buttons[4].focused());
  scrolled.setScrollOffset(0); assert(scrolled.scrollAt({2,2},{0,8})); assert(scrolled.scrollOffset() == 8);
  display.render(canvas,2000); assert(canvas.isOn(49,8) || canvas.isOn(49,10));

  // Grid is a normal layout: scene model, persistence and actual runtime use the same class.
  using namespace madstudio;
  Document doc = Document::createDefault(); doc.pages[0]->root->children.clear();
  auto node = doc.createNode(NodeType::GridLayout); node->columns = 3; node->gap = 2; node->columnGap = 4; node->scrollable = true;
  for (int n = 0; n < 6; ++n) node->children.push_back(doc.createNode(NodeType::Button));
  Node* model = node.get(); doc.pages[0]->root->children.push_back(std::move(node));
  PreviewRuntime runtime(doc); runtime.render(canvas,0);
  auto* actual = dynamic_cast<GridLayout*>(runtime.find(*model)->component.get());
  assert(actual && actual->columns() == 3 && actual->rowGap() == 2 && actual->columnGap() == 4 && actual->scrollable());
  assert(model->previewContentBounds.width == actual->scrollViewport().width && model->previewContentBounds.height > 0);
  const char* path = "/tmp/mad-grid-roundtrip.mad"; std::string error;
  assert(saveMadUi(doc,path,&error)); Document loaded; assert(loadMadUi(path,loaded,&error));
  auto& copy = *loaded.pages[0]->root->children[0]; assert(copy.type == NodeType::GridLayout && copy.columns == 3 && copy.columnGap == 4 && copy.children.size() == 6);
  std::ifstream input(path); std::ostringstream contents; contents << input.rdbuf();
  std::string invalid = contents.str(); const auto at = invalid.find("\"columns\": 3"); assert(at != std::string::npos);
  invalid.replace(at,12,"\"columns\": 0"); { std::ofstream output(path); output << invalid; }
  assert(!loadMadUi(path,loaded,&error)); assert(loaded.pages[0]->root->children[0]->columns == 3);
  std::remove(path);
}
