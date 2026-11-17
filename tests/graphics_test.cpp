#include <MadDisplayStudio/runtime.hpp>
#include <cassert>
#include <cstdio>
#include <fstream>
#include <sstream>

using namespace mad;
using namespace madstudio;

static void paint(Canvas& canvas, Size size, uint32_t nowMs, void* context) {
  assert(size.width == 8 && size.height == 6 && nowMs == 42);
  *static_cast<bool*>(context) = true;
  canvas.clear(Color::On);
  canvas.pixel(-1,0,Color::On); canvas.pixel(8,5,Color::On);
  canvas.pixel(1,1,Color::Off);
}

int main() {
  BareBuffer<32,32> buffer;
  bool called = false;
  CanvasComponent canvas({8,6},paint,&called);
  canvas.setFrame(Frame::px(4,5,8,6)); canvas.layout({0,0,32,32}); canvas.update(42); canvas.draw(buffer);
  assert(called && buffer.isOn(4,5) && buffer.isOn(11,10) && !buffer.isOn(5,6));
  assert(!buffer.isOn(3,5) && !buffer.isOn(12,5) && !buffer.isOn(4,11));

  const MatrixPixel pixels[] = {W,N,BF,WF};
  ImageComponent image(pixels,{2,2}); image.setFrame(Frame::px(2,3,4,4)); image.layout({0,0,32,32});
  buffer.clear(Color::On); image.draw(buffer);
  assert(buffer.isOn(2,3) && buffer.isOn(5,3)); // transparent leaves the background intact
  assert(!buffer.isOn(2,6) && buffer.isOn(5,6));

  buffer.clear(); drawLine(buffer,{7,2},{2,7},Color::On);
  for (int x = 2; x <= 7; ++x) assert(buffer.isOn(x,9-x));
  buffer.clear(); drawEllipse(buffer,{4,5,9,7},Color::On,false);
  assert(!buffer.isOn(8,8) && buffer.isOn(8,5) && buffer.isOn(4,8));
  for (int y = 0; y < 7; ++y) for (int x = 0; x < 9; ++x)
    assert(buffer.isOn(4+x,5+y) == buffer.isOn(12-x,11-y));
  drawEllipse(buffer,{4,5,9,7},Color::On,true); assert(buffer.isOn(8,8));

  // Compact text has no trailing advance or phantom line-height padding.
  Label label("AB"); Button button("AB");
  assert(label.preferredSize().width == 11 && label.preferredSize().height == 7);
  assert(button.preferredSize().width == 15 && button.preferredSize().height == 11);

  Document document = Document::createDefault(); document.pages[0]->root->children.clear();
  auto art = document.createNode(NodeType::Image); art->graphicsWidth = 3; art->graphicsHeight = 2;
  art->imagePixels = {W,N,B,BF,WF,W};
  document.pages[0]->root->children.push_back(std::move(art));
  auto drawing = document.createNode(NodeType::Canvas);
  DrawCommand command; command.kind = DrawingKind::Ellipse; command.rect = {1,2,9,7}; command.filled = true;
  drawing->commands.push_back(command); document.pages[0]->root->children.push_back(std::move(drawing));
  const char* path = "/tmp/mad-graphics-test.mad";
  std::string error; assert(saveMadUi(document,path,&error));
  Document loaded; assert(loadMadUi(path,loaded,&error));
  auto& root = loaded.pages[0]->root;
  assert(root->children[0]->graphicsWidth == 3 && root->children[0]->imagePixels[4] == WF);
  assert(root->children[1]->commands[1].kind == DrawingKind::Ellipse && root->children[1]->commands[1].filled);
  PreviewRuntime preview(loaded); assert(preview.valid()); preview.render(buffer,0);
  assert(buffer.isOn(1,1));

  // Old Matrix files migrate to an Image without changing artwork.
  std::string legacy = R"({"format":"madui","version":2,"screen":{"width":128,"height":64},"pages":[{"root":{"type":"VerticalLayout","children":[{"type":"Matrix","name":"Matrix","matrix":[)";
  for (int i = 0; i < 64; ++i) { if (i) legacy += ','; legacy += i == 10 ? '2' : '0'; }
  legacy += "]}]}}]}";
  { std::ofstream out(path); out << legacy; }
  assert(loadMadUi(path,loaded,&error));
  assert(loaded.pages[0]->root->children[0]->type == NodeType::Image);
  assert(loaded.pages[0]->root->children[0]->name == "Image");
  assert(loaded.pages[0]->root->children[0]->imagePixels[10] == WF);

  // Malformed dimensions, pixel buffers and command coordinates cannot replace the document.
  for (const char* invalid : {
      R"({"type":"Image","graphicsWidth":0})",
      R"({"type":"Image","graphicsWidth":513})",
      R"({"type":"Image","graphicsWidth":2,"graphicsHeight":2,"pixels":[4]})",
      R"({"type":"Canvas","commands":[{"kind":9}]})",
      R"({"type":"Canvas","commands":[{"width":1.5}]})"}) {
    const std::string json = std::string(R"({"format":"mad","version":3,"screen":{"width":128,"height":64},"pages":[{"root":{"type":"VerticalLayout","children":[)") + invalid + "]}}]}";
    { std::ofstream out(path); out << json; }
    assert(!loadMadUi(path,loaded,&error));
    assert(loaded.pages[0]->root->children[0]->imagePixels[10] == WF);
  }
  std::remove(path);
}
