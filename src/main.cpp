#define SDL_MAIN_HANDLED
#define GL_SILENCE_DEPRECATION

#include <SDL.h>
#include <OpenGL/gl.h>

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <cerrno>
#include <sys/stat.h>
#include <iomanip>
#include <memory>
#include <string>

#include <MadDisplay/core.hpp>
#include <MadDisplay/components.hpp>
#include <MadDisplayStudio/madui.hpp>
#include <MadDisplayStudio/runtime.hpp>
#include <MadDisplayStudio/hardware.hpp>
#include <MadDisplayStudio/preview.hpp>
#include <MadDisplayStudio/image_import.hpp>
#include <MadDisplayStudio/file_dialog.hpp>
#include <MadDisplayStudio/unicode_text.hpp>
#include <chrono>
#include <array>

#include "imgui.h"
#include "imgui_internal.h"
#include "phosphor.hpp"
#include "backends/imgui_impl_opengl2.h"
#include "backends/imgui_impl_sdl2.h"
#include "misc/cpp/imgui_stdlib.h"

namespace {

using madstudio::Document;
using madstudio::Node;
using madstudio::NodeType;
using madstudio::Page;

bool isContainer(NodeType type) {
  return madstudio::isContentControl(type) || madstudio::isLayout(type);
}

using madstudio::PreviewCanvas;

bool beginSearchableCombo(const char* label, const char* preview, ImGuiTextFilter& filter) {
  ImGui::SetNextWindowSizeConstraints(ImVec2(440,0),ImVec2(FLT_MAX,FLT_MAX));
  if (!ImGui::BeginCombo(label, preview, ImGuiComboFlags_HeightLarge)) return false;
  if (ImGui::IsWindowAppearing()) { filter.Clear(); ImGui::SetKeyboardFocusHere(); }
  filter.Draw("Search", -60.0f);
  ImGui::BeginChild("results", ImVec2(0, ImGui::GetTextLineHeightWithSpacing()*8));
  return true;
}

bool editColor(const char* label,mad::Color& color) {
  float value[] = {mad::red(color)/255.0f,mad::green(color)/255.0f,mad::blue(color)/255.0f};
  if (!ImGui::ColorEdit3(label,value,ImGuiColorEditFlags_NoInputs)) return false;
  color = mad::rgb(static_cast<uint8_t>(value[0]*255+.5f),static_cast<uint8_t>(value[1]*255+.5f),static_cast<uint8_t>(value[2]*255+.5f)); return true;
}
ImU32 previewColor(mad::Color color) { return IM_COL32(mad::red(color),mad::green(color),mad::blue(color),255); }

void ensureColorImage(Node& node) {
  if (!node.imageColors.empty()) return;
  node.imageColors.resize(node.imagePixels.size()); node.imageCoverage.resize(node.imagePixels.size());
  for (size_t i = 0; i < node.imagePixels.size(); ++i) {
    const auto pixel = node.imagePixels[i];
    node.imageColors[i] = mad::matrixColor(pixel,false,node.background,node.foreground);
    node.imageCoverage[i] = pixel == mad::N ? 0 : pixel == mad::BF || pixel == mad::WF ? 2 : 1;
  }
  node.imagePixels.clear(); node.invalidateImageHash();
}

void hashMix(uint64_t& hash, uint64_t value) {
  hash ^= value + 0x9e3779b97f4a7c15ULL + (hash << 6) + (hash >> 2);
}

void hashString(uint64_t& hash, const std::string& value) {
  for (size_t i = 0; i < value.size(); ++i) hashMix(hash, static_cast<unsigned char>(value[i]));
  hashMix(hash, 0);
}

void hashLength(uint64_t& hash, const mad::Length& length) {
  hashMix(hash, static_cast<uint64_t>(static_cast<int64_t>(length.value)));
  hashMix(hash, static_cast<uint64_t>(length.unit));
}

void hashNode(uint64_t& hash, const Node& node) {
  hashMix(hash, static_cast<uint64_t>(node.type));
  hashString(hash, node.text); hashMix(hash,node.fontMode); hashMix(hash,node.fontSize); hashString(hash,node.fontFamily);
  hashMix(hash, node.hasFrame);
  hashLength(hash, node.frame.x);
  hashLength(hash, node.frame.y);
  hashLength(hash, node.frame.width);
  hashLength(hash, node.frame.height);
  hashMix(hash, static_cast<uint64_t>(node.widthMode));
  hashMix(hash, static_cast<uint64_t>(node.heightMode));
  hashMix(hash, node.visible);
  hashMix(hash, node.enabled);
  hashMix(hash, static_cast<uint64_t>(node.foreground));
  hashMix(hash, static_cast<uint64_t>(node.background));
  hashMix(hash, static_cast<uint64_t>(node.horizontalAlign));
  hashMix(hash, static_cast<uint64_t>(node.verticalAlign));
  hashMix(hash, node.textScale);
  hashMix(hash, node.padding);
  hashMix(hash, node.filled);
  hashMix(hash, node.checked);
  hashMix(hash, node.on);
  hashMix(hash, node.selected);
  hashMix(hash, node.gap); hashMix(hash,node.columns); hashMix(hash,node.columnGap);
  hashMix(hash, node.scrollable); hashMix(hash, node.animated);
  hashMix(hash, static_cast<uint64_t>(node.entrance)); hashMix(hash, static_cast<uint64_t>(node.easing));
  hashMix(hash,node.graphicsWidth); hashMix(hash,node.graphicsHeight);
  hashMix(hash,node.imageHash());
  hashMix(hash,node.commands.size());
  for (const auto& command : node.commands) {
    hashMix(hash,static_cast<uint64_t>(command.kind)); hashMix(hash,command.rect.x); hashMix(hash,command.rect.y);
    hashMix(hash,command.rect.width); hashMix(hash,command.rect.height);
    hashMix(hash,static_cast<uint64_t>(command.color)); hashMix(hash,command.filled);
  }
  hashMix(hash, node.durationMs); hashMix(hash, node.delayMs); hashMix(hash, node.switchDurationMs);
  hashMix(hash, node.children.size());
  for (size_t i = 0; i < node.children.size(); ++i) hashNode(hash, *node.children[i]);
}

void hashNodeIdentity(uint64_t& hash, const Node& node) {
  hashString(hash, node.id);
  hashString(hash, node.name);
  for (size_t i = 0; i < node.children.size(); ++i) hashNodeIdentity(hash, *node.children[i]);
}

uint64_t documentHash(const Document& document, bool includeSelection = true) {
  uint64_t hash = 1469598103934665603ULL;
  hashString(hash, document.name);
  hashString(hash, document.displayProfile);
  hashMix(hash,static_cast<uint8_t>(document.colorMode)); hashMix(hash,static_cast<uint32_t>(document.ink)); hashMix(hash,document.palette.count);
  for (uint8_t i = 0; i < document.palette.count; ++i) hashMix(hash,static_cast<uint32_t>(document.palette.colors[i]));
  for (const auto& region : document.colorRegions) {
    hashMix(hash,region.bounds.x); hashMix(hash,region.bounds.y); hashMix(hash,region.bounds.width); hashMix(hash,region.bounds.height); hashMix(hash,static_cast<uint32_t>(region.color));
  }
  hashMix(hash, static_cast<uint64_t>(document.screenWidth));
  hashMix(hash, static_cast<uint64_t>(document.screenHeight));
  if (includeSelection) hashMix(hash, document.currentPage);
  hashMix(hash, document.focusMs); hashMix(hash, document.scrollMs);
  hashMix(hash, document.transitionMs); hashMix(hash, static_cast<uint64_t>(document.transitionEasing));
  hashMix(hash, document.fps);
  hashMix(hash, document.safeArea.left); hashMix(hash, document.safeArea.top);
  hashMix(hash, document.safeArea.right); hashMix(hash, document.safeArea.bottom);
  hashMix(hash, document.pages.size());
  for (size_t i = 0; i < document.pages.size(); ++i) {
    hashString(hash, document.pages[i]->name);
    hashMix(hash, static_cast<uint64_t>(document.pages[i]->background));
    hashNode(hash, *document.pages[i]->root);
    hashNodeIdentity(hash, *document.pages[i]->root);
  }
  return hash;
}

bool contains(mad::Rect rect, int x, int y) {
  return x >= rect.x && y >= rect.y && x < rect.x + rect.width && y < rect.y + rect.height;
}

Node* hitTest(Node& node, int x, int y) {
  if (!node.visible) return nullptr;
  if ((node.type == NodeType::Panel || madstudio::isContentControl(node.type)) && !contains(node.previewBounds,x,y)) return nullptr;
  if ((node.scrollable || madstudio::isContentControl(node.type)) && !contains(node.previewContentBounds,x,y))
    return contains(node.previewBounds,x,y) ? &node : nullptr;
  for (size_t i = node.children.size(); i > 0; --i) {
    Node* hit = hitTest(*node.children[i - 1], x, y);
    if (hit) return hit;
  }
  return contains(node.previewBounds,x,y) ? &node : nullptr;
}

Node* findParent(Node& root, Node* target) {
  for (size_t i = 0; i < root.children.size(); ++i) {
    if (root.children[i].get() == target) return &root;
    if (Node* parent = findParent(*root.children[i], target)) return parent;
  }
  return nullptr;
}

Node* findNodeById(Node& root, const std::string& id) {
  if (root.id == id) return &root;
  for (size_t i = 0; i < root.children.size(); ++i)
    if (Node* found = findNodeById(*root.children[i], id)) return found;
  return nullptr;
}

using madstudio::kNodeTypes;

bool iconButton(const char* id, const char* glyph, const char* tooltip, bool active = false) {
  ImGui::PushID(id);
  if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(.42f,.42f,.42f,1));
  const bool clicked = ImGui::Button(glyph, ImVec2(28,26));
  if (active) ImGui::PopStyleColor();
  if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s",tooltip);
  ImGui::PopID();
  return clicked;
}

const char* componentIcon(NodeType type) {
  switch (type) {
    case NodeType::Label: return phosphor::TextT;
    case NodeType::Button: return phosphor::Rectangle;
    case NodeType::Switch: return phosphor::ToggleRight;
    case NodeType::Checkbox: return phosphor::CheckSquare;
    case NodeType::RadioButton: return phosphor::RadioButton;
    case NodeType::Canvas: return phosphor::PaintBrush;
    case NodeType::Image: return phosphor::Image;
    case NodeType::Rectangle: return phosphor::Rectangle;
    case NodeType::Ellipse: return phosphor::Circle;
    case NodeType::Line: return phosphor::LineSegment;
    case NodeType::GridLayout: return phosphor::GridFour;
    case NodeType::VerticalLayout: return phosphor::Rows;
    case NodeType::HorizontalLayout: return phosphor::Columns;
    case NodeType::Container: return phosphor::Cube;
    case NodeType::Panel: return phosphor::Square;
    case NodeType::FreeLayout: return phosphor::Cursor;
  }
  return phosphor::Square;
}

void editMilliseconds(const char* label, uint16_t& value) {
  int milliseconds = value;
  if (ImGui::DragInt(label, &milliseconds, 10, 0, 60000, "%d ms", ImGuiSliderFlags_AlwaysClamp))
    value = static_cast<uint16_t>(milliseconds);
}
void editEasing(const char* label, mad::Easing& easing) {
  const char* names[] = {"Linear", "CubicOut", "CubicInOut", "BackOut", "BounceOut"};
  int choice = static_cast<int>(easing);
  if (ImGui::Combo(label, &choice, names, 5)) easing = static_cast<mad::Easing>(choice);
}

