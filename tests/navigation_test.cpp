#include <MadDisplayStudio/runtime.hpp>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <fstream>

using namespace madstudio;

static void checkUnevenControlSequence(bool horizontal,bool scrollable,uint32_t intervalUs) {
  mad::VerticalLayout root;
  mad::HorizontalLayout row;
  mad::Layout& sequence = horizontal ? static_cast<mad::Layout&>(row) : static_cast<mad::Layout&>(root);
  if (horizontal) root.add(row);
  root.setSizeMode(mad::SizeMode::Max,mad::SizeMode::Max);
  sequence.setScrollable(scrollable);
  sequence.setGap(5);
  mad::Button buttons[] = {mad::Button("A"),mad::Button("B"),mad::Button("C"),mad::Button("D"),mad::Button("E"),mad::Button("F")};
  for (int i = 0; i < 6; ++i) {
    const bool large = i >= 1 && i <= 3;
    buttons[i].setSize(mad::SizeSpec::px(large ? 129 : 27,large ? 66 : 11));
    sequence.add(buttons[i]);
  }
  mad::Page page(root); mad::Compositor display; mad::BareBuffer<128,64> screen;
  display.openPage(display.registerPage(page)); uint32_t now = 0; display.render(screen,now);
  const auto forward = horizontal ? mad::Action::Right : mad::Action::Down;
  const auto backward = horizontal ? mad::Action::Left : mad::Action::Up;
  for (int i = 1; i < 6; ++i) {
    assert(display.dispatch(forward)); now += intervalUs; display.render(screen,now);
    assert(page.focusedComponent() == &buttons[i]);
  }
  for (int i = 4; i >= 0; --i) {
    assert(display.dispatch(backward)); now += intervalUs; display.render(screen,now);
    assert(page.focusedComponent() == &buttons[i]);
  }
  for (int i = 1; i < 6; ++i) { assert(display.dispatch(mad::Action::Next)); assert(page.focusedComponent() == &buttons[i]); }
  for (int i = 4; i >= 0; --i) { assert(display.dispatch(mad::Action::Previous)); assert(page.focusedComponent() == &buttons[i]); }
}

