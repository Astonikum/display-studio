#include <MadDisplayStudio/image_import.hpp>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>
#include <cctype>

#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STBI_MAX_DIMENSIONS 8192
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define NANOSVG_IMPLEMENTATION
#include "nanosvg.h"
#define NANOSVGRAST_IMPLEMENTATION
#include "nanosvgrast.h"

namespace madstudio {
namespace {
const size_t maxBytes = 8*1024*1024, maxPixels = 16*1024*1024;
float clamp(float v,float low = 0,float high = 1) { return std::max(low,std::min(high,v)); }
uint8_t byte(float value) { return static_cast<uint8_t>(clamp(value)*255+.5f); }
uint64_t mix(uint64_t hash,uint64_t value) { return (hash^value)*1099511628211ULL; }
}
std::shared_ptr<const ImageSource> decodeImage(const std::string& name,const std::vector<uint8_t>& bytes,std::string& error) {
  error.clear();
  if (bytes.empty() || bytes.size() > maxBytes) { error = "Image file must be 1 byte to 8 MiB"; return {}; }
  std::shared_ptr<ImageSource> source(new ImageSource()); source->name = name; source->encoded = bytes;
  int channels = 0;
  std::string suffix = name.size() >= 4 ? name.substr(name.size()-4) : name;
  std::transform(suffix.begin(),suffix.end(),suffix.begin(),[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  const bool svg = suffix == ".svg";
  if (svg) {
    std::string text(bytes.begin(),bytes.end());
    // NanoSVG draws self-contained vector paths; never resolve external resources.
    for (const char* tag : {"<text","<image","<use","<filter","<!ENTITY","<script"})
      if (text.find(tag) != std::string::npos) { error = "SVG uses unsupported text, references or effects; convert them to paths first"; return {}; }
    std::vector<char> xml(text.begin(),text.end()); xml.push_back(0);
    NSVGimage* image = nsvgParse(xml.data(),"px",96);
    if (!image || !image->shapes || !std::isfinite(image->width) || !std::isfinite(image->height) || image->width < 1e-6f || image->height < 1e-6f || image->width > 1e9f || image->height > 1e9f) {
      if (image) nsvgDelete(image); error = "SVG has no supported vector artwork"; return {};
    }
    const float scale = 1024.0f/std::max(image->width,image->height);
    size_t points = 0,shapes = 0; bool valid = true;
    for (NSVGshape* shape = image->shapes; shape && valid; shape = shape->next) {
      if (++shapes > 10000 || !std::isfinite(shape->opacity) || !std::isfinite(shape->strokeWidth) || std::abs(shape->strokeWidth*scale) > 1e5f) { valid = false; break; }
      for (NSVGpath* path = shape->paths; path && valid; path = path->next) {
        if (path->npts < 0 || (points += path->npts) > 1000000) { valid = false; break; }
        for (int i = 0; i < path->npts*2; ++i) if (!std::isfinite(path->pts[i]) || std::abs(path->pts[i]*scale) > 1e6f) { valid = false; break; }
      }
    }
    if (!valid) { nsvgDelete(image); error = "SVG geometry is too large or invalid"; return {}; }
    source->width = std::max(1,int(std::ceil(image->width*scale)));
    source->height = std::max(1,int(std::ceil(image->height*scale)));
    source->rgba.assign(size_t(source->width)*source->height*4,0);
    NSVGrasterizer* raster = nsvgCreateRasterizer();
    if (!raster) { nsvgDelete(image); error = "Cannot allocate SVG rasterizer"; return {}; }
    nsvgRasterize(raster,image,0,0,scale,source->rgba.data(),source->width,source->height,source->width*4);
    nsvgDeleteRasterizer(raster); nsvgDelete(image);
  } else {
    if (!stbi_info_from_memory(bytes.data(),int(bytes.size()),&source->width,&source->height,&channels) ||
        source->width < 1 || source->height < 1 || source->width > 8192 || source->height > 8192 ||
        size_t(source->width)*source->height > maxPixels) { error = "Invalid PNG/JPEG or image exceeds 16 megapixels / 8192 pixels per side"; return {}; }
    stbi_uc* decoded = stbi_load_from_memory(bytes.data(),int(bytes.size()),&source->width,&source->height,&channels,4);
    if (!decoded) { error = "Cannot decode PNG/JPEG"; return {}; }
    source->rgba.assign(decoded,decoded+size_t(source->width)*source->height*4); stbi_image_free(decoded);
  }
  uint64_t hash = 1469598103934665603ULL;
  for (auto value : bytes) hash = mix(hash,value);
  for (auto value : name) hash = mix(hash,static_cast<unsigned char>(value));
  source->fingerprint = hash; return source;
}
bool importImage(Node& node,const std::string& path,const Document& doc,std::string& error) {
  if (node.type != NodeType::Image) { error = "Select an Image component"; return false; }
  std::ifstream file(path,std::ios::binary|std::ios::ate);
  if (!file || file.tellg() <= 0 || file.tellg() > std::streamoff(maxBytes)) { error = "Cannot read image (maximum 8 MiB)"; return false; }
  std::vector<uint8_t> bytes(static_cast<size_t>(file.tellg())); file.seekg(0);
  if (!file.read(reinterpret_cast<char*>(bytes.data()),bytes.size())) { error = "Cannot read image"; return false; }
  const auto slash = path.find_last_of("/\\");
  auto source = decodeImage(slash == std::string::npos ? path : path.substr(slash+1),bytes,error);
  if (!source) return false;
  if (!node.imageSource && node.graphicsWidth == 8 && node.graphicsHeight == 8 && !node.imagePixels.empty() &&
      std::all_of(node.imagePixels.begin(),node.imagePixels.end(),[](mad::MatrixPixel pixel) { return pixel == mad::N; })) {
    const float fit = std::min(1.f,std::min(float(std::min(512,doc.screenWidth))/source->width,float(std::min(512,doc.screenHeight))/source->height));
    node.graphicsWidth = std::max(1,int(std::round(source->width*fit)));
    node.graphicsHeight = std::max(1,int(std::round(source->height*fit)));
  }
  node.imageSource = std::move(source); node.processedImageKey = 0;
  processImage(node,doc); return true;
}
uint64_t imageFilterKey(const Node& node,const Document& doc) {
  if (!node.imageSource) return 0;
  uint64_t key = node.imageSource->fingerprint;
  const auto& f = node.imageFilters;
  for (float value : {f.brightness,f.contrast,f.gamma,f.saturation,f.threshold,f.ditherStrength,f.alphaThreshold}) {
    uint32_t bits; memcpy(&bits,&value,sizeof(bits)); key = mix(key,bits);
  }
  for (uint64_t value : {uint64_t(node.graphicsWidth),uint64_t(node.graphicsHeight),uint64_t(f.fit),uint64_t(f.dither),uint64_t(f.invert),uint64_t(node.background),uint64_t(doc.colorMode),uint64_t(doc.palette.count)}) key = mix(key,value);
  for (auto color : doc.palette.colors) key = mix(key,static_cast<uint32_t>(color));
  return key ? key : 1;
}
bool processImage(Node& node,const Document& doc) {
  if (!node.imageSource || node.type != NodeType::Image) return false;
  const uint64_t key = imageFilterKey(node,doc); if (key == node.processedImageKey) return false;
  const auto& source = *node.imageSource; const auto& f = node.imageFilters;
  const int w = node.graphicsWidth,h = node.graphicsHeight;
  if (w < 1 || w > 512 || h < 1 || h > 512 || source.width <= 0 || source.height <= 0) return false;
  const bool mono = doc.colorMode == mad::ColorMode::Monochrome || doc.colorMode == mad::ColorMode::Zoned;
  node.imagePixels.clear(); node.imageColors.assign(size_t(w)*h,mad::Color::Off); node.imageCoverage.assign(size_t(w)*h,0);
  float sx = float(w)/source.width, sy = float(h)/source.height;
  if (f.fit != 2) sx = sy = f.fit == 1 ? std::max(sx,sy) : std::min(sx,sy);
  const float ox = (w-source.width*sx)*.5f,oy = (h-source.height*sy)*.5f;
  const int samplesX = std::max(1,std::min(4,int(std::ceil(1/sx)))),samplesY = std::max(1,std::min(4,int(std::ceil(1/sy))));
  const float sampleWeight = 1.f/(samplesX*samplesY);
  std::vector<float> errors(size_t(w+2)*3*2,0);
  static const int bayer[16] = {0,8,2,10,12,4,14,6,3,11,1,9,15,7,13,5};
  for (int y = 0; y < h; ++y) {
    const size_t rowSize = size_t(w+2)*3;
    float* current = errors.data()+(y%2)*rowSize; float* next = errors.data()+((y+1)%2)*rowSize;
    std::fill(next,next+rowSize,0);
    for (int x = 0; x < w; ++x) {
      const float px = (x+.5f-ox)/sx-.5f, py = (y+.5f-oy)/sy-.5f;
      if (px < -.5f || py < -.5f || px >= source.width-.5f || py >= source.height-.5f) continue;
      float rgba[4]{};
      for (int ay = 0; ay < samplesY; ++ay) for (int ax = 0; ax < samplesX; ++ax) {
        const float ux = px+((ax+.5f)/samplesX-.5f)/sx,uy = py+((ay+.5f)/samplesY-.5f)/sy;
        const int ix = int(std::floor(ux)),iy = int(std::floor(uy)); const float fx = ux-ix,fy = uy-iy;
        for (int dy = 0; dy < 2; ++dy) for (int dx = 0; dx < 2; ++dx) {
          const size_t i = (size_t(std::max(0,std::min(source.height-1,iy+dy)))*source.width+std::max(0,std::min(source.width-1,ix+dx)))*4;
          const float weight = (dx ? fx : 1-fx)*(dy ? fy : 1-fy)*sampleWeight,alpha = source.rgba[i+3]/255.f;
          rgba[3] += alpha*weight;
          for (int c = 0; c < 3; ++c) rgba[c] += source.rgba[i+c]/255.f*alpha*weight;
        }
      }
      if (rgba[3] <= 0 || rgba[3]+1e-6f < f.alphaThreshold) continue;
      float color[3] = {mad::red(node.background)/255.f,mad::green(node.background)/255.f,mad::blue(node.background)/255.f};
      for (int c = 0; c < 3; ++c) color[c] = rgba[c]+color[c]*(1-rgba[3]);
      const float gray = .2126f*color[0]+.7152f*color[1]+.0722f*color[2];
      for (int c = 0; c < 3; ++c) {
        color[c] = clamp((gray+(color[c]-gray)*f.saturation-.5f)*f.contrast+.5f+f.brightness);
        if (f.gamma != 1) color[c] = std::pow(color[c],1/std::max(.1f,f.gamma));
        if (f.invert) color[c] = 1-color[c];
        if (f.dither == 2) color[c] += current[(x+1)*3+c];
        if (f.dither == 1) color[c] += ((bayer[(y%4)*4+x%4]+.5f)/16-.5f)*f.ditherStrength*(mono ? 1.f : doc.colorMode == mad::ColorMode::Palette ? .5f : 1/31.f);
      }
      mad::Color result;
      if (mono) result = .2126f*color[0]+.7152f*color[1]+.0722f*color[2] >= f.threshold ? mad::Color::White : mad::Color::Black;
      else if (doc.colorMode == mad::ColorMode::Palette) result = doc.palette.colors[doc.palette.index(mad::rgb(byte(color[0]),byte(color[1]),byte(color[2])))];
      else result = mad::fromRGB565(mad::rgb565(mad::rgb(byte(color[0]),byte(color[1]),byte(color[2]))));
      const size_t index = size_t(y)*w+x; node.imageColors[index] = result; node.imageCoverage[index] = 1;
      if (f.dither == 2) {
        const float quantized[3] = {mad::red(result)/255.f,mad::green(result)/255.f,mad::blue(result)/255.f};
        for (int c = 0; c < 3; ++c) {
          const float error = (color[c]-quantized[c])*f.ditherStrength;
          current[(x+2)*3+c] += error*7/16;
          next[x*3+c] += error*3/16; next[(x+1)*3+c] += error*5/16; next[(x+2)*3+c] += error/16;
        }
      }
    }
  }
  node.processedImageKey = key; node.invalidateImageHash(); return true;
}
void refreshImportedImages(Document& doc) {
  const auto visit = [&](const auto& self,Node& n) -> void { processImage(n,doc); for (auto& child : n.children) self(self,*child); };
  for (auto& page : doc.pages) visit(visit,*page->root);
}
std::string imageSourceHex(const ImageSource& source) {
  static const char* digits = "0123456789abcdef"; std::string result; result.reserve(source.encoded.size()*2);
  for (uint8_t byte : source.encoded) { result += digits[byte>>4]; result += digits[byte&15]; } return result;
}
std::shared_ptr<const ImageSource> imageSourceFromHex(const std::string& name,const std::string& hex,std::string& error) {
  if (hex.empty() || hex.size()%2 || hex.size() > maxBytes*2) { error = "Invalid embedded image size"; return {}; }
  const auto digit = [](char c) { return c >= '0' && c <= '9' ? c-'0' : c >= 'a' && c <= 'f' ? c-'a'+10 : c >= 'A' && c <= 'F' ? c-'A'+10 : -1; };
  std::vector<uint8_t> bytes(hex.size()/2);
  for (size_t i = 0; i < bytes.size(); ++i) {
    const int a = digit(hex[i*2]),b = digit(hex[i*2+1]); if (a < 0 || b < 0) { error = "Invalid embedded image data"; return {}; }
    bytes[i] = uint8_t(a*16+b);
  }
  return decodeImage(name,bytes,error);
}
}
