#include <MadDisplayStudio/unicode_text.hpp>
#include <MadDisplayStudio/runtime.hpp>
#include <MadDisplayStudio/json.hpp>
#include <cassert>
#include <cstring>
#include <fstream>
#include <cstdio>
using namespace madstudio;
using namespace mad;
static std::shared_ptr<TextAsset> shape(const std::string& text) {
  Node node(NodeType::Label); node.text = text; node.fontMode = 2; node.fontSize = 20;
  node.fontFamily = "Native System";
  std::string error; auto result = shapeUnicodeText(node,error); assert(result && error.empty()); return result;
}
int main() {
  for (auto cp : {0x7fu,0x80u,0x7ffu,0x800u,0xffffu,0x10000u,0x10ffffu}) {
    std::string s;
    if (cp < 0x80) s += char(cp);
    else if (cp < 0x800) { s += char(0xc0|(cp>>6)); s += char(0x80|(cp&63)); }
    else if (cp < 0x10000) { s += char(0xe0|(cp>>12)); s += char(0x80|((cp>>6)&63)); s += char(0x80|(cp&63)); }
    else { s += char(0xf0|(cp>>18)); s += char(0x80|((cp>>12)&63)); s += char(0x80|((cp>>6)&63)); s += char(0x80|(cp&63)); }
    const char* p = s.c_str(); assert(nextUtf8Codepoint(p) == cp && !*p);
  }
  for (const char* invalid : {"\xc0\xaf","\xe0\x80\xaf","\xed\xa0\x80","\xf4\x90\x80\x80","\xf5\x80\x80\x80","\xe2\x82","\x80"}) {
    const char* p = invalid; assert(nextUtf8Codepoint(p) == 0xfffd);
  }
  assert(defaultTextSize(u8"Я").width == 5 && defaultTextSize(u8"👩").width == 5);
  assert(defaultTextSize("AA\nA").width == 11 && defaultTextSize("AA\nA").height == 15);
  RGB565Buffer<256,128> bitmap;
  // The default Unicode font must retain the exact firmware 5x7 artwork.
  Node pixel(NodeType::Label); pixel.text = u8"Textы Ёжик!";
  assert(!needsUnicodeFont(pixel));
  std::string pixelError; auto pixelAsset = shapePixelText(pixel,pixelError);
  assert(pixelAsset && pixelError.empty() && pixelAsset->size.height == 7);
  pixel.fontMode = 2; assert(needsUnicodeFont(pixel));
  RGB565Buffer<256,128> direct,prepared;
  for (uint8_t scale : {1,2,3}) {
    direct.clear(); prepared.clear();
    drawDefaultText(direct,0,0,pixel.text.c_str(),scale,Color::White);
    drawTextLayout(prepared,pixelAsset->layout,0,0,scale,Color::White);
    for (int y = 0; y < 128; ++y) for (int x = 0; x < 256; ++x) assert(direct.isOn(x,y) == prepared.isOn(x,y));
  }
  for (uint32_t cp = 0x410; cp <= 0x42f; ++cp) {
    assert(findDefaultGlyph(cp) && findDefaultGlyph(cp+0x20));
    assert(std::memcmp(findDefaultGlyph(cp),findDefaultGlyph(cp+0x20),5) != 0);
    for (unsigned x = 0; x < 5; ++x) assert(!(findDefaultGlyph(cp)[x]&0x80));
  }
  for (char cp = 'A'; cp <= 'Z'; ++cp) {
    assert(std::memcmp(findDefaultGlyph(cp),findDefaultGlyph(cp+'a'-'A'),5) != 0);
    for (unsigned x = 0; x < 5; ++x) assert(!(findDefaultGlyph(cp+'a'-'A')[x]&0x80));
  }
  assert(std::memcmp(findDefaultGlyph('a'),findDefaultGlyph(0x430),5) == 0);
  assert(std::memcmp(findDefaultGlyph('e'),findDefaultGlyph(0x435),5) == 0);
  assert(std::memcmp(findDefaultGlyph('A'),findDefaultGlyph(0x410),5) == 0);
  assert(std::memcmp(findDefaultGlyph('B'),findDefaultGlyph(0x412),5) == 0);
  pixel.text = u8"Textы\nПривет"; pixelAsset = shapePixelText(pixel,pixelError);
  assert(pixelAsset && pixelAsset->size.height == 15 && pixelAsset->size.width == 35);
  pixel.text = u8"世界"; assert(!shapePixelText(pixel,pixelError) && pixelError.empty());
  const uint8_t columns[] = {1,0x80}; std::vector<BitmapGlyph> glyphs;
  for (uint32_t i = 0; i < 300; ++i) glyphs.push_back({0x400+i,1,columns});
  BitmapFont font(16,18,1,glyphs.data(),glyphs.size(),0x400,true);
  assert(findBitmapGlyph(font,0x400+299) == &glyphs[299]);
  drawBitmapText(bitmap,0,0,u8"Я",font,1,Color::White); assert(bitmap.isOn(0,0) && bitmap.isOn(0,15));
  json::Value json; std::string error;
  assert(json::parse(R"("\uD83D\uDE00")",json,error) && json.string == u8"😀");
  assert(json::parse(R"("\u041F\u0440\u0438\u0432\u0435\u0442")",json,error) && json.string == u8"Привет");
  for (const char* invalid : {R"("\uD800")",R"("\uDC00")",R"("\uD800\u0041")", "\"\xed\xa0\x80\"", "\"\xc0\xaf\"", "\"a\nb\""}) assert(!json::parse(invalid,json,error));
#ifdef __APPLE__
  for (const char* text : {u8"Textы",u8"Été Ωμέγα",u8"你好、日本語",u8"مرحبا بالعالم",u8"עברית 123",u8"नमस्ते",u8"😀 🐈 → ∞"}) {
    pixel.text = text; auto a = shapeUnicodeText(pixel,pixelError);
    assert(a && pixelError.empty() && a->missing.empty() && !a->placements.empty());
    for (const auto& glyph : a->bitmaps) assert(glyph.colors.empty());
  }
  pixel.text = u8"لا"; auto pixelJoined = shapeUnicodeText(pixel,pixelError);
  pixel.text = u8"ل ا"; auto pixelSeparated = shapeUnicodeText(pixel,pixelError);
  assert(pixelJoined && pixelSeparated && pixelJoined->placements.size() < pixelSeparated->placements.size());
  pixel.text = u8"AЯ世界"; auto mixedPixel = shapeUnicodeText(pixel,pixelError); assert(mixedPixel);
  for (unsigned i = 0; i < 2; ++i) {
    const auto& glyph = mixedPixel->bitmaps[mixedPixel->placements[i].glyph];
    assert(glyph.width == 5 && glyph.height == 7);
    const auto* columns = findDefaultGlyph(i == 0 ? 'A' : 0x42f);
    for (unsigned y = 0; y < 7; ++y) for (unsigned x = 0; x < 5; ++x) {
      const unsigned bit = y*5+x;
      assert(((glyph.mask[bit/8]>>(bit%8))&1) == ((columns[x]>>y)&1));
    }
  }
  pixel.text = u8"aя世界"; auto mixedLower = shapeUnicodeText(pixel,pixelError); assert(mixedLower);
  for (unsigned i = 0; i < 2; ++i) {
    const auto& glyph = mixedLower->bitmaps[mixedLower->placements[i].glyph];
    assert(glyph.width == 5 && glyph.height == 5);
    const auto* columns = findDefaultGlyph(i == 0 ? 'a' : 0x44f);
    for (unsigned y = 0; y < 5; ++y) for (unsigned x = 0; x < 5; ++x) {
      const unsigned bit = y*5+x;
      assert(((glyph.mask[bit/8]>>(bit%8))&1) == ((columns[x]>>(y+2))&1));
    }
  }
  // A saved old system-font layout is discarded for newly supported Auto text.
  Document migrated = Document::createDefault(); migrated.pages[0]->root->children.clear();
  auto oldLabel = migrated.createNode(NodeType::Label); oldLabel->text = u8"Textы";
  oldLabel->textAsset = shape(u8"Textы"); auto* oldModel = oldLabel.get();
  migrated.pages[0]->root->children.push_back(std::move(oldLabel));
  refreshTextAssets(migrated); assert(!oldModel->textAsset && oldModel->textWarning.empty());
  for (const auto& text : {u8"Привет, мир!",u8"你好、日本語",u8"مرحبا بالعالم",u8"עברית 123",u8"नमस्ते",u8"e\u0301",u8"😀 👩🏽‍💻 🇯🇵"}) {
    auto a = shape(text); assert(a->size.width > 0 && a->size.height > 0 && !a->placements.empty() && a->missing.empty());
    bitmap.clear(); Label label(text); label.setTextLayout(a->layout); label.layout({0,0,256,128}); label.draw(bitmap);
    bool visible = false; for (int y = 0; y < 128; ++y) for (int x = 0; x < 256; ++x) visible |= bitmap.isOn(x,y); assert(visible);
  }
  auto upright = shape("F"); const auto& f = upright->bitmaps[0];
  unsigned topInk = 0,bottomInk = 0;
  for (unsigned y = 0; y < 3; ++y) for (unsigned x = 0; x < f.width; ++x) {
    const unsigned a = y*f.width+x,b = (f.height-1-y)*f.width+x;
    topInk += (f.mask[a/8]>>(a%8))&1; bottomInk += (f.mask[b/8]>>(b%8))&1;
  }
  assert(topInk > bottomInk);
  auto repeated = shape(u8"ЖЖЖЖ"); assert(repeated->bitmaps.size() == 1 && repeated->placements.size() == 4);
  auto joined = shape(u8"لا"), separated = shape(u8"ل ا"); assert(joined->placements.size() < separated->placements.size());
  auto nfc = shape(u8"café"),nfd = shape(u8"cafe\u0301"); assert(nfc->size.width == nfd->size.width);
  auto emoji = shape(u8"👩🏽‍💻"); bool colored = false; for (const auto& glyph : emoji->bitmaps) colored |= !glyph.colors.empty(); assert(colored);
  auto multiline = shape(u8"Привет\n世界"); assert(multiline->size.height > shape(u8"Привет")->size.height);
  Document doc = Document::createDefault(); doc.pages[0]->root->children.clear();
  auto node = doc.createNode(NodeType::Label); node->text = u8"Русский العربية 世界 👩🏽‍💻"; node->fontMode = 2; Node* model = node.get(); doc.pages[0]->root->children.push_back(std::move(node));
  refreshTextAssets(doc); assert(model->textAsset && model->textWarning.empty()); auto savedAsset = model->textAsset;
  refreshTextAssets(doc); assert(model->textAsset == savedAsset);
  const char* file = "/tmp/mad-unicode-roundtrip.mad"; assert(saveMadUi(doc,file,&error)); Document loaded;
  assert(loadMadUi(file,loaded,&error)); auto& restored = *loaded.pages[0]->root->children[0];
  assert(restored.text == model->text && restored.textAsset && restored.textAsset->placements.size() == savedAsset->placements.size());
  for (size_t i = 0; i < savedAsset->bitmaps.size(); ++i) assert(savedAsset->bitmaps[i].mask == restored.textAsset->bitmaps[i].mask && savedAsset->bitmaps[i].colors == restored.textAsset->bitmaps[i].colors);
  auto key = restored.textAsset->key; restored.text += u8" Я"; refreshTextAssets(loaded); assert(restored.textAsset->key != key);
  PreviewRuntime preview(loaded); preview.render(bitmap,0);
  assert(exportTextAssets(loaded,"/tmp/mad-unicode-assets.hpp",error));
  // Oversized and malformed atlases never replace the caller's document.
  auto bad = json::Value{}; assert(json::parse("{\"width\":1,\"height\":1,\"atlas\":[{\"width\":1,\"height\":1,\"mask\":\"zz\"}],\"placements\":[]}",bad,error));
  assert(!readTextAsset(bad,restored,error));
  Label dynamic(u8"Русский"); dynamic.setTextLayout(restored.textAsset->layout); dynamic.setText("DYNAMIC"); assert(!dynamic.textLayout());
  std::remove(file);
#endif
}