std::unique_ptr<Node> cloneNode(Document& document, const Node& source, bool preserveIdentity = false) {
  std::unique_ptr<Node> clone = document.createNode(source.type);
  if (preserveIdentity) clone->id = source.id;
  clone->name = preserveIdentity ? source.name : source.name + " Copy";
  clone->text = source.text; clone->fontMode = source.fontMode; clone->fontSize = source.fontSize; clone->fontFamily = source.fontFamily;
  clone->textAsset = source.textAsset; clone->processedTextKey = source.processedTextKey; clone->textWarning = source.textWarning;
  clone->hasFrame = source.hasFrame;
  clone->frame = source.frame;
  clone->widthMode = source.widthMode;
  clone->heightMode = source.heightMode;
  clone->visible = source.visible;
  clone->enabled = source.enabled;
  clone->foreground = source.foreground;
  clone->background = source.background;
  clone->horizontalAlign = source.horizontalAlign;
  clone->verticalAlign = source.verticalAlign;
  clone->textScale = source.textScale;
  clone->padding = source.padding;
  clone->filled = source.filled;
  clone->checked = source.checked;
  clone->on = source.on;
  clone->selected = source.selected;
  clone->gap = source.gap; clone->columns = source.columns; clone->columnGap = source.columnGap;
  clone->scrollable = source.scrollable;
  clone->animated = source.animated;
  clone->entrance = source.entrance;
  clone->easing = source.easing;
  clone->durationMs = source.durationMs;
  clone->delayMs = source.delayMs;
  clone->switchDurationMs = source.switchDurationMs;
  clone->graphicsWidth = source.graphicsWidth; clone->graphicsHeight = source.graphicsHeight;
  clone->imageSource = source.imageSource; clone->imageFilters = source.imageFilters;
  clone->imagePixels = source.imagePixels; clone->imageColors = source.imageColors; clone->imageCoverage = source.imageCoverage; clone->commands = source.commands;
  for (size_t i = 0; i < source.children.size(); ++i)
    clone->children.push_back(cloneNode(document, *source.children[i], preserveIdentity));
  return clone;
}

Document cloneDocument(const Document& source) {
  Document clone;
  clone.name = source.name;
  clone.displayProfile = source.displayProfile;
  clone.colorMode = source.colorMode; clone.palette = source.palette; clone.ink = source.ink; clone.colorRegions = source.colorRegions;
  clone.screenWidth = source.screenWidth;
  clone.screenHeight = source.screenHeight;
  clone.currentPage = source.currentPage;
  clone.transitionMs = source.transitionMs; clone.focusMs = source.focusMs; clone.scrollMs = source.scrollMs;
  clone.transitionEasing = source.transitionEasing;
  clone.fps = source.fps;
  clone.safeArea = source.safeArea;
  for (size_t i = 0; i < source.pages.size(); ++i) {
    std::unique_ptr<Page> page(new Page());
    page->name = source.pages[i]->name;
    page->background = source.pages[i]->background;
    page->root = cloneNode(clone, *source.pages[i]->root, true);
    clone.pages.push_back(std::move(page));
  }
  clone.rebuildIdCounter();
  return clone;
}

bool editLength(const char* label, mad::Length& length) {
  bool changed = false;
  ImGui::PushID(label);
  int value = length.value;
  if (length.unit == mad::Unit::Max || length.unit == mad::Unit::Auto) ImGui::TextDisabled(length.unit == mad::Unit::Auto ? "auto" : "max");
  else if (ImGui::DragInt("value", &value, 1.0f, -1024, 1024)) { length.value = static_cast<int16_t>(value); changed = true; }
  ImGui::SameLine();
  int unit = length.unit == mad::Unit::Percent ? 1 : length.unit == mad::Unit::Max ? 2 : length.unit == mad::Unit::Auto ? 3 : 0;
  const char* units[] = {"px", "%", "max", "auto"};
  if (ImGui::Combo("unit", &unit, units, 4)) {
    changed = true;
    length.unit = unit == 1 ? mad::Unit::Percent : unit == 2 ? mad::Unit::Max : unit == 3 ? mad::Unit::Auto : mad::Unit::Pixels;
  }
  ImGui::SameLine();
  ImGui::TextUnformatted(label);
  ImGui::PopID();
  return changed;
}

struct ImportPreviewTexture {
  GLuint texture = 0; uint64_t key = 0;
  void clear() { if (texture) glDeleteTextures(1,&texture); texture = 0; key = 0; }
  void update(uint64_t signature,const std::vector<uint8_t>& rgba,int width,int height) {
    if (texture && key == signature) return;
    GLint binding = 0,alignment = 0; glGetIntegerv(GL_TEXTURE_BINDING_2D,&binding); glGetIntegerv(GL_UNPACK_ALIGNMENT,&alignment);
    if (!texture) glGenTextures(1,&texture);
    glBindTexture(GL_TEXTURE_2D,texture); glPixelStorei(GL_UNPACK_ALIGNMENT,1);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,rgba.data());
    glPixelStorei(GL_UNPACK_ALIGNMENT,alignment); glBindTexture(GL_TEXTURE_2D,binding); key = signature;
  }
};
ImportPreviewTexture sourceTexture,resultTexture;
void drawImageImport(Node& node,const Document& document) {
  static std::string path,error; static const Node* previous = nullptr;
  if (previous != &node) { path.clear(); error.clear(); previous = &node; }
  bool load = false;
  ImGui::SetNextItemWidth(std::max(40.f,ImGui::GetContentRegionAvail().x-36));
  if (ImGui::InputTextWithHint("##imagePath","PNG / JPG / SVG",&path,ImGuiInputTextFlags_EnterReturnsTrue)) load = true;
  ImGui::SameLine();
  if (iconButton("importImage",phosphor::FolderOpen,"Import PNG, JPG or SVG")) {
    const std::string selected = madstudio::chooseImageFile();
    if (!selected.empty()) { path = selected; load = true; }
  }
  if (load) madstudio::importImage(node,path,document,error);
  if (!error.empty()) ImGui::TextWrapped("%s",error.c_str());
  if (!node.imageSource) return;
  ImGui::TextDisabled("%s",node.imageSource->name.c_str());
  auto& f = node.imageFilters; bool changed = false;
  const char* fit[] = {"Contain","Cover","Stretch"},*dither[] = {"None","Bayer 4x4","Floyd-Steinberg"};
  changed |= ImGui::Combo("Fit",&f.fit,fit,3);
  changed |= ImGui::SliderFloat("Brightness",&f.brightness,-1,1,"%.2f",ImGuiSliderFlags_AlwaysClamp);
  changed |= ImGui::SliderFloat("Contrast",&f.contrast,0,3,"%.2f",ImGuiSliderFlags_AlwaysClamp);
  changed |= ImGui::SliderFloat("Gamma",&f.gamma,.1f,4,"%.2f",ImGuiSliderFlags_AlwaysClamp);
  if (document.colorMode == mad::ColorMode::RGB || document.colorMode == mad::ColorMode::Palette)
    changed |= ImGui::SliderFloat("Saturation",&f.saturation,0,2,"%.2f",ImGuiSliderFlags_AlwaysClamp);
  if (document.colorMode == mad::ColorMode::Monochrome || document.colorMode == mad::ColorMode::Zoned)
    changed |= ImGui::SliderFloat("Threshold",&f.threshold,0,1,"%.2f",ImGuiSliderFlags_AlwaysClamp);
  changed |= ImGui::Combo("Dither",&f.dither,dither,3);
  if (f.dither) changed |= ImGui::SliderFloat("Strength",&f.ditherStrength,0,1,"%.2f",ImGuiSliderFlags_AlwaysClamp);
  changed |= ImGui::SliderFloat("Alpha cutoff",&f.alphaThreshold,0,1,"%.2f",ImGuiSliderFlags_AlwaysClamp);
  changed |= ImGui::Checkbox("Invert",&f.invert);
  changed |= editColor("Matte",node.background);
  if (iconButton("resetImage",phosphor::ArrowCounterClockwise,"Reset image filters")) { f = madstudio::ImageFilters{}; changed = true; }
  ImGui::SameLine();
  if (iconButton("bakeImage",phosphor::PaintBrush,"Bake to pixels and detach source")) {
    node.imageSource.reset(); node.processedImageKey = 0; node.invalidateImageHash(); return;
  }
  if (changed) { node.processedImageKey = 0; node.invalidateImageHash(); }
  madstudio::processImage(node,document);
  const auto& source = *node.imageSource;
  if (!sourceTexture.texture || sourceTexture.key != source.fingerprint) {
    const float factor = std::min(1.f,256.f/std::max(source.width,source.height));
    const int w = std::max(1,int(source.width*factor)),h = std::max(1,int(source.height*factor));
    std::vector<uint8_t> rgba(size_t(w)*h*4);
    for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x)
      for (int c = 0; c < 4; ++c) rgba[(size_t(y)*w+x)*4+c] = source.rgba[(size_t(y*source.height/h)*source.width+x*source.width/w)*4+c];
    sourceTexture.update(source.fingerprint,rgba,w,h);
  }
  const uint64_t key = node.imageHash();
  if (!resultTexture.texture || resultTexture.key != key) {
    std::vector<uint8_t> rgba(node.imageColors.size()*4);
    for (size_t i = 0; i < node.imageColors.size(); ++i) {
      rgba[i*4] = mad::red(node.imageColors[i]); rgba[i*4+1] = mad::green(node.imageColors[i]); rgba[i*4+2] = mad::blue(node.imageColors[i]);
      rgba[i*4+3] = node.imageCoverage[i] ? 255 : 0;
    }
    resultTexture.update(key,rgba,node.graphicsWidth,node.graphicsHeight);
  }
  const float side = std::max(30.f,(ImGui::GetContentRegionAvail().x-8)*.5f);
  auto preview = [&](const char* label,GLuint texture,int w,int h) {
    ImGui::BeginGroup(); ImGui::TextDisabled("%s",label);
    const float scale = side/std::max(w,h);
    ImGui::Image(static_cast<ImTextureID>(texture),ImVec2(w*scale,h*scale)); ImGui::EndGroup();
  };
  preview("Source",sourceTexture.texture,source.width,source.height); ImGui::SameLine();
  preview("Result",resultTexture.texture,node.graphicsWidth,node.graphicsHeight);
}

