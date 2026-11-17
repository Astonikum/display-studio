#include <MadDisplayStudio/unicode_text.hpp>
#include <cmath>
#include <map>
#include <algorithm>
#include <sstream>
#include <cstring>
#ifdef __APPLE__
#include <CoreText/CoreText.h>
#include <CoreGraphics/CoreGraphics.h>
#endif
namespace madstudio {
#ifdef __APPLE__
namespace {
std::string stringValue(CFStringRef value) {
  if (!value) return {};
  std::string result(CFStringGetMaximumSizeForEncoding(CFStringGetLength(value),kCFStringEncodingUTF8)+1,0);
  if (!CFStringGetCString(value,&result[0],result.size(),kCFStringEncodingUTF8)) return {};
  result.resize(std::char_traits<char>::length(result.c_str())); return result;
}
CFStringRef cfString(const std::string& value) { return CFStringCreateWithBytes(nullptr,reinterpret_cast<const UInt8*>(value.data()),value.size(),kCFStringEncodingUTF8,false); }
GlyphBitmap rasterize(CTFontRef font,CGGlyph glyph,CGRect box,bool colored,bool pixel) {
  const double left = std::floor(CGRectGetMinX(box)),bottom = std::floor(CGRectGetMinY(box));
  const int width = int(std::ceil(CGRectGetMaxX(box))-left),height = int(std::ceil(CGRectGetMaxY(box))-bottom);
  GlyphBitmap result; if (width <= 0 || height <= 0 || width > 256 || height > 256) return result;
  result.width = width; result.height = height;
  std::vector<uint8_t> rgba(size_t(width)*height*4,0);
  CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
  CGContextRef context = CGBitmapContextCreate(rgba.data(),width,height,8,width*4,space,kCGImageAlphaPremultipliedLast|kCGBitmapByteOrder32Big);
  CGColorSpaceRelease(space); if (!context) { result.width = result.height = 0; return result; }
  CGContextSetRGBFillColor(context,1,1,1,1); CGContextSetShouldAntialias(context,!pixel); CGContextSetShouldSmoothFonts(context,false);
  CGContextSetTextMatrix(context,CGAffineTransformIdentity); const CGPoint origin{-left,-bottom}; CTFontDrawGlyphs(font,&glyph,&origin,1,context); CGContextRelease(context);
  result.mask.assign((size_t(width)*height+7)/8,0);
  if (colored) result.colors.assign(size_t(width)*height*2,0);
  for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
    const size_t source = (size_t(y)*width+x)*4,index = size_t(y)*width+x;
    const uint8_t alpha = rgba[source+3]; if (alpha < 96) continue;
    result.mask[index/8] |= 1u<<(index%8);
    if (colored) {
      const auto channel = [&](int c) { return uint8_t(std::min(255u,unsigned(rgba[source+c])*255/alpha)); };
      const uint16_t rgb = mad::rgb565(mad::rgb(channel(0),channel(1),channel(2)));
      result.colors[index*2] = rgb&255; result.colors[index*2+1] = rgb>>8;
    }
  }
  return result;
}
}
std::shared_ptr<TextAsset> shapeUnicodeText(const Node& node,std::string& error) {
  error.clear();
  if (node.text.size() > 65536 || node.fontSize < 6 || node.fontSize > 96) { error = "Text exceeds 64 KiB or font size is outside 6..96 px"; return {}; }
  CFStringRef whole = cfString(node.text); if (!whole) { error = "Text is not valid UTF-8"; return {}; } CFRelease(whole);
  const bool pixel = usesPixelTypeface(node);
  std::string content = node.text;
  if (pixel) {
    auto compact = shapePixelText(node,error);
    if (compact || !error.empty()) return compact;
    // Register only for this process; the user's installed fonts are untouched.
    static const bool registered = []() {
      bool okay = true;
      for (const char* path : {MAD_PIXEL_FONT_PATH,MAD_PIXEL_UPPER_FONT_PATH}) {
        CFURLRef url = CFURLCreateFromFileSystemRepresentation(nullptr,reinterpret_cast<const UInt8*>(path),std::strlen(path),false);
        CFErrorRef failure = nullptr;
        okay = CTFontManagerRegisterFontsForURL(url,kCTFontManagerScopeProcess,&failure) && okay;
        if (failure) CFRelease(failure); CFRelease(url);
      }
      return okay;
    }();
    if (!registered) { error = "Cannot load bundled pixel Unicode fonts"; return {}; }
  }
  CFStringRef name = cfString(pixel ? "MadPixel" : node.fontFamily);
  CTFontRef font = !pixel && node.fontFamily == "Native System" ? CTFontCreateUIFontForLanguage(kCTFontUIFontSystem,node.fontSize,nullptr) : CTFontCreateWithName(name,pixel ? 16 : node.fontSize,nullptr);
  if (name) CFRelease(name);
  if (!font) { error = "Cannot load font"; return {}; }
  if (pixel) {
    CTFontDescriptorRef upper = CTFontDescriptorCreateWithNameAndSize(CFSTR("UnifontUpper"),16);
    const void* descriptors[] = {upper}; CFArrayRef cascade = CFArrayCreate(nullptr,descriptors,1,&kCFTypeArrayCallBacks);
    const void* keys[] = {kCTFontCascadeListAttribute}; const void* values[] = {cascade};
    CFDictionaryRef attributes = CFDictionaryCreate(nullptr,keys,values,1,&kCFTypeDictionaryKeyCallBacks,&kCFTypeDictionaryValueCallBacks);
    CTFontDescriptorRef descriptor = CTFontDescriptorCreateWithAttributes(attributes);
    CTFontRef withFallback = CTFontCreateCopyWithAttributes(font,16,nullptr,descriptor);
    CFRelease(font); font = withFallback;
    CFRelease(descriptor); CFRelease(attributes); CFRelease(cascade); CFRelease(upper);
  }
  auto asset = std::make_shared<TextAsset>(); asset->key = textKey(node);
  if (!pixel && node.fontFamily != "Native System") {
    CFStringRef requested = cfString(node.fontFamily),family = CTFontCopyFamilyName(font),postscript = CTFontCopyPostScriptName(font);
    if (requested && CFStringCompare(requested,family,kCFCompareCaseInsensitive) != kCFCompareEqualTo && CFStringCompare(requested,postscript,kCFCompareCaseInsensitive) != kCFCompareEqualTo)
      asset->warning = "Font unavailable; using "+stringValue(family);
    if (requested) CFRelease(requested); CFRelease(family); CFRelease(postscript);
  }
  std::map<std::string,uint16_t> cache; double top = 0,maximumWidth = 0; size_t totalBytes = 0;
  size_t begin = 0; unsigned lineCount = 0; bool failed = false;
  do {
    if (++lineCount > 512) { error = "Text exceeds 512 lines"; failed = true; break; }
    const size_t end = content.find('\n',begin);
    std::string paragraph = content.substr(begin,end == std::string::npos ? end : end-begin);
    if (!paragraph.empty() && paragraph.back() == '\r') paragraph.pop_back();
    CFStringRef text = cfString(paragraph);
    const void* keys[] = {kCTFontAttributeName,kCTForegroundColorFromContextAttributeName};
    const void* values[] = {font,kCFBooleanTrue};
    CFDictionaryRef attributes = CFDictionaryCreate(nullptr,keys,values,2,&kCFTypeDictionaryKeyCallBacks,&kCFTypeDictionaryValueCallBacks);
    CFAttributedStringRef styled = CFAttributedStringCreate(nullptr,text,attributes);
    CTLineRef line = CTLineCreateWithAttributedString(styled);
    double ascent = 0,descent = 0,leading = 0; const double advance = CTLineGetTypographicBounds(line,&ascent,&descent,&leading);
    CGRect ink = CTLineGetImageBounds(line,nullptr); if (CGRectIsNull(ink) || CGRectIsInfinite(ink)) ink = CGRectZero;
    if (paragraph.empty()) { ascent = CTFontGetAscent(font); descent = CTFontGetDescent(font); leading = CTFontGetLeading(font); }
    const double leftPad = -std::floor(std::min(0.0,double(CGRectGetMinX(ink))));
    const double baseline = top+std::ceil(std::max(ascent,double(CGRectGetMaxY(ink))));
    const double height = std::ceil(std::max(ascent,double(CGRectGetMaxY(ink)))+std::max(descent,-double(CGRectGetMinY(ink)))+std::max(leading,1.0));
    maximumWidth = std::max(maximumWidth,std::ceil(std::max(advance,double(CGRectGetMaxX(ink))))+leftPad);
    CFArrayRef runs = CTLineGetGlyphRuns(line);
    for (CFIndex r = 0; r < CFArrayGetCount(runs) && !failed; ++r) {
      CTRunRef run = static_cast<CTRunRef>(CFArrayGetValueAtIndex(runs,r));
      CTFontRef runFont = static_cast<CTFontRef>(CFDictionaryGetValue(CTRunGetAttributes(run),kCTFontAttributeName));
      CFStringRef postscript = CTFontCopyPostScriptName(runFont); const std::string family = stringValue(postscript); CFRelease(postscript);
      const CFIndex count = CTRunGetGlyphCount(run); if (count > 8192) { error = "Too many shaped glyphs"; failed = true; break; }
      std::vector<CGGlyph> glyphs(count); std::vector<CGPoint> positions(count); std::vector<CGSize> advances(count); std::vector<CFIndex> indices(count);
      CTRunGetGlyphs(run,CFRangeMake(0,0),glyphs.data()); CTRunGetPositions(run,CFRangeMake(0,0),positions.data());
      CTRunGetAdvances(run,CFRangeMake(0,0),advances.data()); CTRunGetStringIndices(run,CFRangeMake(0,0),indices.data());
      const bool colored = !pixel && (CTFontGetSymbolicTraits(runFont)&kCTFontColorGlyphsTrait) != 0;
      for (CFIndex i = 0; i < count; ++i) {
        if ((!glyphs[i] || family.find("LastResort") != std::string::npos) && advances[i].width > 0 && indices[i] >= 0 && indices[i] < CFStringGetLength(text)) {
          uint32_t cp = CFStringGetCharacterAtIndex(text,indices[i]);
          if (cp >= 0xd800 && cp <= 0xdbff && indices[i]+1 < CFStringGetLength(text)) cp = 0x10000+((cp-0xd800)<<10)+(CFStringGetCharacterAtIndex(text,indices[i]+1)-0xdc00);
          if (asset->missing.size() < 64 && std::find(asset->missing.begin(),asset->missing.end(),cp) == asset->missing.end()) asset->missing.push_back(cp);
        }
        CGRect box; CTFontGetBoundingRectsForGlyphs(runFont,kCTFontOrientationHorizontal,&glyphs[i],&box,1);
        if (CGRectIsEmpty(box)) continue;
        const std::string glyphKey = family+":"+std::to_string(glyphs[i]); auto found = cache.find(glyphKey); uint16_t index;
        if (found == cache.end()) {
          GlyphBitmap bitmap = rasterize(runFont,glyphs[i],box,colored,pixel); if (!bitmap.width || !bitmap.height) continue;
          totalBytes += bitmap.mask.size()+bitmap.colors.size();
          if (asset->bitmaps.size() >= 4096 || totalBytes > 4*1024*1024) { error = "Text glyph atlas exceeds 4 MiB / 4096 glyphs"; failed = true; break; }
          index = static_cast<uint16_t>(asset->bitmaps.size()); cache[glyphKey] = index; asset->bitmaps.push_back(std::move(bitmap));
        } else index = found->second;
        const double x = leftPad+positions[i].x+std::floor(CGRectGetMinX(box));
        const double y = baseline-positions[i].y-std::ceil(CGRectGetMaxY(box));
        if (x < -32768 || x > 32767 || y < -32768 || y > 32767 || asset->placements.size() >= 8192) { error = "Text layout exceeds coordinate / glyph limits"; failed = true; break; }
        asset->placements.push_back({index,static_cast<int16_t>(std::round(x)),static_cast<int16_t>(std::round(y))});
      }
    }
    top += std::max(height,1.0);
    CFRelease(line); CFRelease(styled); CFRelease(attributes); CFRelease(text);
    if (top > 32767 || maximumWidth > 32767) { error = "Text layout exceeds 32767 pixels"; failed = true; }
    if (end == std::string::npos) break; begin = end+1;
  } while (!failed);
  CFRelease(font); if (failed) return {};
  asset->size = {static_cast<int16_t>(maximumWidth),static_cast<int16_t>(top)};
  if (!asset->missing.empty()) {
    std::ostringstream warning; warning << "Missing glyphs:";
    for (auto cp : asset->missing) warning << " U+" << std::hex << std::uppercase << cp; if (!asset->warning.empty()) asset->warning += "; "; asset->warning += warning.str();
  }
  asset->finalize(); return asset;
}
#else
std::shared_ptr<TextAsset> shapeUnicodeText(const Node& node,std::string& error) {
  if (usesPixelTypeface(node)) { auto compact = shapePixelText(node,error); if (compact || !error.empty()) return compact; }
  error = "Native Unicode shaping is unavailable on this platform; embedded layouts still render"; return {};
}
#endif
}
