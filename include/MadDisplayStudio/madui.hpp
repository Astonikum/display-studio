#pragma once
#include <MadDisplay/panels.hpp>

#include <memory>
#include <string>
#include <vector>

#include <MadDisplay/MadDisplay.hpp>

namespace madstudio {

// These are the runtime component types exposed by MadDisplay. Keeping the
// editor document in terms of the library's public sizing and style types
// keeps the editor and simulator on the same rendering contract.
enum class NodeType : uint8_t {
  VerticalLayout,
  HorizontalLayout,
  Container,
  Panel,
  FreeLayout,
  Label,
  Button,
  Checkbox,
  Switch,
  RadioButton,
  Canvas, Image, Rectangle, Ellipse, Line, GridLayout
};

static constexpr NodeType kNodeTypes[] = {
    NodeType::VerticalLayout, NodeType::HorizontalLayout, NodeType::Container,
    NodeType::Panel, NodeType::FreeLayout, NodeType::Label, NodeType::Button,
    NodeType::Checkbox, NodeType::Switch, NodeType::RadioButton, NodeType::Canvas, NodeType::Image, NodeType::Rectangle, NodeType::Ellipse, NodeType::Line, NodeType::GridLayout
};

static constexpr NodeType kLayoutTypes[] = {
    NodeType::VerticalLayout, NodeType::HorizontalLayout, NodeType::GridLayout,
    NodeType::FreeLayout, NodeType::Container, NodeType::Panel
};
bool isLayout(NodeType type);
bool isContentControl(NodeType type);
void normalizeContent(struct Document& document);
const char* nodeTypeName(NodeType type);
bool nodeTypeFromName(const std::string& name, NodeType& type);

const char* entranceName(mad::Entrance effect);
const char* easingName(mad::Easing easing);

struct ImageSource {
  std::string name;
  std::vector<uint8_t> encoded, rgba;
  int width = 0, height = 0;
  uint64_t fingerprint = 0;
};
struct ImageFilters {
  float brightness = 0, contrast = 1, gamma = 1, saturation = 1;
  float threshold = .5f, ditherStrength = 1, alphaThreshold = .5f;
  int fit = 0, dither = 1; // Contain/Cover/Stretch; None/Bayer/Floyd-Steinberg.
  bool invert = false;
};

struct TextAsset;

struct Node {
  explicit Node(NodeType type = NodeType::Container, const std::string& name = std::string());
  ~Node();

  Node(const Node&) = delete;
  Node& operator=(const Node&) = delete;

  std::string id;
  std::string name;
  NodeType type;
  std::string text;
  int fontMode = 0, fontSize = 12; // Auto / Pixel / Unicode.
  std::string fontFamily = "System";
  std::shared_ptr<const TextAsset> textAsset;
  uint64_t processedTextKey = 0;
  std::string textWarning;

  bool hasFrame = false;
  mad::Frame frame;
  mad::SizeMode widthMode = mad::SizeMode::Fit;
  mad::SizeMode heightMode = mad::SizeMode::Fit;
  bool visible = true;
  bool enabled = true;

  mad::Color foreground = mad::Color::On;
  mad::Color background = mad::Color::Off;
  mad::HorizontalAlign horizontalAlign = mad::HorizontalAlign::Left;
  mad::VerticalAlign verticalAlign = mad::VerticalAlign::Center;
  uint8_t textScale = 1;
  uint8_t padding = 1;
  bool filled = false;
  bool checked = false;
  bool on = false;
  bool selected = false;
  uint8_t gap = mad::kDefaultGap;
  uint8_t columns = 2, columnGap = mad::kDefaultGap;
  bool scrollable = false;
  bool animated = false;
  mad::Entrance entrance = mad::Entrance::Rise;
  mad::Easing easing = mad::Easing::CubicOut;
  uint16_t durationMs = 480;
  uint16_t delayMs = 0;
  uint16_t switchDurationMs = 220;
  int graphicsWidth = 24;
  int graphicsHeight = 16;
  std::vector<mad::MatrixPixel> imagePixels;
  std::vector<mad::Color> imageColors;
  std::vector<uint8_t> imageCoverage;
  std::shared_ptr<const ImageSource> imageSource;
  ImageFilters imageFilters;
  uint64_t processedImageKey = 0; // Editor cache, never serialized.

  void invalidateImageHash() { imageHashValid_ = false; }
  uint64_t imageHash() const;

  std::vector<mad::DrawCommand> commands;

  // Preview-only resolved bounds. They are intentionally not serialized.
  mad::Rect previewBounds = {0, 0, 0, 0};
  mad::Rect previewContentBounds = {0, 0, 0, 0};

  std::vector<std::unique_ptr<Node> > children;
 private:
  mutable uint64_t imageHash_ = 0;
  mutable bool imageHashValid_ = false;
};

struct Page {
  Page();
  ~Page();

  Page(const Page&) = delete;
  Page& operator=(const Page&) = delete;

  std::string name;
  mad::Color background = mad::Color::Off;
  std::unique_ptr<Node> root;
};

using DisplayProfile = mad::PanelSpec;
static const auto& kDisplayProfiles = mad::kPanelProfiles;

struct Document {
  Document();
  ~Document();

  Document(const Document&) = delete;
  Document& operator=(const Document&) = delete;
  Document(Document&&) = default;
  Document& operator=(Document&&) = default;

  static Document createDefault();
  static Document createAnimated();
  static Document createFocusDemo();
  std::unique_ptr<Node> createNode(NodeType type);
  void rebuildIdCounter();

  std::string name = "Untitled UI";
  std::string displayProfile = "ssd1306-128x64";
  mad::ColorMode colorMode = mad::ColorMode::Monochrome;
  mad::Palette palette;
  mad::Color ink = mad::Color::White;
  std::vector<mad::ColorRegion> colorRegions;
  mad::ColorProfile displayColors() const {
    mad::ColorProfile profile; profile.mode = colorMode; profile.palette = palette; profile.ink = ink;
    profile.regions = colorRegions.empty() ? nullptr : colorRegions.data();
    profile.regionCount = static_cast<uint8_t>(colorRegions.size()); return profile;
  }
  int screenWidth = 128;
  int screenHeight = 64;
  size_t currentPage = 0;
  uint16_t transitionMs = 320;
  uint16_t focusMs = mad::kDefaultFocusDurationMs;
  uint16_t scrollMs = 280;
  mad::Easing transitionEasing = mad::Easing::CubicInOut;
  int16_t fps = 60;
  mad::Insets safeArea{1, 1, 1, 1};
  std::vector<std::unique_ptr<Page> > pages;

 private:
  uint32_t nextId_ = 1;
};

void applyDisplayProfile(Document& document,const DisplayProfile& profile);

bool saveMadUi(const Document& document, const std::string& path, std::string* error = nullptr);
bool loadMadUi(const std::string& path, Document& document, std::string* error = nullptr);

}  // namespace madstudio