void editNode(Node& node,const Document& document,madstudio::PreviewRuntime* runtime) {
  ImGui::TextDisabled("%s", madstudio::nodeTypeName(node.type));
  ImGui::PushItemWidth(-95);
  ImGui::Separator();
  ImGui::InputText("Name", &node.name);
  if (node.type == NodeType::Label) {
    ImGui::InputTextMultiline("Text",&node.text,ImVec2(-1,54));
    if (iconButton("unicodeEditor",phosphor::TextT,"Unicode editor: IME, right-to-left and emoji")) {
      std::string edited; if (madstudio::editUnicodeText(node.text,edited)) node.text = std::move(edited);
    }
  }

  if (ImGui::CollapsingHeader("Layout", ImGuiTreeNodeFlags_DefaultOpen)) {
    if (madstudio::isLayout(node.type) && runtime && ImGui::BeginCombo("Type",madstudio::nodeTypeName(node.type))) {
      for (auto type : madstudio::kLayoutTypes) {
        const bool selected = node.type == type;
        if (ImGui::Selectable(madstudio::nodeTypeName(type),selected)) runtime->setLayoutType(node,type);
        if (selected) ImGui::SetItemDefaultFocus();
      }
      ImGui::EndCombo();
    }
    mad::Length width = node.widthMode == mad::SizeMode::Max ? mad::Length::fill() : node.hasFrame ? node.frame.width : mad::Length::autoSize();
    mad::Length height = node.heightMode == mad::SizeMode::Max ? mad::Length::fill() : node.hasFrame ? node.frame.height : mad::Length::autoSize();
    const bool widthChanged = editLength("Width",width);
    const bool heightChanged = editLength("Height",height);
    if (widthChanged || heightChanged) {
      node.hasFrame = true; node.frame.width = width; node.frame.height = height;
      node.widthMode = node.heightMode = mad::SizeMode::Fit;
    }
    if (node.type == NodeType::VerticalLayout || node.type == NodeType::HorizontalLayout || node.type == NodeType::Container || node.type == NodeType::Panel || node.type == NodeType::GridLayout) {
      int gap = node.gap;
      if (ImGui::SliderInt(node.type == NodeType::GridLayout ? "Row gap" : "Gap", &gap, 0, 16)) node.gap = static_cast<uint8_t>(gap);
      if (node.type == NodeType::GridLayout) {
        int columns = node.columns,columnGap = node.columnGap;
        if (ImGui::SliderInt("Columns",&columns,1,mad::Layout::kMaxComponents)) node.columns = static_cast<uint8_t>(columns);
        if (ImGui::SliderInt("Column gap",&columnGap,0,16)) node.columnGap = static_cast<uint8_t>(columnGap);
      }
      ImGui::Checkbox("Scroll overflow", &node.scrollable);
    }
  }

  if (node.type == NodeType::Label || madstudio::isContentControl(node.type)) {
    if (ImGui::CollapsingHeader("Appearance")) {
      int scale = node.textScale;
      if (ImGui::SliderInt("Scale", &scale, 1, 3)) node.textScale = static_cast<uint8_t>(scale);
      if (ImGui::IsItemHovered()) ImGui::SetTooltip(node.type == NodeType::Label ? "Glyph scale" : "Scales the whole control, content and spacing together.");
      if (node.type == NodeType::Label) {
        const char* fonts[] = {"Auto (pixel)","Pixel 5x7","Unicode (pixel)"};
        ImGui::Combo("Typeface",&node.fontMode,fonts,3);
        if (madstudio::needsUnicodeFont(node)) {
          if (!madstudio::usesPixelTypeface(node)) ImGui::SliderInt("Font px",&node.fontSize,6,96,"%d",ImGuiSliderFlags_AlwaysClamp);
          else ImGui::TextDisabled("Mad Pixel: 5x7 / Unicode 16px; use Scale");
          char family[257]{}; std::strncpy(family,node.fontFamily.c_str(),sizeof(family)-1);
          if (ImGui::InputText("Font family",family,sizeof(family))) node.fontFamily = family;
          if (ImGui::IsItemHovered()) ImGui::SetTooltip("System / Mad Pixel: pixel Unicode. Native System: system font. Or enter an installed font family.");
        }
        if (!node.textWarning.empty()) ImGui::TextWrapped("%s",node.textWarning.c_str());
        const char* horizontal[] = {"Left", "Center", "Right"};
      const char* vertical[] = {"Top", "Center", "Bottom"};
      int h = static_cast<int>(node.horizontalAlign);
      int v = static_cast<int>(node.verticalAlign);
      if (ImGui::Combo("Horizontal", &h, horizontal, 3)) node.horizontalAlign = static_cast<mad::HorizontalAlign>(h);
      if (ImGui::Combo("Vertical", &v, vertical, 3)) node.verticalAlign = static_cast<mad::VerticalAlign>(v);
      }
      editColor("Foreground",node.foreground);
      if (madstudio::isContentControl(node.type)) {
        int gap = node.gap;
        if (ImGui::SliderInt("Content gap",&gap,0,16)) node.gap = static_cast<uint8_t>(gap);
      }
      if (node.type == NodeType::Button) {
        int padding = node.padding;
        if (ImGui::SliderInt("Padding", &padding, 0, 8)) node.padding = static_cast<uint8_t>(padding);
        ImGui::Checkbox("Filled", &node.filled);
        editColor("Background",node.background);
      }
    }
  }
  if (node.type == NodeType::Checkbox || node.type == NodeType::Switch || node.type == NodeType::RadioButton)
    editColor("Background",node.background);
  if (node.type == NodeType::Checkbox) ImGui::Checkbox("Checked", &node.checked);
  if (node.type == NodeType::Switch) { ImGui::Checkbox("On", &node.on); editMilliseconds("Toggle", node.switchDurationMs); }
  if (node.type == NodeType::RadioButton) ImGui::Checkbox("Selected", &node.selected);
  if (node.type == NodeType::Panel) {
    editColor("Border",node.foreground); editColor("Background",node.background);
  }
  if (ImGui::CollapsingHeader("Animation", ImGuiTreeNodeFlags_DefaultOpen)) {
    int effect = node.animated ? static_cast<int>(node.entrance) + 1 : 0;
    const char* effects[] = {"None", "Rise", "Slide", "Reveal"};
    if (ImGui::Combo("Entrance", &effect, effects, 4)) {
      node.animated = effect != 0;
      if (effect) node.entrance = static_cast<mad::Entrance>(effect - 1);
    }
    if (node.animated) {
      editMilliseconds("Duration", node.durationMs);
      editMilliseconds("Delay", node.delayMs);
      editEasing("Curve", node.easing);
    }
  }
  const bool graphic = node.type == NodeType::Image || node.type == NodeType::Canvas ||
      node.type == NodeType::Rectangle || node.type == NodeType::Ellipse || node.type == NodeType::Line;
  if (graphic && ImGui::CollapsingHeader("Graphics",ImGuiTreeNodeFlags_DefaultOpen)) {
    int size[] = {node.graphicsWidth,node.graphicsHeight};
    if (ImGui::DragInt2(node.type == NodeType::Image ? "Pixels" : "Size",size,1,1,node.type == NodeType::Image ? 512 : 1024,"%d",ImGuiSliderFlags_AlwaysClamp)) {
      if (node.type == NodeType::Image && !node.imageSource) {
        ensureColorImage(node);
        std::vector<mad::Color> resized(size[0]*size[1],mad::Color::Off);
        std::vector<uint8_t> coverage(size[0]*size[1],0);
        for (int y = 0; y < std::min(size[1],node.graphicsHeight); ++y) for (int x = 0; x < std::min(size[0],node.graphicsWidth); ++x) {
          resized[y*size[0]+x] = node.imageColors[y*node.graphicsWidth+x];
          coverage[y*size[0]+x] = node.imageCoverage[y*node.graphicsWidth+x];
        }
        node.imageColors = std::move(resized); node.imageCoverage = std::move(coverage); node.invalidateImageHash();
      }
      node.graphicsWidth = size[0]; node.graphicsHeight = size[1];
    }
    if (node.type == NodeType::Image) drawImageImport(node,document);
    if (node.type == NodeType::Image && !node.imageSource && ImGui::TreeNode("Pixel editor")) {
      static mad::Color brush = mad::Color::White; static bool erase = false, fixed = false;
      editColor("Brush",brush); ImGui::Checkbox("Transparent",&erase); ImGui::SameLine(); ImGui::Checkbox("Fixed",&fixed);
      ImGui::BeginChild("pixels",ImVec2(0,210),ImGuiChildFlags_Borders,ImGuiWindowFlags_HorizontalScrollbar);
      const ImVec2 origin = ImGui::GetCursorScreenPos();
      const float cell = node.graphicsWidth <= 12 ? 18.0f : 12.0f;
      ImGui::InvisibleButton("##bitmap",ImVec2(node.graphicsWidth*cell,node.graphicsHeight*cell));
      if (ImGui::IsItemHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        const int x = static_cast<int>((ImGui::GetIO().MousePos.x-origin.x)/cell);
        const int y = static_cast<int>((ImGui::GetIO().MousePos.y-origin.y)/cell);
        if (x >= 0 && y >= 0 && x < node.graphicsWidth && y < node.graphicsHeight) {
          ensureColorImage(node); const size_t index = y*node.graphicsWidth+x;
          const uint8_t coverage = erase ? 0 : fixed ? 2 : 1;
          if (node.imageColors[index] != brush || node.imageCoverage[index] != coverage) {
            node.imageColors[index] = brush; node.imageCoverage[index] = coverage; node.invalidateImageHash();
          }
        }
      }
      auto* draw = ImGui::GetWindowDrawList();
      const auto clipMin = draw->GetClipRectMin(), clipMax = draw->GetClipRectMax();
      const int left = std::max(0,static_cast<int>((clipMin.x-origin.x)/cell));
      const int top = std::max(0,static_cast<int>((clipMin.y-origin.y)/cell));
      const int right = std::min(node.graphicsWidth,static_cast<int>((clipMax.x-origin.x)/cell)+1);
      const int bottom = std::min(node.graphicsHeight,static_cast<int>((clipMax.y-origin.y)/cell)+1);
      for (int y = top; y < bottom; ++y) for (int x = left; x < right; ++x) {
        const size_t index = y*node.graphicsWidth+x;
        const bool colored = !node.imageColors.empty();
        const auto pixel = colored ? mad::N : node.imagePixels[index];
        const bool transparent = colored ? node.imageCoverage[index] == 0 : pixel == mad::N;
        const mad::Color color = colored ? node.imageColors[index] : mad::matrixColor(pixel,false,node.background,node.foreground);
        const ImU32 value = transparent ? ((x+y)%2 ? IM_COL32(60,60,60,255) : IM_COL32(80,80,80,255)) : previewColor(color);
        const ImVec2 pos(origin.x+x*cell,origin.y+y*cell);
        draw->AddRectFilled(pos,ImVec2(pos.x+cell-1,pos.y+cell-1),value);
      }
      ImGui::EndChild(); ImGui::TreePop();
    } else if (node.type == NodeType::Canvas) {
      static const char* kinds[] = {"Pixel","Line","Rectangle","Ellipse"};
      if (node.commands.size() < 256 && iconButton("addCommand",phosphor::Plus,"Add drawing command")) {
        mad::DrawCommand command; command.rect = {0,0,static_cast<int16_t>(node.graphicsWidth),static_cast<int16_t>(node.graphicsHeight)};
        node.commands.push_back(command);
      }
      for (size_t i = 0; i < node.commands.size(); ++i) {
        ImGui::PushID(static_cast<int>(i));
        auto& command = node.commands[i];
        const bool open = ImGui::TreeNode("command","%s %zu",kinds[static_cast<int>(command.kind)],i+1);
        bool remove = false;
        if (open) {
          int kind = static_cast<int>(command.kind);
          if (ImGui::Combo("Type",&kind,kinds,4)) command.kind = static_cast<mad::DrawingKind>(kind);
          int position[] = {command.rect.x,command.rect.y}, size[] = {command.rect.width,command.rect.height};
          if (ImGui::DragInt2("Position",position,1,-1024,1024,"%d",ImGuiSliderFlags_AlwaysClamp)) { command.rect.x = position[0]; command.rect.y = position[1]; }
          if (command.kind != mad::DrawingKind::Pixel && ImGui::DragInt2("Size",size,1,1,1024,"%d",ImGuiSliderFlags_AlwaysClamp)) { command.rect.width = size[0]; command.rect.height = size[1]; }
          editColor("Color",command.color);
          if (command.kind == mad::DrawingKind::Rectangle || command.kind == mad::DrawingKind::Ellipse) ImGui::Checkbox("Filled",&command.filled);
          remove = iconButton("removeCommand",phosphor::Minus,"Remove command");
          ImGui::TreePop();
        }
        ImGui::PopID();
        if (remove) { node.commands.erase(node.commands.begin()+i); break; }
      }
    } else if (node.type != NodeType::Image) {
      editColor("Color",node.foreground);
      if (node.type != NodeType::Line) ImGui::Checkbox("Filled",&node.filled);
    }
  }
  if (ImGui::CollapsingHeader("Advanced")) {
    ImGui::Checkbox("Visible", &node.visible);
    ImGui::SameLine(); ImGui::Checkbox("Enabled", &node.enabled);
    const bool xChanged = editLength("X", node.frame.x);
    const bool yChanged = editLength("Y", node.frame.y);
    if (xChanged || yChanged) node.hasFrame = true;
  }
  ImGui::PopItemWidth();
}

const char* robotoMonoPath() {
#ifdef MADDISPLAY_STUDIO_FONT_PATH
  static const char* configured = MADDISPLAY_STUDIO_FONT_PATH;
  std::ifstream configuredFile(configured);
  if (configuredFile.good()) return configured;
#endif
  static const char* candidates[] = {
      "RobotoMono-Regular.ttf",
      "assets/RobotoMono-Regular.ttf",
      "/Library/Fonts/RobotoMono-Regular.ttf",
      "/System/Library/Fonts/SFNSMono.ttf"
  };
  for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i) {
    std::ifstream file(candidates[i]);
    if (file.good()) return candidates[i];
  }
  return nullptr;
}

