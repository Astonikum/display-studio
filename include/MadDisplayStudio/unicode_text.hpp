#pragma once
#include "madui.hpp"
#include "json.hpp"
#include <MadDisplay/text.hpp>
namespace madstudio {
struct GlyphBitmap {
  uint16_t width = 0,height = 0;
  std::vector<uint8_t> mask,colors;
};
struct TextAsset {
  uint64_t key = 0;
  mad::Size size{0,0};
  std::vector<GlyphBitmap> bitmaps;
  std::vector<mad::TextGlyph> glyphs;
  std::vector<mad::GlyphPlacement> placements;
  std::vector<uint32_t> missing;
  std::string warning;
  mad::TextLayout layout{{0,0},nullptr,0,nullptr,0,nullptr};
  void finalize();
  size_t byteCount() const;
};
std::string textAssetJson(const TextAsset& asset);
bool readTextAsset(const json::Value& value,Node& node,std::string& error);
uint64_t textKey(const Node& node);
bool needsUnicodeFont(const Node& node);
bool usesPixelTypeface(const Node& node);
std::shared_ptr<TextAsset> shapePixelText(const Node& node,std::string& error);
std::shared_ptr<TextAsset> shapeUnicodeText(const Node& node,std::string& error);
void refreshTextAssets(Document& document);
bool exportTextAssets(Document& document,const std::string& path,std::string& error);
std::string hexBytes(const std::vector<uint8_t>& data);
bool unhexBytes(const std::string& value,std::vector<uint8_t>& result,size_t limit);
}
