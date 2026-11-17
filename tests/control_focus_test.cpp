#include <MadDisplayStudio/runtime.hpp>
#include <cassert>
#include <cstring>

// Timed focus must preserve every fixed outline and match native control artwork.
template<class Control> void check(bool state,bool expanded) {
  mad::VerticalLayout nativeRoot, animatedRoot;
  Control native(state), animated(state);
  if (expanded) {
    native.setSize(mad::SizeSpec::px(45,25));
    animated.setSize(mad::SizeSpec::px(45,25));
  }
  nativeRoot.add(native); animatedRoot.add(animated);
  mad::Page nativePage(nativeRoot), animatedPage(animatedRoot);
  mad::BareBuffer<128,64> expected, actual;
  nativePage.draw(expected,{1,1,126,62});
  mad::Compositor compositor;
  compositor.openPage(compositor.registerPage(animatedPage));
  compositor.render(actual,0);
  assert(std::memcmp(expected.pixels(),actual.pixels(),actual.byteCount()) == 0);
  compositor.render(actual,500000);
  assert(std::memcmp(expected.pixels(),actual.pixels(),actual.byteCount()) == 0);
  const auto artwork = animated.focusBounds(), focus = animatedPage.focusVisualBounds();
  assert(focus.x == artwork.x && focus.y == artwork.y && focus.width == artwork.width && focus.height == artwork.height);
}

void switchBorders() {
  for (bool initial : {false,true}) for (bool focused : {false,true}) {
    mad::Switch control(initial);
    control.layout({0,0,20,11}); control.setFocused(focused);
    control.update(0); control.toggle(); control.update(0);
    for (uint32_t now = 0; now <= 220; now += 20) {
      control.update(now);
      mad::BareBuffer<20,11> buffer;
      control.draw(buffer);
      for (int row = 0; row < 11; ++row) {
        const int cut = row == 0 || row == 10 ? 2 : (row == 1 || row == 9 ? 1 : 0);
        for (int col = cut; col < 20 - cut; ++col)
          if (row == 0 || row == 10 || col == cut || col == 19 - cut) assert(buffer.isOn(col,row));
      }
      const float progress = mad::ease(now / 220.f,mad::Easing::CubicInOut);
      const int offset = static_cast<int>((initial ? 9 * (1 - progress) : 9 * progress) + .5f);
      for (int row = 0; row < 7; ++row) for (int col = 0; col < 7; ++col) {
        if ((row == 0 || row == 6) && (col == 0 || col == 6)) continue;
        if (row == 0 || row == 6 || col == 0 || col == 6) assert(buffer.isOn(2 + offset + col,2 + row));
      }
      // Fixed outlines also survive a moving liquid mask crossing the switch.
      control.setFocused(false);
      mad::LiquidFocus blob;
      blob.follow({-20,1,18,9},false,0,144,1);
      blob.follow({1,1,18,9},true,0,144,1);
      blob.follow({1,1,18,9},false,now,144,1);
      buffer.clear();
      mad::LiquidFocusCanvas liquid(buffer,blob,{0,0});
      liquid.fillRect({0,0,20,11},mad::Color::Off); control.draw(liquid);
      assert(buffer.isOn(2,0) && buffer.isOn(0,5) && buffer.isOn(19,5));
      for (int col = 1; col < 6; ++col) assert(buffer.isOn(2 + offset + col,2));
      control.setFocused(focused);
    }
  }
}

void convexFocus() {
  assert(mad::kDefaultFocusDurationMs == 144);
  assert(madstudio::Document::createDefault().focusMs == mad::kDefaultFocusDurationMs);
  mad::LiquidFocus blob;
  const mad::Rect target{70,22,52,16};
  blob.follow({4,4,24,12},false,0,mad::kDefaultFocusDurationMs);
  blob.follow(target,true,0,mad::kDefaultFocusDurationMs);
  for (uint32_t now = 0; now <= mad::kDefaultFocusDurationMs; now += 8) {
    blob.follow(target,false,now,mad::kDefaultFocusDurationMs);
    const auto r = blob.bounds();
    int previousHeight = 0;
    for (int x = 0; x < r.width / 2; ++x) {
      int height = 0;
      for (int y = 0; y < r.height; ++y) if (blob.contains(r.x + x,r.y + y)) ++height;
      assert(height >= previousHeight); previousHeight = height;
    }
    int previousWidth = 0;
    for (int y = 0; y < r.height / 2; ++y) {
      int width = 0;
      for (int x = 0; x < r.width; ++x) if (blob.contains(r.x + x,r.y + y)) ++width;
      assert(width >= previousWidth); previousWidth = width;
    }
  }
  const auto settled = blob.bounds();
  assert(settled.x == target.x && settled.y == target.y && settled.width == target.width && settled.height == target.height);
}

int main() {
  switchBorders(); convexFocus();
  for (bool state : {false,true}) for (bool expanded : {false,true}) {
    check<mad::Checkbox>(state,expanded);
    check<mad::RadioButton>(state,expanded);
    check<mad::Switch>(state,expanded);
  }
  // Studio's foreground/background mapping is identical to the library defaults.
  for (auto type : {madstudio::NodeType::Checkbox,madstudio::NodeType::RadioButton}) {
    auto document = madstudio::Document::createDefault();
    document.pages[0]->root->children.clear();
    auto control = document.createNode(type); auto* model = control.get();
    document.pages[0]->root->children.push_back(std::move(control));
    madstudio::PreviewRuntime runtime(document); mad::BareBuffer<128,64> buffer;
    runtime.render(buffer,0);
    mad::VerticalLayout root;
    mad::Checkbox checkbox; mad::RadioButton radio;
    root.add(type == madstudio::NodeType::Checkbox ? static_cast<mad::Component&>(checkbox) : static_cast<mad::Component&>(radio));
    mad::Page page(root); mad::BareBuffer<128,64> expected;
    page.draw(expected,{1,1,126,62});
    assert(std::memcmp(expected.pixels(),buffer.pixels(),buffer.byteCount()) == 0);
    assert(runtime.activate(*model));
    assert(type == madstudio::NodeType::Checkbox ? model->checked : model->selected);
  }
  // Reopening the current page restarts entrances without resetting control state.
  auto document = madstudio::Document::createAnimated();
  madstudio::PreviewRuntime preview(document); mad::BareBuffer<128,64> start, finish, restarted;
  preview.render(start,0); preview.render(finish,2000000);
  assert(std::memcmp(start.pixels(),finish.pixels(),start.byteCount()) != 0);
  assert(preview.openPage(0)); preview.render(restarted,2000000);
  assert(std::memcmp(start.pixels(),restarted.pixels(),start.byteCount()) == 0);
}