void studioStyle() {
  ImGui::StyleColorsDark();
  for (auto& color : ImGui::GetStyle().Colors) {
    const float gray = (color.x + color.y + color.z) / 3;
    color.x = color.y = color.z = gray;
  }
  auto& style = ImGui::GetStyle();
  style.WindowPadding = ImVec2(8,8); style.FramePadding = ImVec2(5,3);
  style.ItemSpacing = ImVec2(6,6); style.IndentSpacing = 14;
  style.FrameRounding = 1; style.ChildRounding = 0; style.PopupRounding = 0;
  style.WindowBorderSize = 1; style.ChildBorderSize = 1;
  auto* c = style.Colors;
  c[ImGuiCol_WindowBg] = ImVec4(.15f,.15f,.15f,1);
  c[ImGuiCol_ChildBg] = ImVec4(.18f,.18f,.18f,1);
  c[ImGuiCol_FrameBg] = ImVec4(.22f,.22f,.22f,1);
  c[ImGuiCol_FrameBgHovered] = ImVec4(.28f,.28f,.28f,1);
  c[ImGuiCol_FrameBgActive] = ImVec4(.32f,.32f,.32f,1);
  c[ImGuiCol_Button] = c[ImGuiCol_Header] = ImVec4(.26f,.26f,.26f,1);
  c[ImGuiCol_ButtonHovered] = c[ImGuiCol_HeaderHovered] = ImVec4(.34f,.34f,.34f,1);
  c[ImGuiCol_ButtonActive] = c[ImGuiCol_HeaderActive] = ImVec4(.40f,.40f,.40f,1);
  c[ImGuiCol_CheckMark] = c[ImGuiCol_SliderGrab] = ImVec4(.8f,.8f,.8f,1);
  c[ImGuiCol_Text] = ImVec4(.9f,.9f,.9f,1);
  c[ImGuiCol_TextDisabled] = ImVec4(.5f,.5f,.5f,1);
}

class StudioApp {
 public:
  StudioApp() : document_(Document::createDefault()), selected_(nullptr), zoom_(6.0f), path_("design.mad"), status_("Ready"), autosavePending_(false), lastAutosave_(0.0) {
    selected_ = document_.pages[0]->root->children[0].get();
    loadSession();
    resetHistory();
  }

  void persist() { saveSession(); }
  void releaseGraphics() { sourceTexture.clear(); resultTexture.clear(); if (previewTexture_) { glDeleteTextures(1,&previewTexture_); previewTexture_ = 0; } }

