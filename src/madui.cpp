#include <MadDisplayStudio/madui.hpp>
#include <MadDisplayStudio/image_import.hpp>
#include <MadDisplayStudio/unicode_text.hpp>
#include <cstring>
#include <cmath>
#include <MadDisplayStudio/json.hpp>

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <set>
#include <sstream>

namespace madstudio {

namespace {
using Json = json::Value;
using json::Value;
using json::boolField;
using json::field;
using json::numberField;
using json::parse;
using json::stringField;

bool isColor(const std::string& value, mad::Color& color) {
  if (value == "on") { color = mad::Color::On; return true; }
  if (value == "off") { color = mad::Color::Off; return true; }
  if (value.size() != 7 || value[0] != '#') return false;
  uint32_t rgb = 0;
  for (size_t i = 1; i < 7; ++i) {
    const char c = value[i];
    const int digit = c >= '0' && c <= '9' ? c-'0' : c >= 'a' && c <= 'f' ? c-'a'+10 : c >= 'A' && c <= 'F' ? c-'A'+10 : -1;
    if (digit < 0) return false;
    rgb = (rgb<<4)|digit;
  }
  color = mad::rgb(rgb>>16,rgb>>8,rgb); return true;
}
std::string colorName(mad::Color color) {
  if (color == mad::Color::On) return "on";
  if (color == mad::Color::Off) return "off";
  char value[8]; std::snprintf(value,sizeof(value),"#%02X%02X%02X",mad::red(color),mad::green(color),mad::blue(color)); return value;
}
bool readColor(const Json& object,const char* key,mad::Color& color,std::string& error) {
  const Json* value = field(object,key);
  if (!value) return true;
  if (value->type != Json::String || !isColor(value->string,color)) { error = std::string("invalid color: ")+key; return false; }
  return true;
}
const char* colorModeName(mad::ColorMode mode) {
  switch (mode) {
    case mad::ColorMode::Monochrome: return "mono";
    case mad::ColorMode::Palette: return "palette";
    case mad::ColorMode::Zoned: return "zoned";
    case mad::ColorMode::RGB: return "rgb";
  }
  return "rgb";
}

const char* sizeModeName(mad::SizeMode mode) { return mode == mad::SizeMode::Max ? "max" : "fit"; }

mad::SizeMode sizeModeFromName(const std::string& value, mad::SizeMode fallback) {
  return value == "max" ? mad::SizeMode::Max : value == "fit" ? mad::SizeMode::Fit : fallback;
}

const char* horizontalAlignName(mad::HorizontalAlign align) {
  return align == mad::HorizontalAlign::Center ? "center" : align == mad::HorizontalAlign::Right ? "right" : "left";
}

mad::HorizontalAlign horizontalAlignFromName(const std::string& value, mad::HorizontalAlign fallback) {
  if (value == "center") return mad::HorizontalAlign::Center;
  if (value == "right") return mad::HorizontalAlign::Right;
  return value == "left" ? mad::HorizontalAlign::Left : fallback;
}

const char* verticalAlignName(mad::VerticalAlign align) {
  return align == mad::VerticalAlign::Center ? "center" : align == mad::VerticalAlign::Bottom ? "bottom" : "top";
}

mad::VerticalAlign verticalAlignFromName(const std::string& value, mad::VerticalAlign fallback) {
  if (value == "center") return mad::VerticalAlign::Center;
  if (value == "bottom") return mad::VerticalAlign::Bottom;
  return value == "top" ? mad::VerticalAlign::Top : fallback;
}

const char* unitName(mad::Unit unit) {
  return unit == mad::Unit::Auto ? "auto" : unit == mad::Unit::Percent ? "percent" : unit == mad::Unit::Max ? "max" : "pixels";
}

mad::Unit unitFromName(const std::string& value, mad::Unit fallback) {
  if (value == "auto") return mad::Unit::Auto;
  if (value == "percent") return mad::Unit::Percent;
  if (value == "max") return mad::Unit::Max;
  return value == "pixels" ? mad::Unit::Pixels : fallback;
}

void indent(std::string& output, int level) { output.append(static_cast<size_t>(level * 2), ' '); }

void appendJsonString(std::string& output, const std::string& value) {
  output.push_back('"');
  for (size_t i = 0; i < value.size(); ++i) {
    const unsigned char character = static_cast<unsigned char>(value[i]);
    switch (character) {
      case '"': output += "\\\""; break;
      case '\\': output += "\\\\"; break;
      case '\b': output += "\\b"; break;
      case '\f': output += "\\f"; break;
      case '\n': output += "\\n"; break;
      case '\r': output += "\\r"; break;
      case '\t': output += "\\t"; break;
      default: output.push_back(static_cast<char>(character)); break;
    }
  }
  output.push_back('"');
}

void appendKey(std::string& output, const char* key, int level, bool& first) {
  if (!first) output += ",\n";
  first = false;
  indent(output, level);
  appendJsonString(output, key);
  output += ": ";
}

void appendLength(std::string& output, const char* key, const mad::Length& length, int level, bool& first) {
  appendKey(output, key, level, first);
  output += "{\n";
  bool innerFirst = true;
  appendKey(output, "value", level + 1, innerFirst);
  output += std::to_string(length.value);
  appendKey(output, "unit", level + 1, innerFirst);
  appendJsonString(output, unitName(length.unit));
  output += "\n";
  indent(output, level);
  output += "}";
}

void appendNode(std::string& output, const Node& node, int level) {
  output += "{\n";
  bool first = true;
  appendKey(output, "id", level + 1, first); appendJsonString(output, node.id);
  appendKey(output, "name", level + 1, first); appendJsonString(output, node.name);
  appendKey(output, "type", level + 1, first); appendJsonString(output, nodeTypeName(node.type));
  if (node.type == NodeType::Label) {
    appendKey(output, "text", level + 1, first); appendJsonString(output, node.text);
    appendKey(output,"fontMode",level+1,first); output += std::to_string(node.fontMode);
    appendKey(output,"fontSize",level+1,first); output += std::to_string(node.fontSize);
    appendKey(output,"fontFamily",level+1,first); appendJsonString(output,node.fontFamily);
    if (node.textAsset) { appendKey(output,"textLayout",level+1,first); output += textAssetJson(*node.textAsset); }
  }
  appendKey(output, "hasFrame", level + 1, first); output += node.hasFrame ? "true" : "false";
  appendKey(output, "frame", level + 1, first);
  output += "{\n";
  bool frameFirst = true;
  appendLength(output, "x", node.frame.x, level + 2, frameFirst);
  appendLength(output, "y", node.frame.y, level + 2, frameFirst);
  appendLength(output, "width", node.frame.width, level + 2, frameFirst);
  appendLength(output, "height", node.frame.height, level + 2, frameFirst);
  output += "\n"; indent(output, level + 1); output += "}";
  appendKey(output, "widthMode", level + 1, first); appendJsonString(output, sizeModeName(node.widthMode));
  appendKey(output, "heightMode", level + 1, first); appendJsonString(output, sizeModeName(node.heightMode));
  appendKey(output, "visible", level + 1, first); output += node.visible ? "true" : "false";
  appendKey(output, "enabled", level + 1, first); output += node.enabled ? "true" : "false";
  appendKey(output, "foreground", level + 1, first); appendJsonString(output, colorName(node.foreground));
  appendKey(output, "background", level + 1, first); appendJsonString(output, colorName(node.background));
  appendKey(output, "horizontalAlign", level + 1, first); appendJsonString(output, horizontalAlignName(node.horizontalAlign));
  appendKey(output, "verticalAlign", level + 1, first); appendJsonString(output, verticalAlignName(node.verticalAlign));
  appendKey(output, "textScale", level + 1, first); output += std::to_string(node.textScale);
  appendKey(output, "padding", level + 1, first); output += std::to_string(node.padding);
  appendKey(output, "filled", level + 1, first); output += node.filled ? "true" : "false";
  appendKey(output, "checked", level + 1, first); output += node.checked ? "true" : "false";
  appendKey(output, "on", level + 1, first); output += node.on ? "true" : "false";
  appendKey(output, "selected", level + 1, first); output += node.selected ? "true" : "false";
  appendKey(output, "gap", level + 1, first); output += std::to_string(node.gap);
  appendKey(output, "scrollable", level + 1, first); output += node.scrollable ? "true" : "false";
  appendKey(output, "animated", level + 1, first); output += node.animated ? "true" : "false";
  appendKey(output, "entrance", level + 1, first); appendJsonString(output, entranceName(node.entrance));
  appendKey(output, "easing", level + 1, first); appendJsonString(output, easingName(node.easing));
  appendKey(output, "durationMs", level + 1, first); output += std::to_string(node.durationMs);
  appendKey(output, "delayMs", level + 1, first); output += std::to_string(node.delayMs);
  appendKey(output, "switchDurationMs", level + 1, first); output += std::to_string(node.switchDurationMs);
  if (node.type == NodeType::GridLayout) {
    appendKey(output,"columns",level+1,first); output += std::to_string(node.columns);
    appendKey(output,"columnGap",level+1,first); output += std::to_string(node.columnGap);
  }
  if (node.type >= NodeType::Canvas && node.type <= NodeType::Line) {
    appendKey(output, "graphicsWidth", level + 1, first); output += std::to_string(node.graphicsWidth);
    appendKey(output, "graphicsHeight", level + 1, first); output += std::to_string(node.graphicsHeight);
  }
  if (node.type == NodeType::Image && node.imageColors.empty()) {
    appendKey(output, "pixels", level + 1, first); output += "[";
    for (size_t i = 0; i < node.imagePixels.size(); ++i) {
      if (i) output += ",";
      output += std::to_string(node.imagePixels[i]);
    }
    output += "]";
  }
  if (node.type == NodeType::Image && !node.imageColors.empty()) {
    appendKey(output,"colors",level+1,first); output += "[";
    for (size_t i = 0; i < node.imageColors.size(); ++i) { if (i) output += ','; appendJsonString(output,colorName(node.imageColors[i])); }
    output += "]";
    appendKey(output,"coverage",level+1,first); output += "[";
    for (size_t i = 0; i < node.imageColors.size(); ++i) { if (i) output += ','; output += std::to_string(node.imageCoverage.empty() ? 1 : node.imageCoverage[i]); }
    output += "]";
  }
  if (node.type == NodeType::Image && node.imageSource) {
    appendKey(output,"source",level+1,first); output += "{\"name\":";
    appendJsonString(output,node.imageSource->name); output += ",\"data\":";
    appendJsonString(output,imageSourceHex(*node.imageSource)); output += "}";
    appendKey(output,"filters",level+1,first);
    const auto& f = node.imageFilters;
    output += "{\"brightness\":"+std::to_string(f.brightness)+",\"contrast\":"+std::to_string(f.contrast)+",\"gamma\":"+std::to_string(f.gamma)+",\"saturation\":"+std::to_string(f.saturation)+",\"threshold\":"+std::to_string(f.threshold)+",\"ditherStrength\":"+std::to_string(f.ditherStrength)+",\"alphaThreshold\":"+std::to_string(f.alphaThreshold)+",\"fit\":"+std::to_string(f.fit)+",\"dither\":"+std::to_string(f.dither)+",\"invert\":"+(f.invert ? "true" : "false")+"}";
  }
  if (node.type == NodeType::Canvas) {
    appendKey(output, "commands", level + 1, first); output += "[";
    for (size_t i = 0; i < node.commands.size(); ++i) {
      if (i) output += ",";
      const auto& command = node.commands[i];
      output += "{\"kind\":" + std::to_string(static_cast<int>(command.kind))
          + ",\"x\":" + std::to_string(command.rect.x) + ",\"y\":" + std::to_string(command.rect.y)
          + ",\"width\":" + std::to_string(command.rect.width) + ",\"height\":" + std::to_string(command.rect.height)
          + ",\"color\":";
      appendJsonString(output,colorName(command.color));
      output += ",\"filled\":" + std::string(command.filled ? "true" : "false") + "}";
    }
    output += "]";
  }
  appendKey(output, "children", level + 1, first);
  output += "[";
  const bool legacyText = node.type == NodeType::Button && !node.text.empty() &&
      (node.children.empty() || (node.children.size() == 1 && node.children[0]->type == NodeType::Label));
  if (legacyText) {
    Node label(NodeType::Label,"Content"); label.text = node.text;
    label.id = node.children.empty() ? node.id+"-content" : node.children[0]->id;
    label.foreground = node.filled ? node.background : node.foreground;
    output += "\n"; appendNode(output,label,level+2);
  } else for (size_t i = 0; i < node.children.size(); ++i) {
    output += i ? "," : "\n";
    appendNode(output, *node.children[i], level + 2);
  }
  if (legacyText || !node.children.empty()) { output += "\n"; indent(output, level + 1); }
  output += "]\n";
  indent(output, level);
  output += "}";
}

mad::Length readLength(const Json& frame, const char* key, mad::Length fallback) {
  const Json* value = field(frame, key);
  if (!value || value->type != Json::Object) return fallback;
  return mad::Length(numberField(*value, "value", fallback.value),
                     unitFromName(stringField(*value, "unit", unitName(fallback.unit)), fallback.unit));
}

bool canHaveChildren(NodeType type) {
  return isContentControl(type) || isLayout(type);
}

bool readEasing(const Json& value, const char* key, mad::Easing& easing, std::string& error) {
  const Json* fieldValue = field(value, key);
  if (!fieldValue) return true;
  for (auto candidate : {mad::Easing::Linear, mad::Easing::CubicOut, mad::Easing::CubicInOut,
                          mad::Easing::BackOut, mad::Easing::BounceOut})
    if (fieldValue->type == Json::String && fieldValue->string == easingName(candidate)) { easing = candidate; return true; }
  error = std::string("unknown easing: ") + key;
  return false;
}

bool readMilliseconds(const Json& value, const char* key, uint16_t& milliseconds, std::string& error) {
  const Json* number = field(value, key);
  if (!number) return true;
  if (number->type != Json::Number || number->number < 0 || number->number > 60000 ||
      number->number != static_cast<int>(number->number)) {
    error = std::string(key) + " must be an integer between 0 and 60000"; return false;
  }
  milliseconds = static_cast<uint16_t>(number->number);
  return true;
}

std::unique_ptr<Node> readNode(const Json& value, std::string& error,int version) {
  if (value.type != Json::Object) { error = "node must be an object"; return std::unique_ptr<Node>(); }
  NodeType type;
  if (!nodeTypeFromName(stringField(value, "type", ""), type)) {
    error = "unknown node type";
    return std::unique_ptr<Node>();
  }
  std::unique_ptr<Node> node(new Node(type, stringField(value, "name", nodeTypeName(type))));
  if (stringField(value,"type","") == "Matrix" && node->name == "Matrix") node->name = "Image";
  node->id = stringField(value, "id", "");
  node->text = stringField(value, "text", version < 5 && type == NodeType::Button ? "Text" : node->text);
  node->hasFrame = boolField(value, "hasFrame", false);
  const Json* frame = field(value, "frame");
  if (frame && frame->type == Json::Object) {
    node->frame.x = readLength(*frame, "x", node->frame.x);
    node->frame.y = readLength(*frame, "y", node->frame.y);
    node->frame.width = readLength(*frame, "width", node->frame.width);
    node->frame.height = readLength(*frame, "height", node->frame.height);
  }
  node->widthMode = sizeModeFromName(stringField(value, "widthMode", "fit"), node->widthMode);
  node->heightMode = sizeModeFromName(stringField(value, "heightMode", "fit"), node->heightMode);
  node->visible = boolField(value, "visible", node->visible);
  node->enabled = boolField(value, "enabled", node->enabled);
  if (!readColor(value,"foreground",node->foreground,error) || !readColor(value,"background",node->background,error)) return {};
  node->horizontalAlign = horizontalAlignFromName(stringField(value, "horizontalAlign", "left"), node->horizontalAlign);
  node->verticalAlign = verticalAlignFromName(stringField(value, "verticalAlign", "center"), node->verticalAlign);
  node->textScale = static_cast<uint8_t>(std::max(1, std::min(3, numberField(value, "textScale", node->textScale))));
  node->padding = static_cast<uint8_t>(std::max(0, std::min(8, numberField(value, "padding", node->padding))));
  node->filled = boolField(value, "filled", node->filled);
  node->checked = boolField(value, "checked", node->checked);
  node->on = boolField(value, "on", node->on);
  node->selected = boolField(value, "selected", node->selected);
  node->gap = static_cast<uint8_t>(std::max(0, std::min(16, numberField(value, "gap", node->gap))));

  node->scrollable = boolField(value, "scrollable", false);
  node->animated = boolField(value, "animated", false);
  const Json* entrance = field(value, "entrance");
  if (entrance) {
    bool found = false;
    for (auto effect : {mad::Entrance::Rise, mad::Entrance::Slide, mad::Entrance::Reveal})
      if (entrance->type == Json::String && entrance->string == entranceName(effect)) { node->entrance = effect; found = true; }
    if (!found) { error = "unknown entrance effect"; return {}; }
  }
  if (!readEasing(value, "easing", node->easing, error) ||
      !readMilliseconds(value, "durationMs", node->durationMs, error) ||
      !readMilliseconds(value, "delayMs", node->delayMs, error) ||
      !readMilliseconds(value, "switchDurationMs", node->switchDurationMs, error)) return {};

  const auto readInteger = [&](const Json& object,const char* key,int fallback,int low,int high,int& result) {
    const Json* entry = field(object,key);
    if (!entry) { result = fallback; return true; }
    if (entry->type != Json::Number || entry->number < low || entry->number > high || entry->number != static_cast<int>(entry->number)) return false;
    result = static_cast<int>(entry->number); return true;
  };
  if (node->type == NodeType::Label) {
    if (!readInteger(value,"fontMode",0,0,2,node->fontMode) || !readInteger(value,"fontSize",12,6,96,node->fontSize)) { error = "Invalid text font settings"; return {}; }
    node->fontFamily = stringField(value,"fontFamily","System"); if (node->fontFamily.size() > 256) { error = "Font name too long"; return {}; }
    if (const auto* layout = field(value,"textLayout")) if (!readTextAsset(*layout,*node,error)) return {};
  }
  if (node->type == NodeType::GridLayout) {
    int columns,columnGap;
    if (!readInteger(value,"columns",2,1,mad::Layout::kMaxComponents,columns) ||
        !readInteger(value,"columnGap",mad::kDefaultGap,0,255,columnGap)) { error = "invalid grid settings"; return {}; }
    node->columns = static_cast<uint8_t>(columns); node->columnGap = static_cast<uint8_t>(columnGap);
  }
  const int limit = node->type == NodeType::Image ? 512 : 1024;
  if (!readInteger(value,"graphicsWidth",node->graphicsWidth,1,limit,node->graphicsWidth) ||
      !readInteger(value,"graphicsHeight",node->graphicsHeight,1,limit,node->graphicsHeight)) {
    error = "invalid graphics size"; return {};
  }
  if (node->type == NodeType::Image) {
    const Json* pixels = field(value,"pixels");
    if (!pixels) pixels = field(value,"matrix"); // legacy 8x8 artwork
    const size_t count = static_cast<size_t>(node->graphicsWidth)*node->graphicsHeight;
    node->imagePixels.assign(count,mad::N);
    if (pixels) {
      if (pixels->type != Json::Array || pixels->array.size() != count) { error = "image pixel count does not match size"; return {}; }
      for (size_t i = 0; i < count; ++i) {
        const Json& pixel = pixels->array[i];
        if (pixel.type != Json::Number || pixel.number < mad::N || pixel.number > mad::W || pixel.number != static_cast<int>(pixel.number)) {
          error = "invalid image pixel"; return {};
        }
        node->imagePixels[i] = static_cast<mad::MatrixPixel>(pixel.number);
      }
    }
  }
  if (const Json* colors = field(value,"colors")) {
    const size_t count = size_t(node->graphicsWidth)*node->graphicsHeight;
    if (node->type != NodeType::Image || colors->type != Json::Array || colors->array.size() != count) { error = "invalid image colors"; return {}; }
    node->imageColors.resize(count); node->imageCoverage.assign(count,1);
    for (size_t i = 0; i < count; ++i)
      if (colors->array[i].type != Json::String || !isColor(colors->array[i].string,node->imageColors[i])) { error = "invalid image color"; return {}; }
    if (const Json* coverage = field(value,"coverage")) {
      if (coverage->type != Json::Array || coverage->array.size() != count) { error = "invalid image coverage"; return {}; }
      for (size_t i = 0; i < count; ++i) {
        const Json& v = coverage->array[i];
        if (v.type != Json::Number || v.number < 0 || v.number > 2 || v.number != int(v.number)) { error = "invalid image coverage"; return {}; }
        node->imageCoverage[i] = static_cast<uint8_t>(v.number);
      }
    }
    node->imagePixels.clear();
  }
  if (const Json* source = field(value,"source")) {
    if (node->type != NodeType::Image || source->type != Json::Object) { error = "invalid image source"; return {}; }
    node->imageSource = imageSourceFromHex(stringField(*source,"name",""),stringField(*source,"data",""),error);
    if (!node->imageSource) return {};
    if (const Json* filters = field(value,"filters")) {
      auto& f = node->imageFilters;
      auto read = [&](const char* key,float& value,double low,double high) {
        const auto* item = field(*filters,key); if (!item) return true;
        if (item->type != Json::Number || !std::isfinite(item->number) || item->number < low || item->number > high) return false;
        value = static_cast<float>(item->number); return true;
      };
      if (filters->type != Json::Object || !read("brightness",f.brightness,-1,1) || !read("contrast",f.contrast,0,3) ||
          !read("gamma",f.gamma,.1,4) || !read("saturation",f.saturation,0,2) || !read("threshold",f.threshold,0,1) ||
          !read("ditherStrength",f.ditherStrength,0,1) || !read("alphaThreshold",f.alphaThreshold,0,1) ||
          !readInteger(*filters,"fit",0,0,2,f.fit) || !readInteger(*filters,"dither",1,0,2,f.dither)) {
        error = "invalid image filters"; return {};
      }
      const auto* invert = field(*filters,"invert");
      if (invert && invert->type != Json::Boolean) { error = "invalid image invert"; return {}; }
      f.invert = invert && invert->boolean;
    }
  }
  if (const Json* commands = field(value,"commands")) {
    if (node->type != NodeType::Canvas || commands->type != Json::Array || commands->array.size() > 256) { error = "invalid canvas commands"; return {}; }
    node->commands.clear();
    for (const auto& entry : commands->array) {
      int kind, x, y, width, height;
      if (entry.type != Json::Object || !readInteger(entry,"kind",1,0,3,kind) ||
          !readInteger(entry,"x",0,-1024,1024,x) || !readInteger(entry,"y",0,-1024,1024,y) ||
          !readInteger(entry,"width",1,1,1024,width) || !readInteger(entry,"height",1,1,1024,height)) {
        error = "invalid canvas command"; return {};
      }
      mad::DrawCommand command; command.kind = static_cast<mad::DrawingKind>(kind);
      command.rect = {static_cast<int16_t>(x),static_cast<int16_t>(y),static_cast<int16_t>(width),static_cast<int16_t>(height)};
      command.color = boolField(entry,"white",true) ? mad::Color::On : mad::Color::Off;
      if (!readColor(entry,"color",command.color,error)) return {};
      command.filled = boolField(entry,"filled",false); node->commands.push_back(command);
    }
  }

  const Json* children = field(value, "children");
  if (children && children->type == Json::Array) {
    if (!canHaveChildren(node->type) && !children->array.empty()) { error = "leaf nodes cannot have children"; return std::unique_ptr<Node>(); }
    if (children->array.size() > 16) { error = "a MadDisplay container can have at most 16 children"; return std::unique_ptr<Node>(); }
    for (size_t i = 0; i < children->array.size(); ++i) {
      std::unique_ptr<Node> child = readNode(children->array[i], error,version);
      if (!child) return std::unique_ptr<Node>();
      node->children.push_back(std::move(child));
    }
  }
  if (node->type == NodeType::Button && node->children.empty() && version < 5) {
    std::unique_ptr<Node> label(new Node(NodeType::Label,"Content")); label->text = node->text;
    if (!node->id.empty()) label->id = node->id+"-content";
    label->foreground = node->filled ? node->background : node->foreground;
    node->children.push_back(std::move(label)); node->text.clear();
  }
  return node;
}

uint32_t nodeIdNumber(const std::string& id) {
  if (id.compare(0, 5, "node-") != 0 || id.size() == 5) return 0;
  uint32_t number = 0;
  for (size_t i = 5; i < id.size(); ++i) {
    if (id[i] < '0' || id[i] > '9') return 0;
    const uint32_t digit = static_cast<uint32_t>(id[i] - '0');
    if (number > (4294967295U - digit) / 10U) return 4294967295U;
    number = number * 10U + digit;
  }
  return number;
}

void updateNextId(const Node& node, uint32_t& nextId) {
  const uint32_t number = nodeIdNumber(node.id);
  if (number >= nextId && number < 4294967295U) nextId = number + 1;
  for (size_t i = 0; i < node.children.size(); ++i) updateNextId(*node.children[i], nextId);
}

void normalizeIds(Node& node, std::set<std::string>& used, uint32_t& nextId) {
  if (node.id.empty() || used.find(node.id) != used.end()) {
    do { node.id = "node-" + std::to_string(nextId++); } while (used.find(node.id) != used.end());
  }
  used.insert(node.id);
  for (size_t i = 0; i < node.children.size(); ++i) normalizeIds(*node.children[i], used, nextId);
}

}  // namespace

const char* entranceName(mad::Entrance effect) {
  switch (effect) {
    case mad::Entrance::Rise: return "rise";
    case mad::Entrance::Slide: return "slide";
    case mad::Entrance::Reveal: return "reveal";
  }
  return "rise";
}
const char* easingName(mad::Easing easing) {
  switch (easing) {
    case mad::Easing::Linear: return "linear";
    case mad::Easing::CubicOut: return "cubicOut";
    case mad::Easing::CubicInOut: return "cubicInOut";
    case mad::Easing::BackOut: return "backOut";
    case mad::Easing::BounceOut: return "bounceOut";
  }
  return "cubicOut";
}

bool isContentControl(NodeType type) {
  return type == NodeType::Button || type == NodeType::Checkbox || type == NodeType::Switch || type == NodeType::RadioButton;
}
void normalizeContent(Document& document) {
  const auto visit = [&](const auto& self,Node& node) -> void {
    if (node.type == NodeType::Button && !node.text.empty()) {
      if (node.children.empty()) {
        auto label = document.createNode(NodeType::Label); label->name = "Content"; node.children.push_back(std::move(label));
      }
      if (node.children.size() == 1 && node.children[0]->type == NodeType::Label) {
        node.children[0]->text = node.text; node.children[0]->foreground = node.filled ? node.background : node.foreground;
      }
      node.text.clear();
    }
    for (auto& child : node.children) self(self,*child);
  };
  for (auto& page : document.pages) visit(visit,*page->root);
}

const char* nodeTypeName(NodeType type) {
  switch (type) {
    case NodeType::GridLayout: return "GridLayout";
    case NodeType::VerticalLayout: return "VerticalLayout";
    case NodeType::HorizontalLayout: return "HorizontalLayout";
    case NodeType::Container: return "Container";
    case NodeType::Panel: return "Panel";
    case NodeType::FreeLayout: return "FreeLayout";
    case NodeType::Label: return "Label";
    case NodeType::Button: return "Button";
    case NodeType::Checkbox: return "Checkbox";
    case NodeType::Switch: return "Switch";
    case NodeType::RadioButton: return "RadioButton";
    case NodeType::Canvas: return "Canvas";
    case NodeType::Image: return "Image";
    case NodeType::Rectangle: return "Rectangle";
    case NodeType::Ellipse: return "Ellipse";
    case NodeType::Line: return "Line";
  }
  return "Container";
}

bool nodeTypeFromName(const std::string& name, NodeType& type) {
  if (name == "Matrix") { type = NodeType::Image; return true; }
  for (auto candidate : kNodeTypes)
    if (name == nodeTypeName(candidate)) { type = candidate; return true; }
  return false;
}

bool isLayout(NodeType type) {
  for (auto layout : kLayoutTypes) if (type == layout) return true;
  return false;
}

Node::Node(NodeType type, const std::string& name)
    : name(name.empty() ? nodeTypeName(type) : name), type(type), text(type == NodeType::Label ? "Text" : "") {
  if (type == NodeType::Image) {
    graphicsWidth = graphicsHeight = 8; imagePixels.assign(64,mad::N);
  }
  if (type == NodeType::Canvas) commands.push_back(mad::DrawCommand{});
}
uint64_t Node::imageHash() const {
  if (imageHashValid_) return imageHash_;
  uint64_t hash = 1469598103934665603ULL;
  const auto mix = [&](uint64_t value) { hash = (hash^value)*1099511628211ULL; };
  if (imageSource) {
    mix(imageSource->fingerprint);
    for (float value : {imageFilters.brightness,imageFilters.contrast,imageFilters.gamma,imageFilters.saturation,imageFilters.threshold,imageFilters.ditherStrength,imageFilters.alphaThreshold}) {
      uint32_t bits; std::memcpy(&bits,&value,sizeof(bits)); mix(bits);
    }
    mix(imageFilters.fit); mix(imageFilters.dither); mix(imageFilters.invert);
  }
  mix(imagePixels.size()); for (auto value : imagePixels) mix(static_cast<uint8_t>(value));
  mix(imageColors.size()); for (auto value : imageColors) mix(static_cast<uint32_t>(value));
  mix(imageCoverage.size()); for (auto value : imageCoverage) mix(value);
  imageHash_ = hash; imageHashValid_ = true; return hash;
}
Node::~Node() = default;

Page::Page() : name("Main"), root(new Node(NodeType::VerticalLayout, "Root")) {
  root->hasFrame = true;
  root->frame.width = root->frame.height = mad::Length::percent(100);
}
Page::~Page() = default;

Document::Document() = default;
Document::~Document() = default;

void Document::rebuildIdCounter() {
  nextId_ = 1;
  for (size_t i = 0; i < pages.size(); ++i) updateNextId(*pages[i]->root, nextId_);
}

Document Document::createDefault() {
  Document document;
  document.pages.push_back(std::unique_ptr<Page>(new Page()));
  Node& root = *document.pages[0]->root;
  std::unique_ptr<Node> title = document.createNode(NodeType::Label);
  title->name = "Title";
  title->text = "MadDisplay UI";
  root.children.push_back(std::move(title));
  std::unique_ptr<Node> button = document.createNode(NodeType::Button);
  button->name = "StartButton";
  button->children[0]->text = "START";
  root.children.push_back(std::move(button));
  return document;
}

Document Document::createAnimated() {
  Document document;
  document.name = "Motion playground";
  const char* names[] = {"Welcome", "Controls", "Ready"};
  const char* titles[] = {"MAD DISPLAY", "MAKE IT MOVE", "SMOOTH MOVES"};
  for (int i = 0; i < 3; ++i) {
    std::unique_ptr<Page> page(new Page());
    page->name = names[i];
    page->root->widthMode = mad::SizeMode::Max;
    page->root->gap = 5;
    auto title = document.createNode(NodeType::Label);
    title->text = titles[i]; title->name = "Title";
    title->animated = true; title->entrance = mad::Entrance::Reveal; title->durationMs = 900;
    page->root->children.push_back(std::move(title));
    auto button = document.createNode(NodeType::Button);
    button->children[0]->text = i == 0 ? "LET'S GO" : (i == 1 ? "TRY THE SWITCH" : "PLAY AGAIN");
    button->name = "Action"; button->widthMode = mad::SizeMode::Max;
    button->animated = true; button->entrance = mad::Entrance::Slide;
    button->durationMs = 850; button->delayMs = 140; button->easing = mad::Easing::BackOut;
    page->root->children.push_back(std::move(button));
    auto row = document.createNode(NodeType::HorizontalLayout);
    row->name = "Controls"; row->gap = 8;
    row->animated = true; row->entrance = mad::Entrance::Rise; row->durationMs = 700; row->delayMs = 280;
    row->children.push_back(document.createNode(NodeType::Switch));
    row->children.push_back(document.createNode(NodeType::Checkbox));
    page->root->children.push_back(std::move(row));
    document.pages.push_back(std::move(page));
  }
  return document;
}

Document Document::createFocusDemo() {
  Document document;
  document.name = "Liquid focus";
  document.pages.emplace_back(new Page());
  auto& root = *document.pages[0]->root;
  root.widthMode = root.heightMode = mad::SizeMode::Max;
  auto board = document.createNode(NodeType::FreeLayout);
  board->name = "Navigation grid"; board->hasFrame = true;
  board->frame = mad::Frame::px(0,0,288,108);
  for (int row = 0; row < 4; ++row) for (int col = 0; col < 4; ++col) {
    auto button = document.createNode(NodeType::Button);
    button->name = "Cell " + std::to_string(row + 1) + " " + std::to_string(col + 1);
    button->children[0]->text = std::string(1,static_cast<char>('A' + row)) + std::to_string(col + 1);
    button->hasFrame = true; button->frame = mad::Frame::px(col * 76,row * 30,60,18);
    board->children.push_back(std::move(button));
  }
  root.children.push_back(std::move(board));
  return document;
}

std::unique_ptr<Node> Document::createNode(NodeType type) {
  std::unique_ptr<Node> node(new Node(type));
  node->id = "node-" + std::to_string(nextId_++);
  if (type == NodeType::Button) {
    auto label = createNode(NodeType::Label); label->name = "Content"; node->children.push_back(std::move(label));
  }
  return node;
}

bool saveMadUi(const Document& document, const std::string& path, std::string* error) {
  // Preserve reference-only projects, including legacy device programs.
  { std::ifstream old(path); std::ostringstream bytes; bytes << old.rdbuf();
    Json previous; std::string parseError;
    if (parse(bytes.str(), previous, parseError) && field(previous, "uiFile")) {
      const std::string backupPath = path + ".legacy.bak";
      std::ifstream existing(backupPath); std::ostringstream backup; backup << existing.rdbuf();
      if (existing && backup.str() != bytes.str()) { if (error) *error = "legacy backup differs"; return false; }
      if (!existing) { std::ofstream out(backupPath); out << bytes.str(); out.close();
        if (!out) { if (error) *error = "cannot preserve legacy project"; return false; } }
    }
  }
  const std::string temporary = path + ".tmp";
  std::ofstream file(temporary.c_str(), std::ios::out | std::ios::trunc);
  if (!file) { if (error) *error = "cannot open file for writing"; return false; }
  std::string output;
  output.reserve(8192);
  output += "{\n  \"format\": \"mad\",\n  \"version\": 6,\n  \"name\": ";
  appendJsonString(output, document.name);
  output += ",\n  \"displayProfile\": "; appendJsonString(output, document.displayProfile);
  output += ",\n  \"colorMode\": "; appendJsonString(output,colorModeName(document.colorMode));
  output += ",\n  \"ink\": "; appendJsonString(output,colorName(document.ink));
  output += ",\n  \"palette\": [";
  for (uint8_t i = 0; i < document.palette.count; ++i) { if (i) output += ','; appendJsonString(output,colorName(document.palette.colors[i])); }
  output += "],\n  \"colorRegions\": [";
  for (size_t i = 0; i < document.colorRegions.size(); ++i) {
    if (i) output += ',';
    const auto& region = document.colorRegions[i];
    output += "{\"x\":"+std::to_string(region.bounds.x)+",\"y\":"+std::to_string(region.bounds.y)
        +",\"width\":"+std::to_string(region.bounds.width)+",\"height\":"+std::to_string(region.bounds.height)+",\"color\":";
    appendJsonString(output,colorName(region.color)); output += '}';
  }
  output += "]";
  output += ",\n  \"screen\": {\n    \"width\": ";
  output += std::to_string(document.screenWidth);
  output += ",\n    \"height\": ";
  output += std::to_string(document.screenHeight);
  output += "\n  },\n  \"transitionMs\": " + std::to_string(document.transitionMs);
  output += ",\n  \"focusMs\": " + std::to_string(document.focusMs);
  output += ",\n  \"scrollMs\": " + std::to_string(document.scrollMs);
  output += ",\n  \"transitionEasing\": "; appendJsonString(output, easingName(document.transitionEasing));
  output += ",\n  \"fps\": " + std::to_string(document.fps);
  output += ",\n  \"safeArea\": {\"left\": " + std::to_string(document.safeArea.left)
      + ", \"top\": " + std::to_string(document.safeArea.top)
      + ", \"right\": " + std::to_string(document.safeArea.right)
      + ", \"bottom\": " + std::to_string(document.safeArea.bottom) + "},\n  \"pages\": [\n";
  for (size_t i = 0; i < document.pages.size(); ++i) {
    if (i) output += ",\n";
    indent(output, 2); output += "{\n";
    bool first = true;
    appendKey(output, "name", 3, first); appendJsonString(output, document.pages[i]->name);
    appendKey(output, "background", 3, first); appendJsonString(output, colorName(document.pages[i]->background));
    appendKey(output, "root", 3, first); output += "\n"; indent(output, 3);
    appendNode(output, *document.pages[i]->root, 3);
    output += "\n"; indent(output, 2); output += "}";
  }
  output += "\n  ],\n  \"currentPage\": ";
  output += std::to_string(document.currentPage < document.pages.size() ? document.currentPage : 0);
  output += "\n}\n";
  file << output;
  file.close();
  if (!file) { std::remove(temporary.c_str()); if (error) *error = "cannot write file"; return false; }
  if (std::rename(temporary.c_str(), path.c_str()) != 0) { std::remove(temporary.c_str()); if (error) *error = "cannot replace design file"; return false; }
  return true;
}

static bool loadDesign(const std::string& path, Document& document, std::string* error, unsigned depth) {
  if (depth > 8) { if (error) *error = "cyclic legacy project reference"; return false; }
  std::ifstream file(path.c_str(), std::ios::in | std::ios::binary);
  if (!file) { if (error) *error = "cannot open file"; return false; }
  std::ostringstream contents;
  contents << file.rdbuf();
  Json root;
  std::string parseError;
  if (!parse(contents.str(), root, parseError)) { if (error) *error = parseError; return false; }
  const std::string format = stringField(root, "format", "");
  if (root.type != Json::Object || (format != "madui" && format != "mad")) {
    if (error) *error = "not a MadDisplay design"; return false;
  }
  if (format == "mad" && field(root, "uiFile")) {
    const int version = numberField(root, "version", 1);
    const std::string asset = stringField(root, "uiFile", "");
    if ((version != 1 && version != 2) || asset.empty()) { if (error) *error = "invalid legacy project"; return false; }
    const size_t slash = path.find_last_of("/\\");
    return loadDesign(asset[0] == '/' || slash == std::string::npos ? asset : path.substr(0, slash+1) + asset,
                      document, error, depth+1);
  }
  const Json* version = field(root, "version");
  if (version && (version->type != Json::Number || (version->number != 1 && version->number != 2 && !(format == "mad" && (version->number == 3 || version->number == 4 || version->number == 5 || version->number == 6))))) {
    if (error) *error = "unsupported design version"; return false;
  }
  const Json* screen = field(root, "screen");
  const Json* pages = field(root, "pages");
  if (!screen || screen->type != Json::Object || !pages || pages->type != Json::Array || pages->array.empty()) {
    if (error) *error = "design needs screen and at least one page";
    return false;
  }
  Document parsed;
  parsed.name = stringField(root, "name", parsed.name);
  parsed.screenWidth = numberField(*screen, "width", 128);
  parsed.screenHeight = numberField(*screen, "height", 64);
  if (parsed.screenWidth <= 0 || parsed.screenHeight <= 0 || parsed.screenWidth > 1024 || parsed.screenHeight > 1024) {
    if (error) *error = "screen dimensions must be between 1 and 1024";
    return false;
  }
  parsed.displayProfile = stringField(root, "displayProfile", "");
  if (parsed.displayProfile.empty()) {
    for (const auto& profile : kDisplayProfiles)
      if (profile.width == parsed.screenWidth && profile.height == parsed.screenHeight) parsed.displayProfile = profile.id;
  }
  for (const auto& profile : kDisplayProfiles)
    if (parsed.displayProfile == profile.id) { parsed.colorMode = profile.colorMode; break; }
  if (const Json* mode = field(root,"colorMode")) {
    bool found = false;
    for (auto candidate : {mad::ColorMode::Monochrome,mad::ColorMode::Palette,mad::ColorMode::Zoned,mad::ColorMode::RGB})
      if (mode->type == Json::String && mode->string == colorModeName(candidate)) { parsed.colorMode = candidate; found = true; }
    if (!found) { if (error) *error = "unknown display color mode"; return false; }
  }
  std::string colorError;
  if (!readColor(root,"ink",parsed.ink,colorError)) { if (error) *error = colorError; return false; }
  if (const Json* palette = field(root,"palette")) {
    if (palette->type != Json::Array || palette->array.size() < 2 || palette->array.size() > 16) { if (error) *error = "palette needs 2 to 16 colors"; return false; }
    parsed.palette.count = static_cast<uint8_t>(palette->array.size());
    for (size_t i = 0; i < palette->array.size(); ++i)
      if (palette->array[i].type != Json::String || !isColor(palette->array[i].string,parsed.palette.colors[i])) { if (error) *error = "invalid palette color"; return false; }
  }
  if (const Json* regions = field(root,"colorRegions")) {
    if (regions->type != Json::Array || regions->array.size() > 255) { if (error) *error = "invalid color regions"; return false; }
    for (const auto& entry : regions->array) {
      mad::ColorRegion region{{0,0,0,0},mad::Color::White};
      const char* keys[] = {"x","y","width","height"}; int16_t* values[] = {&region.bounds.x,&region.bounds.y,&region.bounds.width,&region.bounds.height};
      for (int i = 0; i < 4; ++i) {
        const Json* v = field(entry,keys[i]);
        if (!v || v->type != Json::Number || v->number < (i < 2 ? 0 : 1) || v->number > 1024 || v->number != int(v->number)) { if (error) *error = "invalid color region bounds"; return false; }
        *values[i] = static_cast<int16_t>(v->number);
      }
      if (!readColor(entry,"color",region.color,colorError)) { if (error) *error = colorError; return false; }
      parsed.colorRegions.push_back(region);
    }
  }
  std::string settingError;
  if (!readMilliseconds(root, "transitionMs", parsed.transitionMs, settingError) ||
      !readMilliseconds(root,"focusMs",parsed.focusMs,settingError) ||
      !readMilliseconds(root,"scrollMs",parsed.scrollMs,settingError) ||
      !readEasing(root, "transitionEasing", parsed.transitionEasing, settingError)) {
    if (error) *error = settingError; return false;
  }
  parsed.fps = static_cast<int16_t>(std::max(1, std::min(240, numberField(root, "fps", 60))));
  if (const Json* insets = field(root, "safeArea")) {
    parsed.safeArea = mad::Insets(
        std::max(0, std::min(1024, numberField(*insets, "left", 1))),
        std::max(0, std::min(1024, numberField(*insets, "top", 1))),
        std::max(0, std::min(1024, numberField(*insets, "right", 1))),
        std::max(0, std::min(1024, numberField(*insets, "bottom", 1))));
  }
  if (pages->array.size() > 16) { if (error) *error = "a MadDisplay compositor can have at most 16 pages"; return false; }
  for (size_t i = 0; i < pages->array.size(); ++i) {
    const Json& value = pages->array[i];
    const Json* pageRoot = field(value, "root");
    if (value.type != Json::Object || !pageRoot) { if (error) *error = "invalid page"; return false; }
    std::string nodeError;
    std::unique_ptr<Node> node = readNode(*pageRoot, nodeError,numberField(root,"version",1));
    if (!node) { if (error) *error = nodeError; return false; }
    if (!isLayout(node->type)) {
      if (error) *error = "page root must be a layout";
      return false;
    }
    std::unique_ptr<Page> page(new Page());
    page->name = stringField(value, "name", "Page");
    if (!readColor(value,"background",page->background,nodeError)) { if (error) *error = nodeError; return false; }
    page->root = std::move(node);
    parsed.pages.push_back(std::move(page));
  }
  std::set<std::string> usedIds;
  uint32_t generatedId = 1;
  for (const auto& page : parsed.pages) updateNextId(*page->root,generatedId);
  for (size_t i = 0; i < parsed.pages.size(); ++i) normalizeIds(*parsed.pages[i]->root, usedIds, generatedId);
  parsed.rebuildIdCounter();
  parsed.currentPage = static_cast<size_t>(numberField(root, "currentPage", 0));
  if (parsed.currentPage >= parsed.pages.size()) parsed.currentPage = 0;
  normalizeContent(parsed);
  refreshImportedImages(parsed);
  refreshTextAssets(parsed);
  document = std::move(parsed);
  return true;
}

void applyDisplayProfile(Document& document,const DisplayProfile& profile) {
  document.displayProfile = profile.id; document.screenWidth = profile.width; document.screenHeight = profile.height;
  document.colorMode = profile.colorMode; document.ink = profile.ink;
  document.palette = mad::panelPalette(profile); document.colorRegions.clear();
  if (profile.colorMode == mad::ColorMode::Zoned) {
    document.colorRegions.push_back({{0,0,profile.width,16},mad::Color::Yellow});
    document.colorRegions.push_back({{0,16,profile.width,static_cast<int16_t>(profile.height-16)},mad::Color::Blue});
  }
}

bool loadMadUi(const std::string& path, Document& document, std::string* error) {
  return loadDesign(path, document, error, 0);
}

}  // namespace madstudio
