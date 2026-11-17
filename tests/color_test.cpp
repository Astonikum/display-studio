#include <MadDisplayStudio/runtime.hpp>
#include <cassert>
#include <cstdio>
#include <fstream>
#include <sstream>
using namespace madstudio;
using mad::Color;
int main() {
  Document document = Document::createDefault();
  for (const auto& profile : kDisplayProfiles) if (profile.id == std::string("ili9341-320x240")) applyDisplayProfile(document,profile);
  assert(document.colorMode == mad::ColorMode::RGB && document.screenWidth == 320);
  document.palette = mad::Palette(Color::Black,Color::Red,Color::Yellow,Color::Cyan,4);
  document.ink = Color::Green;
  document.colorRegions = {{{0,0,320,16},Color::Yellow},{{0,16,320,224},Color::Blue}};
  document.pages[0]->background = mad::rgb(17,33,65);
  auto& root = document.pages[0]->root; root->children.clear();
  auto image = document.createNode(NodeType::Image); image->graphicsWidth = image->graphicsHeight = 2;
  image->imageColors = {Color::Red,Color::Green,Color::Blue,Color::Yellow}; image->imageCoverage = {1,0,2,1}; image->imagePixels.clear();
  const auto before = image->imageHash(); image->imageColors[0] = Color::Cyan; image->invalidateImageHash(); assert(image->imageHash() != before);
  root->children.push_back(std::move(image));
  auto drawing = document.createNode(NodeType::Canvas); drawing->commands[0].color = Color::Magenta;
  root->children.push_back(std::move(drawing));
  const char* path = "/tmp/mad-studio-colors.mad"; std::string error;
  assert(saveMadUi(document,path,&error)); Document loaded; assert(loadMadUi(path,loaded,&error));
  assert(loaded.colorMode == mad::ColorMode::RGB && loaded.ink == Color::Green);
  assert(loaded.palette.count == 4 && loaded.palette.colors[2] == Color::Yellow);
  assert(loaded.colorRegions[1].color == Color::Blue && loaded.colorRegions[1].bounds.height == 224);
  assert(loaded.pages[0]->background == document.pages[0]->background);
  assert(loaded.pages[0]->root->children[0]->imageColors[0] == Color::Cyan);
  assert(loaded.pages[0]->root->children[0]->imageCoverage[2] == 2);
  assert(loaded.pages[0]->root->children[1]->commands[0].color == Color::Magenta);
  PreviewRuntime runtime(loaded); mad::RGB565Buffer<320,240> canvas; runtime.render(canvas,0);
  assert(canvas.pixel(1,1) == Color::Cyan && canvas.pixel(2,1) == mad::fromRGB565(mad::rgb565(loaded.pages[0]->background)));
  assert(canvas.pixel(1,2) == Color::Blue);
  for (const auto& profile : kDisplayProfiles) if (profile.colorMode == mad::ColorMode::Zoned) {
    Document bicolor = Document::createDefault(); applyDisplayProfile(bicolor,profile);
    assert(bicolor.displayColors().map(Color::White,0,0) == Color::Yellow);
    assert(bicolor.displayColors().map(Color::White,0,63) == Color::Blue);
  }
  std::ifstream input(path); std::ostringstream bytes; bytes << input.rdbuf(); const std::string valid = bytes.str();
  for (const char* invalid : {"#GG0000","#FFFFF","red"}) {
    std::string json = valid; const auto at = json.find("#00FFFF"); assert(at != std::string::npos);
    json.replace(at,7,invalid); { std::ofstream output(path); output << json; }
    assert(!loadMadUi(path,loaded,&error)); assert(loaded.colorMode == mad::ColorMode::RGB);
  }
  // Legacy monochrome documents keep their appearance and pixel storage.
  { std::ofstream output(path); output << R"({"format":"mad","version":3,"displayProfile":"ssd1306-128x64","screen":{"width":128,"height":64},"pages":[{"background":"off","root":{"type":"VerticalLayout","children":[{"type":"Button","foreground":"on"}]}}]})"; }
  assert(loadMadUi(path,loaded,&error)); assert(loaded.colorMode == mad::ColorMode::Monochrome);
  assert(loaded.pages[0]->root->children[0]->foreground == Color::On);
  std::remove(path);
}