  void draw() {
    if (document_.pages.empty()) return;
    if (!ImGui::GetIO().WantTextInput && !ImGui::IsAnyItemActive()) {
      if (ImGui::IsKeyPressed(ImGuiKey_Space,false)) playing_ = !playing_;
      else if ((ImGui::GetIO().KeyCtrl || ImGui::GetIO().KeySuper) && ImGui::IsKeyPressed(ImGuiKey_N, false)) newDocument();
      else if ((ImGui::GetIO().KeyCtrl || ImGui::GetIO().KeySuper) && ImGui::IsKeyPressed(ImGuiKey_S, false)) save();
      else if ((ImGui::GetIO().KeyCtrl || ImGui::GetIO().KeySuper) && ImGui::IsKeyPressed(ImGuiKey_Z, false)) undo();
      else if ((ImGui::GetIO().KeyCtrl || ImGui::GetIO().KeySuper) && ImGui::IsKeyPressed(ImGuiKey_Y, false)) redo();
      else if (ImGui::IsKeyPressed(ImGuiKey_Delete, false)) removeSelected();
    }
    if (!ImGui::GetIO().WantCaptureKeyboard && !ImGui::GetIO().WantTextInput) {
      if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) dispatchPreview(mad::Action::Down);
      if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) dispatchPreview(mad::Action::Up);
      if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) dispatchPreview(mad::Action::Left);
      if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) dispatchPreview(mad::Action::Right);
      if (ImGui::IsKeyPressed(ImGuiKey_Enter)) dispatchPreview(mad::Action::Use);
      if (ImGui::IsKeyPressed(ImGuiKey_Tab)) dispatchPreview(ImGui::GetIO().KeyShift ? mad::Action::Previous : mad::Action::Next);
      if (ImGui::IsKeyPressed(ImGuiKey_Backspace)) dispatchPreview(mad::Action::Back);
    }
    drawMenu();
    drawWorkspace();
    recordHistoryIfChanged();
    handleAutosave();
  }

  private:
  enum class DropPosition { Before, Into, After };
  enum class ContextAction { None, AddChild, Duplicate, Delete, ToggleVisible, ToggleEnabled, MoveUp, MoveDown, Reorder };
  enum class PageAction { None, New, Duplicate, Delete };

  struct HistoryEntry {
    Document document;
    std::string selectedId;
  };

  bool isRoot(const Node& node) const {
    return document_.pages[document_.currentPage]->root.get() == &node;
  }

  void queueContextAction(ContextAction action, Node& node, NodeType type = NodeType::Container) {
    contextScrollable_ = false; contextNodeName_.clear();
    contextAction_ = action;
    contextTarget_ = &node;
    contextType_ = type;
    contextDropPosition_ = DropPosition::Into;
  }

  void queueReorder(Node& target, const char* sourceId, bool after) {
    if (!sourceId || !sourceId[0] || target.id == sourceId) return;
    contextAction_ = ContextAction::Reorder;
    contextTarget_ = &target;
    contextSourceId_ = sourceId;
    contextInsertAfter_ = after;
  }

  void queuePaletteDrop(Node& target, NodeType type, DropPosition position) {
    contextAction_ = ContextAction::AddChild;
    contextTarget_ = &target;
    contextType_ = type;
    contextDropPosition_ = position;
  }

  void queuePageAction(PageAction action, size_t index = 0) {
    pageAction_ = action;
    pageActionIndex_ = index;
  }

  DropPosition dropPosition(const Node& node, ImVec2 itemMin, ImVec2 itemMax) const {
    if (isRoot(node)) return DropPosition::Into;
    const float mouseY = ImGui::GetMousePos().y;
    if (!isContainer(node.type)) return mouseY < (itemMin.y + itemMax.y) * 0.5f ? DropPosition::Before : DropPosition::After;
    const float height = itemMax.y - itemMin.y;
    if (mouseY < itemMin.y + height / 3.0f) return DropPosition::Before;
    if (mouseY > itemMax.y - height / 3.0f) return DropPosition::After;
    return DropPosition::Into;
  }

  void drawDropIndicator(ImVec2 itemMin, ImVec2 itemMax, DropPosition position) const {
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImU32 color = ImGui::GetColorU32(ImGuiCol_DragDropTarget);
    if (position == DropPosition::Into) draw->AddRect(itemMin, itemMax, color, 2.0f, 0, 2.0f);
    else {
      const float y = position == DropPosition::Before ? itemMin.y : itemMax.y;
      draw->AddLine(ImVec2(itemMin.x, y), ImVec2(itemMax.x, y), color, 2.0f);
    }
  }

  void drawNodeTree(Node& node) {
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (&node == selected_) flags |= ImGuiTreeNodeFlags_Selected;
    if (node.children.empty()) flags |= ImGuiTreeNodeFlags_Leaf;
    else flags |= ImGuiTreeNodeFlags_DefaultOpen;
    ImGui::PushID(node.id.c_str());
    const bool open = ImGui::TreeNodeEx("node", flags, "%s%s", node.name.c_str(), node.animated ? "  ~" : "");
    const ImVec2 itemMin = ImGui::GetItemRectMin();
    const ImVec2 itemMax = ImGui::GetItemRectMax();
    if (ImGui::IsItemClicked()) selected_ = &node;
    if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) selected_ = &node;
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
      ImGui::SetDragDropPayload("MADUI_NODE", node.id.c_str(), node.id.size() + 1);
      ImGui::TextUnformatted(node.name.c_str());
      ImGui::EndDragDropSource();
    }
    if (ImGui::BeginDragDropTarget()) {
      if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("MADUI_NODE", ImGuiDragDropFlags_AcceptNoDrawDefaultRect)) {
        const char* sourceId = static_cast<const char*>(payload->Data);
        queueReorder(node, sourceId, ImGui::GetMousePos().y > (itemMin.y + itemMax.y) * 0.5f);
      }
      if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("MADUI_CREATE_NODE", ImGuiDragDropFlags_AcceptNoDrawDefaultRect)) {
        if (payload->DataSize == sizeof(int)) {
          const int value = *static_cast<const int*>(payload->Data);
          if (value >= 0 && value < static_cast<int>(sizeof(kNodeTypes) / sizeof(kNodeTypes[0])))
            queuePaletteDrop(node, kNodeTypes[value], dropPosition(node, itemMin, itemMax));
        }
      }
      if (const ImGuiPayload* payload = ImGui::GetDragDropPayload()) {
        if (payload->IsDataType("MADUI_NODE"))
          drawDropIndicator(itemMin, itemMax, ImGui::GetMousePos().y > (itemMin.y + itemMax.y) * 0.5f ? DropPosition::After : DropPosition::Before);
        else if (payload->IsDataType("MADUI_CREATE_NODE"))
          drawDropIndicator(itemMin, itemMax, dropPosition(node, itemMin, itemMax));
      }
      ImGui::EndDragDropTarget();
    }
    if (ImGui::BeginPopupContextItem("nodeContext")) {
      selected_ = &node;
      if (ImGui::BeginMenu("Add child", isContainer(node.type))) {
        for (size_t i = 0; i < sizeof(kNodeTypes) / sizeof(kNodeTypes[0]); ++i)
          if (ImGui::MenuItem(madstudio::nodeTypeName(kNodeTypes[i]))) queueContextAction(ContextAction::AddChild, node, kNodeTypes[i]);
        ImGui::EndMenu();
      }
      if (ImGui::MenuItem("Duplicate", nullptr, false, !isRoot(node))) queueContextAction(ContextAction::Duplicate, node);
      Node* parent = isRoot(node) ? nullptr : findParent(*document_.pages[document_.currentPage]->root, &node);
      size_t index = 0;
      if (parent) {
        while (index < parent->children.size() && parent->children[index].get() != &node) ++index;
      }
      if (ImGui::MenuItem("Move up", nullptr, false, parent && index > 0)) queueContextAction(ContextAction::MoveUp, node);
      if (ImGui::MenuItem("Move down", nullptr, false, parent && index + 1 < parent->children.size())) queueContextAction(ContextAction::MoveDown, node);
      if (ImGui::MenuItem(node.visible ? "Hide" : "Show")) queueContextAction(ContextAction::ToggleVisible, node);
      if (ImGui::MenuItem(node.enabled ? "Disable" : "Enable")) queueContextAction(ContextAction::ToggleEnabled, node);
      ImGui::Separator();
      if (ImGui::MenuItem("Delete", nullptr, false, !isRoot(node))) queueContextAction(ContextAction::Delete, node);
      ImGui::EndPopup();
    }
    if (open) {
      for (size_t i = 0; i < node.children.size(); ++i) drawNodeTree(*node.children[i]);
      ImGui::TreePop();
    }
    ImGui::PopID();
  }

  void processContextAction() {
    Node* target = contextTarget_;
    const ContextAction action = contextAction_;
    const NodeType type = contextType_;
    const std::string sourceId = contextSourceId_;
    const bool insertAfter = contextInsertAfter_;
    contextAction_ = ContextAction::None;
    contextTarget_ = nullptr;
    contextSourceId_.clear();
    if (!target) return;
    if (action == ContextAction::AddChild) {
      Node* inserted = addNodeAt(*target, type, contextDropPosition_);
      if (inserted) {
        inserted->scrollable = contextScrollable_;
        if (!contextNodeName_.empty()) inserted->name = contextNodeName_;
        if (contextScrollable_) {
          inserted->widthMode = mad::SizeMode::Max;
          inserted->heightMode = mad::SizeMode::Max;
        }
      }
      contextScrollable_ = false; contextNodeName_.clear();
    }
    else if (action == ContextAction::Duplicate) duplicateNode(*target);
    else if (action == ContextAction::MoveUp) moveSibling(*target, -1);
    else if (action == ContextAction::MoveDown) moveSibling(*target, 1);
    else if (action == ContextAction::Reorder) reorderNode(sourceId, *target, insertAfter);
    else if (action == ContextAction::Delete) { selected_ = target; removeSelected(); }
    else if (action == ContextAction::ToggleVisible) { target->visible = !target->visible; requestAutosave(); }
    else if (action == ContextAction::ToggleEnabled) { target->enabled = !target->enabled; requestAutosave(); }
  }

  void drawWorkspace() {
    ImGui::SetNextWindowPos(ImVec2(0, 20), ImGuiCond_Always);
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowSize(ImVec2(display.x, display.y - 20), ImGuiCond_Always);
    ImGui::Begin("MadStudio", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings);
    drawToolbar();
    drawHardwareControls();
    ImGui::Spacing();
    const float width = ImGui::GetContentRegionAvail().x;
    const float left = std::min(260.0f, width * .24f);
    const float right = std::min(280.0f, width * .27f);
    ImGui::BeginChild("Layers", ImVec2(left, 0), true);
    drawPages(); ImGui::Separator(); drawHierarchy();
    ImGui::EndChild(); ImGui::SameLine();
    ImGui::BeginChild("Canvas", ImVec2(std::max(120.0f, width - left - right - 12), 0), true);
    drawPreviewContents();
    ImGui::EndChild(); ImGui::SameLine();
    ImGui::BeginChild("Properties", ImVec2(0, 0), true);
    ImGui::TextUnformatted("Properties"); ImGui::Separator();
    if (selected_) editNode(*selected_,document_,previewRuntime_.get());
    ImGui::EndChild();
    ImGui::End();
  }

  void drawMenu() {
    if (!ImGui::BeginMainMenuBar()) return;
    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem("New UI", "Ctrl+N")) newDocument();
      if (ImGui::MenuItem("Open UI")) load();
      if (ImGui::MenuItem("Save", "Ctrl+S")) save();
      ImGui::InputText("Name", &document_.name);
      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Edit")) {
      if (ImGui::MenuItem("Undo", "Ctrl+Z", false, !undoHistory_.empty())) undo();
      if (ImGui::MenuItem("Redo", "Ctrl+Y", false, !redoHistory_.empty())) redo();
      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Simulation")) {
      int dimensions[] = {document_.screenWidth,document_.screenHeight};
      if (ImGui::DragInt2("Dimensions",dimensions,1,1,1024,"%d",ImGuiSliderFlags_AlwaysClamp)) {
        document_.screenWidth = dimensions[0]; document_.screenHeight = dimensions[1]; document_.displayProfile = "custom";
      }
      const char* modes[] = {"Monochrome","Palette","Fixed color zones","RGB"};
      int mode = static_cast<int>(document_.colorMode);
      if (ImGui::Combo("Colors",&mode,modes,4)) document_.colorMode = static_cast<mad::ColorMode>(mode);
      if (document_.colorMode == mad::ColorMode::Monochrome || document_.colorMode == mad::ColorMode::Zoned) editColor("Emission",document_.ink);
      if (document_.colorMode == mad::ColorMode::Palette) {
        int count = document_.palette.count;
        if (ImGui::SliderInt("Palette size",&count,2,16)) document_.palette.count = static_cast<uint8_t>(count);
        for (int i = 0; i < count; ++i) { ImGui::PushID(i); editColor("Color",document_.palette.colors[i]); ImGui::PopID(); }
      }
      if (document_.colorMode == mad::ColorMode::Zoned) {
        if (document_.colorRegions.size() < 255 && iconButton("addRegion",phosphor::Plus,"Add color region"))
          document_.colorRegions.push_back({{0,0,static_cast<int16_t>(document_.screenWidth),static_cast<int16_t>(document_.screenHeight)},document_.ink});
        for (size_t i = 0; i < document_.colorRegions.size(); ++i) {
          ImGui::PushID(static_cast<int>(i)); auto& region = document_.colorRegions[i]; bool remove = false;
          if (ImGui::TreeNode("region","Region %zu",i+1)) {
            editColor("Color",region.color);
            int bounds[] = {region.bounds.x,region.bounds.y,region.bounds.width,region.bounds.height};
            if (ImGui::DragInt4("Bounds",bounds,1,0,1024,"%d",ImGuiSliderFlags_AlwaysClamp))
              region.bounds = {static_cast<int16_t>(bounds[0]),static_cast<int16_t>(bounds[1]),static_cast<int16_t>(std::max(1,bounds[2])),static_cast<int16_t>(std::max(1,bounds[3]))};
            remove = iconButton("remove",phosphor::Minus,"Remove color region"); ImGui::TreePop();
          }
          ImGui::PopID(); if (remove) { document_.colorRegions.erase(document_.colorRegions.begin()+i); break; }
        }
      }
      ImGui::Separator();
      ImGui::Checkbox("Grid", &showGrid_); ImGui::Checkbox("Fit", &fitPreview_);
      if (!fitPreview_) ImGui::SliderFloat("Zoom", &zoom_, 1,14,"%.0fx");
      ImGui::Separator();
      editMilliseconds("Focus", document_.focusMs);
      editMilliseconds("Scroll", document_.scrollMs);
      editMilliseconds("Transition", document_.transitionMs);
      editEasing("Curve", document_.transitionEasing);
      int fps = document_.fps;
      if (ImGui::SliderInt("FPS", &fps, 1,120)) document_.fps = static_cast<int16_t>(fps);
      int insets[] = {document_.safeArea.left,document_.safeArea.top,document_.safeArea.right,document_.safeArea.bottom};
      if (ImGui::DragInt4("Safe area", insets,1,0,128,"%d",ImGuiSliderFlags_AlwaysClamp))
        document_.safeArea = mad::Insets(insets[0],insets[1],insets[2],insets[3]);
      ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();
  }

  void resetHardware() {
    hardwareClock_.reset(); canvas_.invalidate(); framePending_ = false; renderDue_ = true;
    cpuAccum_ = busAccum_ = wireAccum_ = 0; completedFrames_ = 0;
    lastResourceUs_ = resourceSampleUs_ = simulationUs_; historyOffset_ = 0;
    cpuHistory_.fill(0); busHistory_.fill(0); ramHistory_.fill(0);
    fpsHistory_.fill(0); wireHistory_.fill(0); hostHistory_.fill(0);
  }
  void loadHardware() {
    const char* path = MADDISPLAY_STUDIO_HARDWARE_PATH;
    std::ifstream local("madstudio.hardware.json"); if (local.good()) path = "madstudio.hardware.json";
    if (madstudio::loadHardwareProfiles(path,hardwareProfiles_,hardwareError_)) { hardwareIndex_ = 0; resetHardware(); }
    hardwareLoaded_ = true;
  }
  void drawHardwareControls() {
    if (!hardwareLoaded_) loadHardware();
    ImGui::SetNextItemWidth(245);
    if (beginSearchableCombo("##hardware",hardwareProfiles_[hardwareIndex_].name.c_str(),hardwareFilter_)) {
      bool found = false;
      for (size_t i = 0; i < hardwareProfiles_.size(); ++i) {
        const auto& candidate=hardwareProfiles_[i];
        const std::string search=candidate.name+" "+candidate.architecture+" "+candidate.radio;
        if (!hardwareFilter_.PassFilter(search.c_str())) continue;
        found = true;
        if (ImGui::Selectable(hardwareProfiles_[i].name.c_str(),i == hardwareIndex_)) { hardwareIndex_ = i; resetHardware(); canvas_.invalidate(); requestAutosave(); ImGui::CloseCurrentPopup(); }
      }
      if (!found) ImGui::TextDisabled("No matches");
      ImGui::EndChild();
      ImGui::EndCombo();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Hardware emulation. CPU/RAM estimates, shared dirty-window planning, estimated panel timing. One render thread; blocking bus.");
    ImGui::SameLine();
    if (iconButton("hardwareSettings",phosphor::Cube,"Hardware limits / calibration")) ImGui::OpenPopup("Hardware limits");
    if (ImGui::BeginPopup("Hardware limits")) {
      auto& p = hardwareProfiles_[hardwareIndex_]; bool changed = false;
      ImGui::TextUnformatted(p.name.c_str());
      ImGui::Text("%s %.0f core(s) %.0f MHz",p.architecture.c_str(),p.cores,p.cpuHz/1e6);
      ImGui::Text("SRAM %.0f KiB PSRAM %.1f MiB Flash %.1f MiB",p.ramBytes/1024,p.psramBytes/1048576,p.flashBytes/1048576);
      ImGui::Text("Logic %.1f V Radio %s",p.logicVolts,p.radio.c_str());
      if (!p.notes.empty()) ImGui::TextWrapped("%s",p.notes.c_str());
      ImGui::TextDisabled("Single rendering core. CPU coefficients and reserve are estimates.");
      if (p.cpuHz) {
        auto number = [&](const char* label,double& value,double lo,double hi) {
          ImGui::SetNextItemWidth(140); if (ImGui::InputDouble(label,&value,0,0,"%.0f")) { value = std::isfinite(value) ? std::max(lo,std::min(hi,value)) : lo; changed = true; }
        };
        number("CPU Hz",p.cpuHz,100000,1e10); number("RAM bytes",p.ramBytes,128,1e12);
        number("Reserved RAM",p.reservedBytes,0,p.ramBytes); number("PSRAM bytes",p.psramBytes,0,1e12);
        number("I2C Hz",p.i2cHz,1000,400000); number("SPI Hz",p.spiHz,1000,80000000);
        number("Cycles / fill pixel",p.pixelCycles,.01,10000); number("Cycles / point",p.pointCycles,.01,10000);
        number("Cycles / span",p.spanCycles,.01,10000); number("Cycles / scan pixel",p.inspectCycles,.01,10000);
        number("Cycles / node",p.nodeCycles,1,1e7); number("Cycles / wire byte",p.byteCycles,0,10000);
        number("Packet overhead us",p.packetUs,0,1e6); number("RAM / node",p.nodeBytes,16,65536);
        ImGui::TextDisabled("CPU coefficients need board calibration.");
        ImGui::TextDisabled("RAM includes buffers, estimated scene and reserve.");
      }
      if (ImGui::Button("Reload profiles")) loadHardware();
      if (!hardwareError_.empty()) ImGui::TextWrapped("%s",hardwareError_.c_str());
      if (changed) resetHardware();
      ImGui::EndPopup();
    }
    ImGui::SameLine();
    if (hardwareProfiles_[hardwareIndex_].cpuHz) ImGui::TextDisabled("Device estimates");
    else ImGui::TextDisabled("Unthrottled / host timing");
    if (!hardwareError_.empty()) { ImGui::SameLine(); ImGui::TextUnformatted("Profile error"); }
  }
  void drawResourceGraphs() {
    ImGui::Separator();
    if (memoryExceeded_) ImGui::Text("RAM limit: %.1f / %.1f KiB",memoryBytes_/1024,(hardwareProfiles_[hardwareIndex_].ramBytes+hardwareProfiles_[hardwareIndex_].psramBytes)/1024);
    const float width = std::max(50.0f,(ImGui::GetContentRegionAvail().x-16)/3);
    auto graph = [&](const char* id,const char* unit,const std::array<float,100>& values,float max) {
      const float current = values[(historyOffset_+values.size()-1)%values.size()];
      char label[80]; std::snprintf(label,sizeof(label),"%s %.1f %s",id+2,current,unit);
      ImGui::PlotLines(id,values.data(),values.size(),historyOffset_,label,0,max,ImVec2(width,48));
    };
    // Two rows, fixed 20-second history sampled at 5 Hz; pause freezes everything.
    ImGui::PushStyleColor(ImGuiCol_PlotLines,ImVec4(.75f,.75f,.75f,1));
    graph("##CPU","% est.",cpuHistory_,100); ImGui::SameLine();
    graph("##Bus","% est.",busHistory_,100); ImGui::SameLine();
    graph("##RAM","KiB est.",ramHistory_,std::max(1.0f,float(memoryBytes_/1024*1.2)));
    graph("##FPS","",fpsHistory_,std::max(1.0f,float(document_.fps))); ImGui::SameLine();
    graph("##Wire","KiB/s",wireHistory_,FLT_MAX); ImGui::SameLine();
    graph("##Host","ms",hostHistory_,FLT_MAX);
    ImGui::PopStyleColor();
  }

  void drawToolbar() {
    if (iconButton("new",phosphor::FilePlus,"New UI")) newDocument();
    ImGui::SameLine(); if (iconButton("open",phosphor::FolderOpen,"Open UI")) load();
    ImGui::SameLine(); if (iconButton("save",phosphor::FloppyDisk,"Save design")) save();
    ImGui::SameLine(); ImGui::SetNextItemWidth(220);
    if (ImGui::InputText("##uiPath", &path_)) { requestAutosave(); }
    ImGui::SameLine();
    const char* preview = "Custom display";
    for (const auto& profile : madstudio::kDisplayProfiles)
      if (document_.displayProfile == profile.id) preview = profile.name;
    ImGui::SetNextItemWidth(245);
    if (beginSearchableCombo("##display", preview, displayFilter_)) {
      bool found = false;
      for (const auto& profile : madstudio::kDisplayProfiles) {
        const std::string search=std::string(profile.name)+" "+profile.controller+" "+profile.adapter;
        if (!displayFilter_.PassFilter(search.c_str())) continue;
        found = true;
        if (ImGui::Selectable(profile.name, document_.displayProfile == profile.id)) {
          madstudio::applyDisplayProfile(document_,profile);
          invalidatePreview(); requestAutosave();
          ImGui::CloseCurrentPopup();
        }
        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip("%s\nAdapter %s\nNative %u bpp Wire %u bpp%s",profile.controller,profile.adapter,profile.bits(),profile.wireBits(),profile.epaper() ? "\nE-paper refresh time depends on panel revision and temperature." : "");
        }
      }
      if (!found) ImGui::TextDisabled("No matches");
      ImGui::EndChild();
      ImGui::EndCombo();
    }
    ImGui::SameLine(); if (iconButton("pause",playing_ ? phosphor::Pause : phosphor::Play,playing_ ? "Pause simulation / Space" : "Resume simulation / Space")) playing_ = !playing_;
    ImGui::SameLine(); ImGui::TextDisabled("|");
    ImGui::SameLine(); if (iconButton("left",phosphor::ArrowLeft,"Left")) dispatchPreview(mad::Action::Left);
    ImGui::SameLine(); if (iconButton("up",phosphor::ArrowUp,"Up")) dispatchPreview(mad::Action::Up);
    ImGui::SameLine(); if (iconButton("down",phosphor::ArrowDown,"Down")) dispatchPreview(mad::Action::Down);
    ImGui::SameLine(); if (iconButton("right",phosphor::ArrowRight,"Right")) dispatchPreview(mad::Action::Right);
    ImGui::SameLine(); if (iconButton("previous",phosphor::SkipBack,"Previous")) dispatchPreview(mad::Action::Previous);
    ImGui::SameLine(); if (iconButton("next",phosphor::SkipForward,"Next")) dispatchPreview(mad::Action::Next);
    ImGui::SameLine(); if (iconButton("use",phosphor::Play,"Use")) dispatchPreview(mad::Action::Use);
    ImGui::SameLine(); if (iconButton("back",phosphor::ArrowUUpLeft,"Back")) dispatchPreview(mad::Action::Back);
  }

  void drawHierarchy() {
    ImGui::TextUnformatted("Controls");
    auto tile = [&](NodeType type, bool sameLine) {
      if (sameLine) ImGui::SameLine(0,4);
      const bool clicked = iconButton(madstudio::nodeTypeName(type),componentIcon(type),
          madstudio::nodeTypeName(type));
      if (ImGui::BeginDragDropSource()) {
        const int value = static_cast<int>(type);
        ImGui::SetDragDropPayload("MADUI_CREATE_NODE", &value, sizeof(value));
        ImGui::TextUnformatted(madstudio::nodeTypeName(type)); ImGui::EndDragDropSource();
      }
      if (clicked) {
        Node* target = selected_ ? selected_ : document_.pages[document_.currentPage]->root.get();
        queueContextAction(ContextAction::AddChild,*target,type);
      }
    };
    tile(NodeType::Label,false); tile(NodeType::Button,true); tile(NodeType::Switch,true);
    tile(NodeType::Checkbox,true); tile(NodeType::RadioButton,true);
    ImGui::Separator(); ImGui::TextUnformatted("Graphics");
    tile(NodeType::Canvas,false); tile(NodeType::Image,true); tile(NodeType::Rectangle,true);
    tile(NodeType::Ellipse,true); tile(NodeType::Line,true);
    ImGui::Separator(); ImGui::TextUnformatted("Layouts");
    auto layoutTile = [&](const char* id, NodeType type, bool scrollable, bool sameLine) {
      if (sameLine) ImGui::SameLine(0,4);
      const char* glyph = scrollable ? (type == NodeType::VerticalLayout ? phosphor::ArrowsDownUp : phosphor::ArrowsLeftRight) : componentIcon(type);
      if (iconButton(id,glyph,id)) {
        Node* target = selected_ ? selected_ : document_.pages[document_.currentPage]->root.get();
        queueContextAction(ContextAction::AddChild,*target,type);
        contextScrollable_ = scrollable; contextNodeName_ = id;
      }
    };
    layoutTile("VerticalLayout",NodeType::VerticalLayout,false,false);
    layoutTile("HorizontalLayout",NodeType::HorizontalLayout,false,true);
    layoutTile("GridLayout",NodeType::GridLayout,false,true);
    layoutTile("FreeLayout",NodeType::FreeLayout,false,true);
    layoutTile("Vertical scroll",NodeType::VerticalLayout,true,false);
    layoutTile("Horizontal scroll",NodeType::HorizontalLayout,true,true);
    layoutTile("Box / Container",NodeType::Container,false,true);
    layoutTile("Panel",NodeType::Panel,false,true);
    ImGui::Separator();
    ImGui::BeginChild("hierarchy", ImVec2(0,0), false);
    drawNodeTree(*document_.pages[document_.currentPage]->root);
    ImGui::EndChild();
    processContextAction();
  }

  void drawPages() {
    ImGui::TextUnformatted("Pages"); ImGui::SameLine();
    if (iconButton("newPage",phosphor::Plus,"Add page")) queuePageAction(PageAction::New);
    ImGui::BeginChild("pageList", ImVec2(0,90),true);
    for (size_t i = 0; i < document_.pages.size(); ++i) {
      ImGui::PushID(static_cast<int>(i));
      if (ImGui::Selectable(document_.pages[i]->name.c_str(), document_.currentPage == i)) selectPage(i);
      if (ImGui::BeginPopupContextItem("pageSettings")) {
        ImGui::InputText("Name", &document_.pages[i]->name);
        if (editColor("Background",document_.pages[i]->background)) requestAutosave();
        if (ImGui::MenuItem("Duplicate")) queuePageAction(PageAction::Duplicate,i);
        if (ImGui::MenuItem("Delete", nullptr,false,document_.pages.size() > 1)) queuePageAction(PageAction::Delete,i);
        ImGui::EndPopup();
      }
      ImGui::PopID();
    }
    ImGui::EndChild();
    processPageAction();
  }

  void selectPage(size_t index) {
    if (index >= document_.pages.size()) return;
    const bool samePage = index == document_.currentPage;
    ensurePreviewRuntime();
    previewRuntime_->openPage(index);
    selected_ = document_.pages[index]->root.get();
    if (playing_ || !samePage) renderDue_ = true;
    requestAutosave();
  }

  Node* addNodeAt(Node& target, NodeType type, DropPosition position) {
    Page& page = *document_.pages[document_.currentPage];
    Node* parent = nullptr;
    size_t index = 0;
    if (position == DropPosition::Into && isContainer(target.type)) {
      parent = &target;
      index = parent->children.size();
    } else if (isRoot(target)) {
      parent = page.root.get();
      index = parent->children.size();
    } else {
      parent = findParent(*page.root, &target);
      if (!parent) return nullptr;
      while (index < parent->children.size() && parent->children[index].get() != &target) ++index;
      if (index == parent->children.size()) return nullptr;
      if (position == DropPosition::After) ++index;
    }
    if (!parent || !isContainer(parent->type)) return nullptr;
    if (parent->children.size() >= 16) {
      status_ = "A MadDisplay container can have at most 16 children";
      return nullptr;
    }
    std::unique_ptr<Node> node = document_.createNode(type);
    node->name = madstudio::nodeTypeName(type);
    Node* inserted = node.get();
    parent->children.insert(parent->children.begin() + static_cast<std::ptrdiff_t>(index), std::move(node));
    selected_ = inserted;
    requestAutosave();
    return inserted;
  }

  void duplicateNode(Node& source) {
    if (isRoot(source)) return;
    Page& page = *document_.pages[document_.currentPage];
    Node* parent = findParent(*page.root, &source);
    if (!parent || parent->children.size() >= 16) return;
    std::unique_ptr<Node> copy = cloneNode(document_, source);
    Node* selectedCopy = copy.get();
    for (std::vector<std::unique_ptr<Node> >::iterator it = parent->children.begin(); it != parent->children.end(); ++it) {
      if (it->get() == &source) {
        parent->children.insert(it + 1, std::move(copy));
        selected_ = selectedCopy;
        requestAutosave();
        return;
      }
    }
  }

  void removeSelected() {
    Page& page = *document_.pages[document_.currentPage];
    Node* parent = findParent(*page.root, selected_);
    if (!parent) return;
    for (std::vector<std::unique_ptr<Node> >::iterator it = parent->children.begin(); it != parent->children.end(); ++it) {
      if (it->get() == selected_) { selected_ = parent; parent->children.erase(it); requestAutosave(); return; }
    }
  }

  void moveSibling(Node& node, int direction) {
    if (isRoot(node)) return;
    Page& page = *document_.pages[document_.currentPage];
    Node* parent = findParent(*page.root, &node);
    if (!parent) return;
    for (size_t i = 0; i < parent->children.size(); ++i) {
      if (parent->children[i].get() == &node) {
        const int next = static_cast<int>(i) + direction;
        if (next < 0 || next >= static_cast<int>(parent->children.size())) return;
        std::swap(parent->children[i], parent->children[static_cast<size_t>(next)]);
        selected_ = &node;
        requestAutosave();
        return;
      }
    }
  }

  void reorderNode(const std::string& sourceId, Node& target, bool after) {
    if (isRoot(target)) return;
    Page& page = *document_.pages[document_.currentPage];
    Node* source = findNodeById(*page.root, sourceId);
    if (!source || isRoot(*source) || source == &target) return;
    Node* sourceParent = findParent(*page.root, source);
    Node* targetParent = findParent(*page.root, &target);
    if (!sourceParent || sourceParent != targetParent) {
      status_ = "Drop items next to their siblings";
      return;
    }
    std::vector<std::unique_ptr<Node> >& siblings = sourceParent->children;
    size_t sourceIndex = 0;
    while (sourceIndex < siblings.size() && siblings[sourceIndex].get() != source) ++sourceIndex;
    if (sourceIndex == siblings.size()) return;
    std::unique_ptr<Node> moved = std::move(siblings[sourceIndex]);
    siblings.erase(siblings.begin() + static_cast<std::ptrdiff_t>(sourceIndex));
    size_t targetIndex = 0;
    while (targetIndex < siblings.size() && siblings[targetIndex].get() != &target) ++targetIndex;
    if (targetIndex == siblings.size()) return;
    if (after) ++targetIndex;
    siblings.insert(siblings.begin() + static_cast<std::ptrdiff_t>(targetIndex), std::move(moved));
    selected_ = source;
    requestAutosave();
  }

  void invalidatePreview() {
    previewSignature_ = 0;
    previewRuntime_.reset();
    resetHardware(); canvas_.invalidate();
  }
  void ensurePreviewRuntime() {
    madstudio::normalizeContent(document_);
    madstudio::refreshImportedImages(document_);
    madstudio::refreshTextAssets(document_);
    const uint64_t signature = documentHash(document_, false);
    if (!previewRuntime_ || previewSignature_ != signature) {
      resetHardware(); canvas_.invalidate();
      previewRuntime_.reset(new madstudio::PreviewRuntime(document_));
      previewSignature_ = signature;
      renderDue_ = true;
    }
  }
  void dispatchPreview(mad::Action action) {
    if (!playing_) return;
    ensurePreviewRuntime();
    const size_t previousPage = document_.currentPage;
    if (previewRuntime_->dispatch(action)) {
      if (previousPage != document_.currentPage) selected_ = document_.pages[document_.currentPage]->root.get();
      previewSignature_ = documentHash(document_, false); renderDue_ = true;
      if (action == mad::Action::Use) requestAutosave();
    }
  }
  void updatePreviewTexture() {
    const uint16_t* pixels = canvas_.viewPixels();
    const size_t count = size_t(canvas_.width())*canvas_.height();
    const bool resized = textureWidth_ != canvas_.width() || textureHeight_ != canvas_.height();
    if (!resized && texturePixels_.size() == count && memcmp(texturePixels_.data(),pixels,count*sizeof(uint16_t)) == 0) return;
    GLint texture = 0, alignment = 0; glGetIntegerv(GL_TEXTURE_BINDING_2D,&texture); glGetIntegerv(GL_UNPACK_ALIGNMENT,&alignment);
    if (!previewTexture_) glGenTextures(1,&previewTexture_);
    glBindTexture(GL_TEXTURE_2D,previewTexture_); glPixelStorei(GL_UNPACK_ALIGNMENT,1);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    if (resized) glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,canvas_.width(),canvas_.height(),0,GL_RGB,GL_UNSIGNED_SHORT_5_6_5,pixels);
    else glTexSubImage2D(GL_TEXTURE_2D,0,0,0,canvas_.width(),canvas_.height(),GL_RGB,GL_UNSIGNED_SHORT_5_6_5,pixels);
    glPixelStorei(GL_UNPACK_ALIGNMENT,alignment); glBindTexture(GL_TEXTURE_2D,texture);
    textureWidth_ = canvas_.width(); textureHeight_ = canvas_.height();
    texturePixels_.assign(pixels,pixels+count);
  }
  void drawPreviewContents() {
    ensurePreviewRuntime();
    const double dt = ImGui::GetIO().DeltaTime;
    if (playing_) {
      previewClockUs_ += static_cast<uint32_t>(dt * 1000000.0);
      simulationUs_ += dt*1000000.0;
      frameElapsed_ += dt;
    }
    const double now = simulationUs_;
    const auto& board = hardwareProfiles_[hardwareIndex_];
    bool differential = true;
    if (board.cpuHz && mad::panelSpec(document_.displayProfile.c_str()).kind == mad::PanelKind::RGB) {
      const double both = double(document_.screenWidth)*document_.screenHeight*4+madstudio::sceneMemory(document_,board);
      differential = both <= board.ramBytes+board.psramBytes;
    }
    canvas_.configure(document_,differential);
    madstudio::HardwareCost memory = madstudio::estimateHardware(board,document_,{}, {},differential);
    memoryExceeded_ = memory.memoryExceeded;
    memoryBytes_ = memory.ramBytes;
    cpuAccum_ += hardwareClock_.cpuOverlap(lastResourceUs_,now);
    const double wireTime = hardwareClock_.busOverlap(lastResourceUs_,now);
    busAccum_ += wireTime;
    if (pendingBusUs_ > 0) wireAccum_ += pendingWireBytes_*wireTime/pendingBusUs_;
    lastResourceUs_ = now;
    if (framePending_ && hardwareClock_.ready(now) && playing_) {
      updatePreviewTexture(); framePending_ = false; ++completedFrames_;
    }
    if (!memoryExceeded_ && !framePending_ && hardwareClock_.ready(now) &&
        (renderDue_ || (playing_ && frameElapsed_ >= 1.0 / document_.fps))) {
      const auto start = std::chrono::steady_clock::now();
      previewRuntime_->render(canvas_, previewClockUs_);
      hostMs_ = std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
      const auto cost = madstudio::estimateHardware(board,document_,previewRuntime_->work(),canvas_.transfer(),differential);
      hardwareClock_.submit(now,cost); framePending_ = true;
      pendingBusUs_ = cost.busUs; pendingWireBytes_ = cost.wireBytes;
      if (!board.cpuHz) wireAccum_ += cost.wireBytes;
      if (hardwareClock_.ready(now)) { updatePreviewTexture(); framePending_ = false; ++completedFrames_; }
      renderDue_ = false; frameElapsed_ = fmod(frameElapsed_,1.0/document_.fps);
    }
    if (now-resourceSampleUs_ >= 200000) {
      const double elapsed = now-resourceSampleUs_;
      const size_t i = historyOffset_;
      cpuHistory_[i] = static_cast<float>(cpuAccum_/elapsed*100);
      busHistory_[i] = static_cast<float>(busAccum_/elapsed*100);
      ramHistory_[i] = static_cast<float>(memoryBytes_/1024);
      fpsHistory_[i] = static_cast<float>(completedFrames_*1e6/elapsed);
      wireHistory_[i] = static_cast<float>(wireAccum_/elapsed*1e6/1024);
      hostHistory_[i] = static_cast<float>(hostMs_);
      historyOffset_ = (historyOffset_+1)%cpuHistory_.size();
      cpuAccum_ = busAccum_ = wireAccum_ = 0; completedFrames_ = 0; resourceSampleUs_ = now;
    }
    const float graphHeight = 148.0f;
    ImGui::BeginChild("displayViewport", ImVec2(0, std::max(80.0f,ImGui::GetContentRegionAvail().y-graphHeight)), false, ImGuiWindowFlags_NoScrollWithMouse);
    const ImVec2 available = ImGui::GetContentRegionAvail();
    const float fit = std::max(0.1f, std::min((available.x - 48) / document_.screenWidth, (available.y - 48) / document_.screenHeight));
    const float desired=std::min(fit,fitPreview_ ? 12.0f : zoom_);
    const float scale=desired<1 ? desired : floorf(desired);
    const ImVec2 size(document_.screenWidth * scale, document_.screenHeight * scale);
    ImVec2 origin = ImGui::GetCursorScreenPos();

    ImGui::SetCursorScreenPos(origin);
    ImGui::InvisibleButton("screen", size);
    const bool clicked = ImGui::IsItemClicked();
    if (playing_ && ImGui::IsItemHovered()) {
      const auto& io = ImGui::GetIO();
      float dx = io.MouseWheelH, dy = io.MouseWheel;
      if (io.KeyShift && dx == 0) { dx = dy; dy = 0; }
      if (dx != 0 || dy != 0) {
        const mad::Point position{static_cast<int16_t>((io.MousePos.x-origin.x)/scale),static_cast<int16_t>((io.MousePos.y-origin.y)/scale)};
        const auto pixels = [](float& remainder,float value) {
          remainder = std::max(-1024.0f,std::min(1024.0f,remainder+value*12));
          const int16_t whole = static_cast<int16_t>(remainder); remainder -= whole; return whole;
        };
        if (previewRuntime_->scrollAt(position,{pixels(wheelPixelsX_,-dx),pixels(wheelPixelsY_,-dy)})) renderDue_ = true;
      }
    }

    ImDrawList* draw = ImGui::GetWindowDrawList();
    if (previewTexture_ && !memoryExceeded_) draw->AddImage(static_cast<ImTextureID>(previewTexture_),origin,ImVec2(origin.x+size.x,origin.y+size.y));
    if (framePending_ && canvas_.spec().epaper() && now >= hardwareClock_.panelStartUs()) {
      const double duration=hardwareClock_.readyUs()-hardwareClock_.panelStartUs();
      const double progress=duration>0 ? (now-hardwareClock_.panelStartUs())/duration : 1;
      if (canvas_.transfer().fullRefresh) {
        const bool dark=(static_cast<int>(progress*6)%2)==0;
        draw->AddRectFilled(origin,ImVec2(origin.x+size.x,origin.y+size.y),dark ? IM_COL32(20,20,20,210) : IM_COL32(242,241,228,230));
      }
      ImGui::TextDisabled("Panel %s refresh %.0f%%",canvas_.transfer().fullRefresh ? "full" : "partial",progress*100);
    }
    if (showGrid_ && scale >= 4) {
      for (int x = 0; x <= document_.screenWidth; ++x) draw->AddLine(ImVec2(origin.x + x * scale, origin.y), ImVec2(origin.x + x * scale, origin.y + size.y), IM_COL32(48,48,52,150));
      for (int y = 0; y <= document_.screenHeight; ++y) draw->AddLine(ImVec2(origin.x, origin.y + y * scale), ImVec2(origin.x + size.x, origin.y + y * scale), IM_COL32(48,48,52,150));
    }
    auto* selection = selected_ ? previewRuntime_->find(*selected_) : nullptr;
    if (selected_ && selection && !selection->component->interactive() && selected_->previewBounds.width > 0) {
      const auto r = selected_->previewBounds;
      draw->AddRect(ImVec2(origin.x + r.x * scale, origin.y + r.y * scale),
          ImVec2(origin.x + (r.x + r.width) * scale, origin.y + (r.y + r.height) * scale), IM_COL32(210,210,210,255), 0, 0, 2);
    }
    if (clicked) {
      const auto mouse = ImGui::GetIO().MousePos;
      Node* hit = hitTest(*document_.pages[document_.currentPage]->root,
          static_cast<int>((mouse.x - origin.x) / scale), static_cast<int>((mouse.y - origin.y) / scale));
      if (hit) {
        selected_ = hit;
        if (playing_ && previewRuntime_->activate(*hit)) {
          previewSignature_ = documentHash(document_,false); renderDue_ = true; requestAutosave();
        }
      }
    }
    ImGui::EndChild();
    drawResourceGraphs();
  }

  void newDocument() {
    document_ = Document::createDefault();
    path_ = "design.mad";
    invalidatePreview();
    selected_ = document_.pages[0]->root->children[0].get();
    resetHistory();
    status_ = "New document";
    requestAutosave();
  }

  static bool ensureParentDirectory(const std::string& path) {
    for (size_t slash = path.find('/'); slash != std::string::npos; slash = path.find('/', slash + 1)) {
      const std::string directory = path.substr(0, slash);
      if (directory.empty()) continue;
      if (mkdir(directory.c_str(), 0755) != 0) {
        struct stat info;
        if (errno != EEXIST || stat(directory.c_str(), &info) != 0 || !S_ISDIR(info.st_mode)) return false;
      }
    }
    return true;
  }

  void addPage() {
    std::unique_ptr<Page> page(new Page());
    page->name = "Page " + std::to_string(document_.pages.size() + 1);
    document_.pages.push_back(std::move(page));
    document_.currentPage = document_.pages.size() - 1;
    invalidatePreview();
    selected_ = document_.pages[document_.currentPage]->root.get();
    status_ = "New page";
    requestAutosave();
  }

  void duplicatePage(size_t index) {
    if (index >= document_.pages.size()) return;
    const Page& source = *document_.pages[index];
    std::unique_ptr<Page> copy(new Page());
    copy->name = source.name + " Copy";
    copy->background = source.background;
    copy->root = cloneNode(document_, *source.root);
    document_.pages.insert(document_.pages.begin() + static_cast<std::ptrdiff_t>(index + 1), std::move(copy));
    document_.currentPage = index + 1;
    invalidatePreview();
    selected_ = document_.pages[document_.currentPage]->root.get();
    status_ = "Page duplicated";
    requestAutosave();
  }

  void deletePage(size_t index) {
    if (document_.pages.size() <= 1 || index >= document_.pages.size()) return;
    document_.pages.erase(document_.pages.begin() + static_cast<std::ptrdiff_t>(index));
    invalidatePreview();
    if (document_.currentPage > index) --document_.currentPage;
    else if (document_.currentPage >= document_.pages.size()) document_.currentPage = document_.pages.size() - 1;
    selected_ = document_.pages[document_.currentPage]->root.get();
    status_ = "Page deleted";
    requestAutosave();
  }

  void processPageAction() {
    const PageAction action = pageAction_;
    const size_t index = pageActionIndex_;
    pageAction_ = PageAction::None;
    if (action == PageAction::New) addPage();
    else if (action == PageAction::Duplicate) duplicatePage(index);
    else if (action == PageAction::Delete) deletePage(index);
  }

  HistoryEntry captureHistory() const {
    HistoryEntry entry;
    entry.document = cloneDocument(document_);
    if (selected_) entry.selectedId = selected_->id;
    return entry;
  }

  void resetHistory() {
    undoHistory_.clear();
    redoHistory_.clear();
    historyBaseline_ = cloneDocument(document_);
    historySelectedId_ = selected_ ? selected_->id : std::string();
    historyHash_ = documentHash(document_);
  }

  void recordHistoryIfChanged() {
    const uint64_t hash = documentHash(document_);
    if (hash == historyHash_) return;
    HistoryEntry entry;
    entry.document = std::move(historyBaseline_);
    entry.selectedId = historySelectedId_;
    undoHistory_.push_back(std::move(entry));
    if (undoHistory_.size() > 64) undoHistory_.erase(undoHistory_.begin());
    redoHistory_.clear();
    historyBaseline_ = cloneDocument(document_);
    historySelectedId_ = selected_ ? selected_->id : std::string();
    historyHash_ = hash;
  }

  void restoreHistory(HistoryEntry entry) {
    document_ = std::move(entry.document);
    if (document_.pages.empty()) return;
    if (document_.currentPage >= document_.pages.size()) document_.currentPage = 0;
    selected_ = findNodeById(*document_.pages[document_.currentPage]->root, entry.selectedId);
    if (!selected_) selected_ = document_.pages[document_.currentPage]->root.get();
    invalidatePreview();
    historyBaseline_ = cloneDocument(document_);
    historySelectedId_ = selected_->id;
    historyHash_ = documentHash(document_);
    requestAutosave();
  }

  void undo() {
    if (undoHistory_.empty()) return;
    redoHistory_.push_back(captureHistory());
    HistoryEntry entry = std::move(undoHistory_.back());
    undoHistory_.pop_back();
    restoreHistory(std::move(entry));
  }

  void redo() {
    if (redoHistory_.empty()) return;
    undoHistory_.push_back(captureHistory());
    HistoryEntry entry = std::move(redoHistory_.back());
    redoHistory_.pop_back();
    restoreHistory(std::move(entry));
  }

  void save() {
    std::string error;
    if (path_.size() >= 6 && path_.substr(path_.size()-6) == ".madui") path_.resize(path_.size()-2);
    const std::string uiPath = path_;
    if (!ensureParentDirectory(uiPath)) { status_ = "Cannot create directory"; return; }
    status_ = saveMadUi(document_, uiPath, &error) ? "Saved " + uiPath : "Save failed: " + error;
    requestAutosave();
  }

  void load() {
    Document loaded;
    std::string error;
    if (!loadMadUi(path_, loaded, &error)) { status_ = "Load failed: " + error; return; }
    document_ = std::move(loaded);
    invalidatePreview();
    selected_ = document_.pages[document_.currentPage]->root.get();
    resetHistory();
    status_ = "Loaded " + path_;
    requestAutosave();
  }

  void requestAutosave() { autosavePending_ = true; }

  void handleAutosave() {
    if (ImGui::IsAnyItemActive()) autosavePending_ = true;
    const double now = ImGui::GetTime();
    if (autosavePending_ && now - lastAutosave_ >= 0.75) saveSession();
  }

  void saveSession() {
    std::string error;
    if (!saveMadUi(document_, ".mad_display_studio.autosave.mad", &error)) {
      status_ = "Autosave failed: " + error;
      return;
    }
    std::ofstream session(".mad_display_studio.session", std::ios::out | std::ios::trunc);
    if (!session) {
      status_ = "Autosave failed: cannot write session";
      return;
    }
    session << "version=5\n"
            << "path=" << path_ << "\n"
            << "hardware=" << hardwareProfiles_[hardwareIndex_].id << "\n"
            << "zoom=" << std::setprecision(5) << zoom_ << "\n"
            << "page=" << document_.currentPage << "\n"
            << "selected=" << (selected_ ? selected_->id : std::string()) << "\n";
    if (!session) {
      status_ = "Autosave failed: cannot write session";
      return;
    }
    lastAutosave_ = ImGui::GetTime();
    autosavePending_ = false;
  }

  void loadSession() {
    std::ifstream session(".mad_display_studio.session");
    if (!hardwareLoaded_) loadHardware();
    std::string selectedId;
    int sessionVersion = 0;
    std::string line;
    while (std::getline(session, line)) {
      if (line.compare(0, 8, "version=") == 0) sessionVersion = std::atoi(line.substr(8).c_str());
      else if (line.compare(0, 9, "hardware=") == 0) {
        for (size_t i = 0; i < hardwareProfiles_.size(); ++i) if (hardwareProfiles_[i].id == line.substr(9)) hardwareIndex_ = i;
      }
      else if (line.compare(0, 5, "path=") == 0) path_ = line.substr(5);
      else if (line.compare(0, 5, "zoom=") == 0) zoom_ = static_cast<float>(std::atof(line.substr(5).c_str()));
      else if (line.compare(0, 5, "page=") == 0) document_.currentPage = static_cast<size_t>(std::strtoul(line.substr(5).c_str(), nullptr, 10));
      else if (line.compare(0, 9, "selected=") == 0) selectedId = line.substr(9);
    }
    zoom_ = std::max(1.0f, std::min(14.0f, zoom_));
    std::string error;
    Document restored;
    // Prefer the old UI autosave while upgrading a reference-only session.
    const bool restoredLegacy = sessionVersion < 5 && loadMadUi(".mad_display_studio.autosave.madui", restored, &error);
    if (!restoredLegacy && !loadMadUi(".mad_display_studio.autosave.mad", restored, &error)) return;
    if (path_.size() >= 6 && path_.substr(path_.size()-6) == ".madui") path_.resize(path_.size()-2);
    document_ = std::move(restored);
    if (document_.currentPage >= document_.pages.size()) document_.currentPage = 0;
    selected_ = findNodeById(*document_.pages[document_.currentPage]->root, selectedId);
    if (!selected_) selected_ = document_.pages[document_.currentPage]->root.get();
    status_ = "Session restored";
  }

  Document document_;
  Node* selected_;
  PreviewCanvas canvas_;
  std::vector<madstudio::HardwareProfile> hardwareProfiles_{madstudio::HardwareProfile{}};
  ImGuiTextFilter hardwareFilter_, displayFilter_;
  size_t hardwareIndex_ = 0;
  bool hardwareLoaded_ = false, framePending_ = false, memoryExceeded_ = false;
  std::string hardwareError_;
  madstudio::HardwareClock hardwareClock_;
  double simulationUs_ = 0,lastResourceUs_ = 0,resourceSampleUs_ = 0;
  double cpuAccum_ = 0,busAccum_ = 0,wireAccum_ = 0,memoryBytes_ = 0,hostMs_ = 0;
  double pendingBusUs_ = 0,pendingWireBytes_ = 0;
  unsigned completedFrames_ = 0;
  size_t historyOffset_ = 0;
  std::array<float,100> cpuHistory_{},busHistory_{},ramHistory_{},fpsHistory_{},wireHistory_{},hostHistory_{};
  GLuint previewTexture_ = 0;
  int textureWidth_ = 0, textureHeight_ = 0;
  std::vector<uint16_t> texturePixels_;
  float zoom_;
  std::string path_;
  std::string status_;
  bool autosavePending_;
  double lastAutosave_;
  ContextAction contextAction_ = ContextAction::None;
  bool contextScrollable_ = false;
  std::string contextNodeName_;
  Node* contextTarget_ = nullptr;
  NodeType contextType_ = NodeType::Container;
  DropPosition contextDropPosition_ = DropPosition::Into;
  std::string contextSourceId_;
  bool contextInsertAfter_ = false;
  PageAction pageAction_ = PageAction::None;
  size_t pageActionIndex_ = 0;
  uint64_t previewSignature_ = 0;
  std::unique_ptr<madstudio::PreviewRuntime> previewRuntime_;
  float wheelPixelsX_ = 0, wheelPixelsY_ = 0;
  uint32_t previewClockUs_ = 0;
  double frameElapsed_ = 0;
  bool playing_ = true;
  bool showGrid_ = true, fitPreview_ = true, renderDue_ = true;
  Document historyBaseline_;
  std::string historySelectedId_;
  uint64_t historyHash_ = 0;
  std::vector<HistoryEntry> undoHistory_;
  std::vector<HistoryEntry> redoHistory_;

};

}  // namespace

