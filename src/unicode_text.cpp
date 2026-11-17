#include <MadDisplayStudio/unicode_text.hpp>
#include <fstream>
#include <cstdio>
#include <cctype>
#include <set>
#include <map>
#include <algorithm>
namespace madstudio {
void TextAsset::finalize() {
  glyphs.clear(); glyphs.reserve(bitmaps.size());
  for (const auto& b : bitmaps) glyphs.push_back({b.width,b.height,b.mask.data(),b.colors.empty() ? nullptr : b.colors.data()});
  layout = {size,glyphs.data(),static_cast<uint16_t>(glyphs.size()),placements.data(),static_cast<uint16_t>(placements.size()),nullptr};
}
size_t TextAsset::byteCount() const {
  size_t bytes = glyphs.size()*sizeof(mad::TextGlyph)+placements.size()*sizeof(mad::GlyphPlacement)+sizeof(mad::TextLayout);
  for (const auto& bitmap : bitmaps) bytes += bitmap.mask.size()+bitmap.colors.size(); return bytes;
}
uint64_t textKey(const Node& node) {
  uint64_t hash = 1469598103934665603ULL ^ (usesPixelTypeface(node) ? 3u : 1u); // Keep existing native-font assets portable.
  for (const auto* text : {&node.text,&node.fontFamily}) { for (unsigned char c : *text) hash = (hash^c)*1099511628211ULL; hash = (hash^0xff)*1099511628211ULL; }
  hash = (hash^uint64_t(node.fontMode))*1099511628211ULL;
  return (hash^uint64_t(node.fontSize))*1099511628211ULL;
}
bool usesPixelTypeface(const Node& node) {
  return node.fontFamily.empty() || node.fontFamily == "System" || node.fontFamily == "Mad Pixel";
}
std::shared_ptr<TextAsset> shapePixelText(const Node& node,std::string& error) {
  error.clear();
  if (node.text.size() > 65536 || node.text.find('\0') != std::string::npos) { error = "Invalid or oversized pixel text"; return {}; }
  const char* cursor = node.text.c_str();
  while (*cursor) {
    const auto cp = mad::nextUtf8Codepoint(cursor);
    if (cp != '\n' && cp != '\r' && cp != '\t' && !mad::findDefaultGlyph(cp)) return {};
  }
  auto asset = std::make_shared<TextAsset>(); asset->key = textKey(node);
  std::map<const uint8_t*,uint16_t> cache;
  int x = 0,y = 0,width = 0; cursor = node.text.c_str();
  while (*cursor) {
    const auto cp = mad::nextUtf8Codepoint(cursor);
    if (cp == '\r') continue;
    if (cp == '\n') { x = 0; y += mad::kTextHeight; }
    else if (cp == '\t') x += 4*mad::kTextAdvance;
    else {
      const auto* columns = mad::findDefaultGlyph(cp);
      auto found = cache.find(columns); uint16_t index;
      if (found == cache.end()) {
        GlyphBitmap b; b.width = mad::kGlyphWidth; b.height = mad::kGlyphHeight; b.mask.assign(5,0);
        for (unsigned row = 0; row < b.height; ++row) for (unsigned col = 0; col < b.width; ++col) {
          const unsigned bit = row*b.width+col;
          if (columns[col]&(1u<<row)) b.mask[bit/8] |= 1u<<(bit%8);
        }
        index = uint16_t(asset->bitmaps.size()); cache[columns] = index; asset->bitmaps.push_back(std::move(b));
      } else index = found->second;
      if (asset->placements.size() >= 8192) { error = "Too many pixel glyphs"; return {}; }
      asset->placements.push_back({index,int16_t(x),int16_t(y)}); x += mad::kTextAdvance;
    }
    width = std::max(width,x);
    if (width > 32767 || y >= 512*mad::kTextHeight) { error = "Pixel text exceeds layout limits"; return {}; }
  }
  asset->size = {int16_t(width ? width-1 : 0),int16_t(y+mad::kGlyphHeight)};
  asset->finalize(); return asset;
}
bool needsUnicodeFont(const Node& node) {
  if (node.type != NodeType::Label || node.fontMode == 1) return false;
  if (node.fontMode == 2) return true;
  const char* cursor = node.text.c_str();
  while (*cursor) {
    const auto cp = mad::nextUtf8Codepoint(cursor);
    if (cp != '\n' && cp != '\r' && cp != '\t' && !mad::findDefaultGlyph(cp)) return true;
  }
  if (node.text.find('\0') != std::string::npos) return true;
  return false;
}
void refreshTextAssets(Document& doc) {
  const auto visit = [&](const auto& self,Node& n) -> void {
    if (n.type == NodeType::Label) {
      const auto key = textKey(n);
      if (!needsUnicodeFont(n)) {
        n.textAsset.reset(); n.textWarning.clear(); n.processedTextKey = key;
        if (n.fontMode == 1) {
          const char* cursor = n.text.c_str();
          while (*cursor) {
            const auto cp = mad::nextUtf8Codepoint(cursor);
            if (cp != '\n' && cp != '\r' && cp != '\t' && !mad::findDefaultGlyph(cp)) { n.textWarning = "Pixel 5x7 has no glyphs for this text; choose Auto or Unicode"; break; }
          }
        }
      }
      else if (n.processedTextKey != key) {
        std::string error; n.textAsset = shapeUnicodeText(n,error); n.processedTextKey = key;
        n.textWarning = n.textAsset ? n.textAsset->warning : error;
      }
    }
    for (auto& child : n.children) self(self,*child);
  };
  for (auto& p : doc.pages) visit(visit,*p->root);
}
std::string hexBytes(const std::vector<uint8_t>& data) {
  const char* digits = "0123456789abcdef"; std::string result; result.reserve(data.size()*2);
  for (uint8_t c : data) { result += digits[c>>4]; result += digits[c&15]; } return result;
}
bool unhexBytes(const std::string& value,std::vector<uint8_t>& result,size_t limit) {
  if (value.size()%2 || value.size()/2 > limit) return false;
  const auto hex = [](char c) { return c >= '0' && c <= '9' ? c-'0' : c >= 'a' && c <= 'f' ? c-'a'+10 : c >= 'A' && c <= 'F' ? c-'A'+10 : -1; };
  result.resize(value.size()/2);
  for (size_t i = 0; i < result.size(); ++i) { const int a = hex(value[i*2]),b = hex(value[i*2+1]); if (a < 0 || b < 0) return false; result[i] = uint8_t(a*16+b); }
  return true;
}
bool exportTextAssets(Document& doc,const std::string& path,std::string& error) {
  normalizeContent(doc); refreshTextAssets(doc);
  std::ofstream out(path+".tmp"); if (!out) { error = "Cannot create text asset header"; return false; }
  out << R"(#pragma once
#include <MadDisplay/MadDisplay.hpp>
#if defined(__AVR__)
#include <avr/pgmspace.h>
#define MAD_TEXT_FLASH PROGMEM
#define MAD_TEXT_REF(symbol,token) reinterpret_cast<const uint8_t*>(uintptr_t(token))
#if defined(RAMPZ)
#define MAD_TEXT_READ(symbol,index) pgm_read_byte_far(pgm_get_far_address(symbol)+(index))
#else
#define MAD_TEXT_READ(symbol,index) pgm_read_byte((symbol)+(index))
#endif
#else
#define MAD_TEXT_FLASH
#define MAD_TEXT_REF(symbol,token) symbol
#endif
namespace mad_text_assets {
static uint8_t readByte(const uint8_t* p,uint32_t i);
)";
  std::vector<std::string> storage;
  std::map<std::string,unsigned> tokens;
  bool okay = true; std::set<std::string> names;
  const auto emitBytes = [&](const std::string& name,const std::vector<uint8_t>& bytes) {
    storage.push_back(name); tokens[name] = storage.size();
    out << "static const uint8_t " << name << "[] MAD_TEXT_FLASH = {";
    for (size_t i = 0; i < bytes.size(); ++i) { if (i) out << ','; out << unsigned(bytes[i]); }
    out << "};\n";
  };
  const auto visit = [&](const auto& self,Node& node) -> void {
    if (needsUnicodeFont(node)) {
      if (!node.textAsset) { okay = false; error = node.textWarning; return; }
      std::string name = "text_"; for (unsigned char c : node.id) name += std::isalnum(c) ? char(c) : '_';
      if (!names.insert(name).second) { okay = false; error = "Text asset identifiers must be unique"; return; }
      const auto& a = *node.textAsset;
      if (!a.missing.empty()) { okay = false; error = "Cannot export text with missing glyphs: "+node.id; return; }
      for (size_t i = 0; i < a.bitmaps.size(); ++i) {
        emitBytes(name+"_mask_"+std::to_string(i),a.bitmaps[i].mask);
        if (!a.bitmaps[i].colors.empty()) emitBytes(name+"_color_"+std::to_string(i),a.bitmaps[i].colors);
      }
      out << "static const mad::TextGlyph " << name << "_glyphs[] = {\n";
      for (size_t i = 0; i < a.bitmaps.size(); ++i) {
        const auto& b = a.bitmaps[i]; const std::string mask = name+"_mask_"+std::to_string(i),color = name+"_color_"+std::to_string(i);
        out << '{' << b.width << ',' << b.height << ",MAD_TEXT_REF(" << mask << ',' << tokens[mask] << "),";
        if (b.colors.empty()) out << "nullptr"; else out << "MAD_TEXT_REF(" << color << ',' << tokens[color] << ')';
        out << "},\n";
      }
      if (a.bitmaps.empty()) out << "{0,0,nullptr,nullptr}\n";
      out << "};\nstatic const mad::GlyphPlacement " << name << "_positions[] = {\n";
      for (auto p : a.placements) out << '{' << p.glyph << ',' << p.x << ',' << p.y << "},\n";
      if (a.placements.empty()) out << "{0,0,0}\n";
      out << "};\nstatic const mad::TextLayout " << name << " = {{" << a.size.width << ',' << a.size.height << "}," << name << "_glyphs," << a.bitmaps.size() << ',' << name << "_positions," << a.placements.size() << ",readByte};\n";
    }
    for (auto& child : node.children) self(self,*child);
  };
  for (auto& page : doc.pages) visit(visit,*page->root);
  out << "static uint8_t readByte(const uint8_t* p,uint32_t i) {\n#if defined(__AVR__)\n switch(reinterpret_cast<uintptr_t>(p)) {\n";
  for (size_t i = 0; i < storage.size(); ++i) out << "case " << i+1 << ": return MAD_TEXT_READ(" << storage[i] << ",i);\n";
  out << "default: return 0; }\n#else\nreturn p[i];\n#endif\n}\n}\n#undef MAD_TEXT_FLASH\n#undef MAD_TEXT_REF\n#undef MAD_TEXT_READ\n"; out.close();
  if (storage.size() > 65535) { okay = false; error = "Too many glyph storage blocks for AVR export"; }
  if (!okay || !out) { std::remove((path+".tmp").c_str()); if (error.empty()) error = "Cannot write text assets"; return false; }
  if (std::rename((path+".tmp").c_str(),path.c_str())) { error = "Cannot replace text asset header"; return false; }
  return true;
}
}

