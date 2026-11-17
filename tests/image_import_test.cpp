#include <MadDisplayStudio/image_import.hpp>
#include <MadDisplayStudio/runtime.hpp>
#include <cassert>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <algorithm>
using namespace madstudio;
static std::vector<uint8_t> read(const std::string& file) {
  std::ifstream stream(file,std::ios::binary); return {std::istreambuf_iterator<char>(stream),std::istreambuf_iterator<char>()};
}
int main() {
  const std::string dir = MAD_IMAGE_FIXTURES; std::string error;
  Document doc = Document::createDefault(); doc.pages[0]->root->children.clear();
  auto image = doc.createNode(NodeType::Image); Node& node = *image;
  doc.pages[0]->root->children.push_back(std::move(image));
  assert(importImage(node,dir+"/gray.png",doc,error)); assert(node.imageSource && node.imageSource->width == 8);
  assert(node.graphicsWidth == 8 && node.graphicsHeight == 8 && node.imageColors.size() == 64);
  node.imageFilters.dither = 0; node.imageFilters.threshold = .6f; assert(processImage(node,doc));
  for (auto color : node.imageColors) assert(color == mad::Color::Black);
  assert(!processImage(node,doc)); const auto hash = node.imageHash();
  node.imageFilters.threshold = .4f; assert(processImage(node,doc)); assert(node.imageHash() != hash);
  for (auto color : node.imageColors) assert(color == mad::Color::White);
  node.imageFilters.threshold = .5f; node.imageFilters.dither = 1; processImage(node,doc);
  auto white = std::count(node.imageColors.begin(),node.imageColors.end(),mad::Color::White); assert(white >= 24 && white <= 40);
  node.imageFilters.dither = 2; processImage(node,doc);
  white = std::count(node.imageColors.begin(),node.imageColors.end(),mad::Color::White); assert(white >= 24 && white <= 40);
  node.graphicsWidth = 7; node.graphicsHeight = 5; node.imageFilters.alphaThreshold = 1; node.imageFilters.fit = 2; processImage(node,doc);
  for (auto coverage : node.imageCoverage) assert(coverage == 1);
  node.graphicsWidth = node.graphicsHeight = 8; node.imageFilters.alphaThreshold = .5f; node.imageFilters.fit = 0;
  const auto source = node.imageSource;
  assert(!importImage(node,dir+"/does-not-exist.png",doc,error) && node.imageSource == source);
  assert(!decodeImage("bad.png",{1,2,3,4},error));
  assert(!imageSourceFromHex("bad.png","ZZ",error));
  assert(!imageSourceFromHex("bad.png","0",error));
  assert(decodeImage("photo.jpeg",read(dir+"/gray.jpg"),error));
  auto svg = decodeImage("SHAPE.sVg",read(dir+"/shapes.svg"),error); assert(svg && svg->width == 1024 && svg->height == 512);
  assert(svg->rgba[(256*1024+600)*4+1] != svg->rgba[(256*1024+1000)*4+1]);
  assert(svg->rgba[0] == 255 && svg->rgba[1] == 0 && svg->rgba[3] == 255);
  const std::string bad = "<svg width=\"64\" height=\"64\"><text>x</text></svg>";
  assert(!decodeImage("text.svg",{bad.begin(),bad.end()},error));

  node.imageFilters = {}; node.imageFilters.dither = 0; doc.colorMode = mad::ColorMode::RGB;
  assert(processImage(node,doc));
  assert(node.imageColors[0] != mad::Color::White && node.imageColors[0] != mad::Color::Black);
  const auto normal = node.imageColors[0]; node.imageFilters.brightness = .3f; processImage(node,doc);
  assert(mad::red(node.imageColors[0]) > mad::red(normal));
  node.imageFilters.brightness = 0; node.imageFilters.invert = true; processImage(node,doc);
  assert(mad::red(node.imageColors[0]) < 128);
  doc.colorMode = mad::ColorMode::Palette; doc.palette = mad::Palette(mad::Color::Black,mad::Color::Red);
  processImage(node,doc); for (auto c : node.imageColors) assert(c == mad::Color::Black || c == mad::Color::Red);
  doc.palette = mad::Palette(mad::Color::Blue,mad::Color::Green); assert(processImage(node,doc));
  for (auto c : node.imageColors) assert(c == mad::Color::Blue || c == mad::Color::Green);
  doc.colorMode = mad::ColorMode::Zoned; assert(processImage(node,doc));
  for (auto c : node.imageColors) assert(c == mad::Color::Black || c == mad::Color::White);

  doc.colorMode = mad::ColorMode::RGB; node.graphicsWidth = 4; node.graphicsHeight = 2;
  node.imageFilters = {}; node.imageFilters.fit = 2; node.imageFilters.dither = 0;
  assert(importImage(node,dir+"/alpha.png",doc,error));
  assert(node.imageCoverage[0] == 1 && node.imageCoverage[1] == 0 && node.imageCoverage[2] == 1);
  assert(node.imageColors[0] == mad::Color::Red && mad::blue(node.imageColors[2]) >= 120 && mad::blue(node.imageColors[2]) <= 140);
  node.imageFilters.alphaThreshold = .75f; processImage(node,doc); assert(node.imageCoverage[2] == 0);
  node.graphicsWidth = node.graphicsHeight = 8; node.imageFilters.fit = 0; processImage(node,doc);
  for (int x = 0; x < 8; ++x) assert(node.imageCoverage[x] == 0 && node.imageCoverage[7*8+x] == 0);
  node.imageFilters.fit = 1; node.imageFilters.alphaThreshold = .1f; processImage(node,doc);
  assert(std::any_of(node.imageCoverage.begin(),node.imageCoverage.begin()+8,[](uint8_t c) { return c == 1; }));

  // Embedded originals survive moving/deleting the source file; no disk reads on slider changes.
  node.imageFilters.gamma = 1.5f; node.imageFilters.contrast = 1.25f; processImage(node,doc);
  const auto expected = node.imageColors; const auto expectedCoverage = node.imageCoverage;
  const char* path = "/tmp/mad-import-roundtrip.mad";
  assert(saveMadUi(doc,path,&error)); Document loaded; assert(loadMadUi(path,loaded,&error));
  auto& restored = *loaded.pages[0]->root->children[0];
  assert(restored.imageSource && restored.imageSource->encoded == node.imageSource->encoded);
  assert(restored.imageFilters.gamma == 1.5f && restored.imageFilters.contrast == 1.25f);
  assert(restored.imageColors == expected && restored.imageCoverage == expectedCoverage);
  restored.imageFilters.brightness = -.2f; assert(processImage(restored,loaded));
  PreviewRuntime runtime(loaded); mad::RGB565Buffer<128,64> canvas; runtime.render(canvas,0);
  std::ifstream input(path); std::ostringstream saved; saved << input.rdbuf(); std::string invalid = saved.str();
  const auto at = invalid.find("\"gamma\":1.500000"); assert(at != std::string::npos);
  invalid.replace(at,16,"\"gamma\":-1.000000"); { std::ofstream out(path); out << invalid; }
  assert(!loadMadUi(path,loaded,&error)); assert(restored.imageFilters.gamma == 1.5f);
  std::remove(path);
}