int main(int argc,char** argv) {
  for (uint32_t interval : {0u,16000u,400000u}) {
    for (bool scrollable : {false,true}) {
      checkUnevenControlSequence(false,scrollable,interval);
      checkUnevenControlSequence(true,scrollable,interval);
    }
  }
  // Arrow navigation preserves columns despite uneven control heights and row offsets.
  {
    mad::VerticalLayout root;
    mad::FreeLayout board(mad::Frame::px(0,0,110,90)); root.add(board);
    mad::Button buttons[7] = {mad::Button("A"),mad::Button("B"),mad::Button("C"),
        mad::Button("D"),mad::Button("E"),mad::Button("F"),mad::Button("WIDE")};
    const mad::Frame frames[] = {mad::Frame::px(0,0,12,11),mad::Frame::px(40,0,12,11),mad::Frame::px(80,0,12,15),
        mad::Frame::px(0,18,12,13),mad::Frame::px(40,20,12,11),mad::Frame::px(80,20,12,11),mad::Frame::px(0,42,110,11)};
    for (int i = 0; i < 7; ++i) board.add(buttons[i],frames[i]);
    mad::Page page(root); mad::BareBuffer<128,128> screen; page.draw(screen);
    assert(page.focus(buttons[1]));
    assert(!page.dispatch(mad::Action::Up));
    assert(page.dispatch(mad::Action::Down) && page.focusedComponent() == &buttons[4]);
    assert(page.dispatch(mad::Action::Up) && page.focusedComponent() == &buttons[1]);
    assert(page.dispatch(mad::Action::Down) && page.focusedComponent() == &buttons[4]);
    assert(page.dispatch(mad::Action::Down) && page.focusedComponent() == &buttons[6]);
    assert(!page.dispatch(mad::Action::Down));
    assert(page.dispatch(mad::Action::Up) && page.focusedComponent() == &buttons[4]);
    assert(page.focus(buttons[0]));
    for (int i = 1; i < 7; ++i) { assert(page.dispatch(mad::Action::Next)); assert(page.focusedComponent() == &buttons[i]); }
    assert(!page.dispatch(mad::Action::Next));
    for (int i = 5; i >= 0; --i) { assert(page.dispatch(mad::Action::Previous)); assert(page.focusedComponent() == &buttons[i]); }
    assert(!page.dispatch(mad::Action::Previous));
    buttons[1].setEnabled(false); buttons[2].setVisible(false); page.draw(screen);
    assert(page.dispatch(mad::Action::Next) && page.focusedComponent() == &buttons[3]);
    assert(page.dispatch(mad::Action::Previous) && page.focusedComponent() == &buttons[0]);
  }
  // Row membership follows allocated bounds, not smaller centered control artwork.
  {
    mad::VerticalLayout root; mad::HorizontalLayout row; root.add(row);
    mad::Button button("A");
    mad::Checkbox checkbox(mad::SizeSpec::px(40,45));
    mad::Switch toggle(mad::SizeSpec::px(40,11));
    mad::RadioButton radio(mad::SizeSpec::px(40,50));
    row.add(button); row.add(checkbox); row.add(toggle); row.add(radio);
    mad::Component* ordered[] = {&button,&checkbox,&toggle,&radio};
    mad::Page page(root); mad::BareBuffer<192,64> screen; page.draw(screen);
    for (int i = 1; i < 4; ++i) { assert(page.dispatch(mad::Action::Right)); assert(page.focusedComponent() == ordered[i]); }
    for (int i = 2; i >= 0; --i) { assert(page.dispatch(mad::Action::Left)); assert(page.focusedComponent() == ordered[i]); }
  }
  Document document = Document::createFocusDemo();
  PreviewRuntime runtime(document);
  mad::BareBuffer<128,64> canvas, previous;
  uint32_t now = 0;
  auto render = [&]() { runtime.render(canvas,now); };
  auto advance = [&](uint32_t ms) {
    const uint32_t end = now + ms * 1000;
    while (now < end) { now += end - now < 16667u ? end - now : 16667u; render(); }
  };
  auto visible = [&]() {
    const mad::Rect r = runtime.compositor().currentPage()->focusVisualBounds();
    assert(r.x >= 1 && r.y >= 1 && r.x + r.width <= 127 && r.y + r.height <= 63);
  };
  render(); visible();
  auto* page = runtime.compositor().currentPage();
  assert(page->scrollPosition().x == 0 && page->scrollPosition().y == 0);
  std::memcpy(previous.pixels(),canvas.pixels(),canvas.byteCount());
  assert(runtime.dispatch(mad::Action::Right)); render();
  assert(std::memcmp(previous.pixels(),canvas.pixels(),canvas.byteCount()) == 0);
  std::ofstream frames;
  if (argc > 1) frames.open(argv[1],std::ios::binary);
  for (int i = 0; i < 24; ++i) {
    advance(16);
    if (i == 5) assert(page->focusVisualBounds().width > 60);
    if (frames) frames.write(reinterpret_cast<const char*>(canvas.pixels()),canvas.byteCount());
  }
  visible(); assert(page->scrollPosition().x > 0);
  for (int i = 0; i < 2; ++i) { assert(runtime.dispatch(mad::Action::Right)); advance(400); visible(); }
  for (int i = 0; i < 3; ++i) { assert(runtime.dispatch(mad::Action::Down)); advance(400); visible(); }
  assert(page->scrollPosition().x > 100 && page->scrollPosition().y > 20);
  for (int i = 0; i < 3; ++i) { assert(runtime.dispatch(mad::Action::Left)); advance(400); visible(); }
  for (int i = 0; i < 3; ++i) { assert(runtime.dispatch(mad::Action::Up)); advance(400); visible(); }
  assert(page->scrollPosition().x == 0 && page->scrollPosition().y == 0);
  assert(runtime.dispatch(mad::Action::Right)); advance(80);
  const auto before = page->focusVisualBounds();
  assert(runtime.dispatch(mad::Action::Right)); render();
  const auto interrupted = page->focusVisualBounds();
  assert(before.x == interrupted.x && before.y == interrupted.y && before.width == interrupted.width);
  advance(500); visible();
  // Pause applies to both the camera and liquid geometry.
  std::memcpy(previous.pixels(),canvas.pixels(),canvas.byteCount()); render();
  assert(std::memcmp(previous.pixels(),canvas.pixels(),canvas.byteCount()) == 0);
  const auto retainedCamera = page->scrollPosition();
  for (auto& node : document.pages[0]->root->children[0]->children) runtime.find(*node)->component->setVisible(false);
  render(); advance(400);
  // Losing focus does not reset the camera of an explicitly sized canvas.
  assert(page->scrollPosition().x == retainedCamera.x && page->scrollPosition().y == retainedCamera.y);
  for (size_t i = 0; i < canvas.byteCount(); ++i) assert(canvas.pixels()[i] == 0);

  // Nested scroll views interpolate their own positions without double panning.
  mad::VerticalLayout column;
  mad::HorizontalLayout rows[3];
  mad::Button buttons[3][3] = {{mad::Button("A"),mad::Button("B"),mad::Button("C")},
      {mad::Button("D"),mad::Button("E"),mad::Button("F")},
      {mad::Button("G"),mad::Button("H"),mad::Button("I")}};
  column.setSizeMode(mad::SizeMode::Max,mad::SizeMode::Max); column.setScrollable(true); column.setGap(12);
  for (int row = 0; row < 3; ++row) {
    rows[row].setSize(mad::SizeSpec::px(126,24)); rows[row].setScrollable(true);
    for (int col = 0; col < 3; ++col) {
      buttons[row][col].setSize(mad::SizeSpec::px(70,24)); rows[row].add(buttons[row][col]);
    }
    column.add(rows[row]);
  }
  mad::Page nested(column); mad::Compositor compositor;
  compositor.openPage(compositor.registerPage(nested));
  now = 0; compositor.render(canvas,now);
  assert(nested.focus(buttons[2][2])); compositor.render(canvas,now);
  const auto horizontalTarget = rows[2].scrollTarget(), verticalTarget = column.scrollTarget();
  assert(horizontalTarget > 0 && verticalTarget > 0);
  compositor.render(canvas,100000);
  assert(rows[2].scrollOffset() > 0 && rows[2].scrollOffset() < horizontalTarget);
  assert(column.scrollOffset() > 0 && column.scrollOffset() < verticalTarget);
  compositor.render(canvas,500000);
  assert(rows[2].scrollOffset() == horizontalTarget && column.scrollOffset() == verticalTarget);
  assert(nested.focus(buttons[0][0])); compositor.render(canvas,500000);
  compositor.render(canvas,1000000);
  assert(column.scrollOffset() == 0);
  assert(nested.scrollPosition().x == 0 && nested.scrollPosition().y == 0);
  // A shifted Canvas must rasterize rectangles beyond its physical dimensions.
  canvas.clear(); mad::CanvasView camera(canvas,{0,0,128,64},-200,-80);
  camera.fillRect({205,85,4,4},mad::Color::On);
  assert(canvas.isOn(5,5) && canvas.isOn(8,8) && !canvas.isOn(9,9));

  mad::VerticalLayout originRoot;
  originRoot.setSizeMode(mad::SizeMode::Max,mad::SizeMode::Max);
  mad::FreeLayout negative(mad::Frame::px(0,0,220,120));
  mad::Button origin("HOME"), outside("LEFT");
  negative.add(origin,mad::Frame::px(0,0,40,18));
  negative.add(outside,mad::Frame::px(-90,-40,40,18));
  originRoot.add(negative);
  mad::Page negativePage(originRoot); mad::Compositor negativeCompositor;
  negativeCompositor.openPage(negativeCompositor.registerPage(negativePage));
  negativeCompositor.render(canvas,0);
  assert(negativePage.focus(outside)); negativeCompositor.render(canvas,0);
  negativeCompositor.render(canvas,500000);
  assert(negativePage.scrollPosition().x < 0 && negativePage.scrollPosition().y < 0);
  const auto outsideBounds = negativePage.focusVisualBounds();
  assert(outsideBounds.x >= 1 && outsideBounds.y >= 1);
  assert(negativePage.focus(origin)); negativeCompositor.render(canvas,500000);
  negativeCompositor.render(canvas,1000000);
  const auto homeBounds = negativePage.focusVisualBounds();
  assert(homeBounds.x >= 1 && homeBounds.y >= 1 && homeBounds.x + homeBounds.width <= 127 && homeBounds.y + homeBounds.height <= 63);
  assert(negativePage.scrollPosition().x == -86 && negativePage.scrollPosition().y == -40);

  const char* path = "/tmp/mad-navigation-roundtrip.madui";
  document.focusMs = 510; document.scrollMs = 390;
  std::string error; assert(saveMadUi(document,path,&error));
  Document loaded; assert(loadMadUi(path,loaded,&error));
  assert(loaded.focusMs == 510 && loaded.scrollMs == 390);
  std::remove(path);
  if (argc > 2) {
    auto demo = Document::createFocusDemo();
    assert(saveMadUi(demo,argv[2],&error));
  }
  mad::VerticalLayout wrappedRoot;
  mad::Button first("ONE"), second("TWO");
  wrappedRoot.add(first); wrappedRoot.add(second);
  mad::Page wrappedPage(wrappedRoot);
  mad::Compositor wrapped;
  wrapped.openPage(wrapped.registerPage(wrappedPage));
  wrapped.render(canvas,0xffff0000u);
  assert(wrapped.dispatch(mad::Action::Down));
  wrapped.render(canvas,0xffff1000u);
  wrapped.render(canvas,0x00010000u);
  wrapped.render(canvas,0x00100000u);
  assert(wrappedPage.focusedComponent() == &second);
  const auto bounds = wrappedPage.focusVisualBounds();
  assert(bounds.y == second.focusBounds().y && bounds.height == second.focusBounds().height);
  // Instant navigation remains available for constrained targets.
  wrapped.setNavigationAnimation(0,0);
  assert(wrapped.dispatch(mad::Action::Up));
  wrapped.render(canvas,0x00200000u);
  assert(wrappedPage.focusedComponent() == &first);

}