namespace madstudio {
namespace {
std::string keyHex(uint64_t key) { const char* digits = "0123456789abcdef"; std::string value(16,'0'); for (int i = 15; i >= 0; --i) { value[i] = digits[key&15]; key >>= 4; } return value; }
}
std::string textAssetJson(const TextAsset& asset) {
  std::string out = "{\"key\":\""+keyHex(asset.key)+"\",\"width\":"+std::to_string(asset.size.width)+",\"height\":"+std::to_string(asset.size.height)+",\"atlas\":[";
  for (size_t i = 0; i < asset.bitmaps.size(); ++i) {
    const auto& b = asset.bitmaps[i]; if (i) out += ',';
    out += "{\"width\":"+std::to_string(b.width)+",\"height\":"+std::to_string(b.height)+",\"mask\":\""+hexBytes(b.mask)+"\",\"color\":\""+hexBytes(b.colors)+"\"}";
  }
  out += "],\"placements\":[";
  for (size_t i = 0; i < asset.placements.size(); ++i) { const auto p = asset.placements[i]; if (i) out += ','; out += '['+std::to_string(p.glyph)+','+std::to_string(p.x)+','+std::to_string(p.y)+']'; }
  out += "],\"missing\":[";
  for (size_t i = 0; i < asset.missing.size(); ++i) { if (i) out += ','; out += std::to_string(asset.missing[i]); }
  return out+"]}";
}
bool readTextAsset(const json::Value& value,Node& node,std::string& error) {
  const auto integer = [](const json::Value* v,int low,int high,int& result) {
    if (!v || v->type != json::Value::Number || !(v->number >= low && v->number <= high) || v->number != int(v->number)) return false;
    result = int(v->number); return true;
  };
  auto asset = std::make_shared<TextAsset>(); int width,height;
  const auto* atlas = json::field(value,"atlas"),*positions = json::field(value,"placements");
  if (node.type != NodeType::Label || value.type != json::Value::Object || !integer(json::field(value,"width"),0,32767,width) ||
      !integer(json::field(value,"height"),0,32767,height) || !atlas || atlas->type != json::Value::Array || atlas->array.size() > 4096 ||
      !positions || positions->type != json::Value::Array || positions->array.size() > 8192) { error = "Invalid text layout"; return false; }
  asset->size = {int16_t(width),int16_t(height)}; size_t bytes = 0;
  for (const auto& item : atlas->array) {
    GlyphBitmap bitmap; int w,h;
    if (!integer(json::field(item,"width"),1,256,w) || !integer(json::field(item,"height"),1,256,h) ||
        !unhexBytes(json::stringField(item,"mask",""),bitmap.mask,(size_t(w)*h+7)/8) || bitmap.mask.size() != (size_t(w)*h+7)/8 ||
        !unhexBytes(json::stringField(item,"color",""),bitmap.colors,size_t(w)*h*2) || (!bitmap.colors.empty() && bitmap.colors.size() != size_t(w)*h*2)) {
      error = "Invalid text glyph bitmap"; return false;
    }
    bytes += bitmap.mask.size()+bitmap.colors.size(); if (bytes > 4*1024*1024) { error = "Text atlas exceeds 4 MiB"; return false; }
    bitmap.width = w; bitmap.height = h; asset->bitmaps.push_back(std::move(bitmap));
  }
  for (const auto& position : positions->array) {
    int glyph,x,y;
    if (position.type != json::Value::Array || position.array.size() != 3 || !integer(&position.array[0],0,int(asset->bitmaps.size())-1,glyph) ||
        !integer(&position.array[1],-32768,32767,x) || !integer(&position.array[2],-32768,32767,y)) { error = "Invalid glyph placement"; return false; }
    asset->placements.push_back({uint16_t(glyph),int16_t(x),int16_t(y)});
  }
  if (const auto* missing = json::field(value,"missing")) {
    if (missing->type != json::Value::Array || missing->array.size() > 64) { error = "Invalid missing-glyph list"; return false; }
    for (const auto& v : missing->array) { int cp; if (!integer(&v,0,0x10ffff,cp) || (cp >= 0xd800 && cp <= 0xdfff)) { error = "Invalid missing-glyph scalar"; return false; } asset->missing.push_back(cp); }
  }
  asset->key = textKey(node);
  if (json::stringField(value,"key","") != keyHex(asset->key)) return true; // Edited text/style: reshape from source.
  if (!asset->missing.empty()) asset->warning = "Missing glyphs: "+std::to_string(asset->missing.size());
  asset->finalize(); node.textAsset = asset; node.processedTextKey = asset->key; node.textWarning = asset->warning; return true;
}
}