int main(int argc,char** argv) {
  if (argc == 4 && std::strcmp(argv[1],"--export-text") == 0) {
    Document document; std::string error;
    if (!madstudio::loadMadUi(argv[2],document,&error) || !madstudio::exportTextAssets(document,argv[3],error)) { std::fprintf(stderr,"%s\n",error.c_str()); return 1; }
    std::printf("Text assets exported: %s\n",argv[3]); return 0;
  }
  const bool smoke = argc > 1 && std::strcmp(argv[1],"--smoke") == 0;
  unsigned smokeFrames = 0; GLenum smokeError = GL_NO_ERROR;
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0) return 1;
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window* window = SDL_CreateWindow("Mad Studio", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                        1440, 900, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
  if (!window) { SDL_Quit(); return 1; }
  SDL_GLContext context = SDL_GL_CreateContext(window);
  SDL_GL_MakeCurrent(window, context);
  SDL_GL_SetSwapInterval(1);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = ".mad_studio.ini";
  studioStyle();
  ImFont* textFont = nullptr;
  if (const char* font = robotoMonoPath()) textFont = io.Fonts->AddFontFromFileTTF(font,15.0f,nullptr,io.Fonts->GetGlyphRangesCyrillic());
  if (!textFont) io.Fonts->AddFontDefault();
  ImFontConfig iconConfig; iconConfig.MergeMode = true; iconConfig.PixelSnapH = true;
  iconConfig.GlyphOffset = ImVec2(0,2);
  io.Fonts->AddFontFromFileTTF(MADDISPLAY_STUDIO_ICON_FONT_PATH,18.0f,&iconConfig,phosphor::ranges);
  ImGui_ImplSDL2_InitForOpenGL(window, context);
  ImGui_ImplOpenGL2_Init();

  bool done = false;
  StudioApp app;
  while (!done) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      ImGui_ImplSDL2_ProcessEvent(&event);
      if (event.type == SDL_QUIT) done = true;
    }
    ImGui_ImplOpenGL2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
    app.draw();
    ImGui::Render();
    int width = 0, height = 0;
    SDL_GL_GetDrawableSize(window, &width, &height);
    glViewport(0, 0, width, height);
    glClearColor(0.055f, 0.055f, 0.065f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
    if (smoke && smokeFrames == 119) {
      std::vector<unsigned char> pixels(size_t(width)*height*3);
      glPixelStorei(GL_PACK_ALIGNMENT,1); glReadPixels(0,0,width,height,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());
      std::ofstream image("madstudio-smoke.ppm",std::ios::binary);
      image << "P6\n" << width << " " << height << "\n255\n";
      for (int y = height-1; y >= 0; --y) image.write(reinterpret_cast<const char*>(pixels.data()+size_t(y)*width*3),size_t(width)*3);
    }
    SDL_GL_SwapWindow(window);
    if (smoke) {
      smokeError = glGetError();
      if (smokeError != GL_NO_ERROR || ++smokeFrames >= 120) done = true;
    }
  }

  app.persist();
  app.releaseGraphics();
  ImGui::SaveIniSettingsToDisk(io.IniFilename);
  ImGui_ImplOpenGL2_Shutdown();
  ImGui_ImplSDL2_Shutdown();
  ImGui::DestroyContext();
  SDL_GL_DeleteContext(context);
  SDL_DestroyWindow(window);
  SDL_Quit();
  if (smoke) std::printf("MadStudio smoke: %u frames, GL error %u\n",smokeFrames,static_cast<unsigned>(smokeError));
  return smokeError == GL_NO_ERROR ? 0 : 2;
}
