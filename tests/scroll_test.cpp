#include <MadDisplayStudio/runtime.hpp>
#include <cassert>
using namespace mad;
using namespace madstudio;

struct Art : Component {
  Size preferredSize() const override { return {32,24}; }
  void draw(Canvas& canvas) const override { canvas.fillRect(bounds_,Color::On); }
};

int main() {
  // A focusable button above a noninteractive tail must not pin the viewport.
  VerticalLayout root(Frame::px(0,0,32,24)); root.setScrollable(true); root.setGap(0);
  Button button("A"); Art tail; root.add(button); root.add(tail);
  mad::Page page(root); Compositor display; display.setSafeArea({0,0,0,0}); display.setNavigationAnimation(144,100);
  display.openPage(display.registerPage(page)); BareBuffer<32,24> screen;
  display.render(screen,0);
  assert(root.scrollContentExtent() == 35);
  assert(display.scrollAt({4,4},{0,200}));
  display.render(screen,50000); assert(root.scrollOffset() > 0 && root.scrollOffset() < 11);
  display.render(screen,200000); assert(root.scrollOffset() == 11);
  assert(page.focusedComponent() == &button);
  display.render(screen,400000); assert(root.scrollOffset() == 11); // no snap back
  assert(screen.isOn(0,23)); // the tail is reachable
  // Right-hand visual lane, thumb at the bottom and no graphic overlap.
  assert(!screen.isOn(29,23) && screen.isOn(30,23) && screen.isOn(31,23));
  assert(!screen.isOn(30,0) && screen.isOn(31,0));
  assert(!display.scrollAt({4,4},{0,200}));
  assert(page.focus(button)); display.render(screen,400000); display.render(screen,600000);
  assert(root.scrollOffset() == 0); // explicit focus requests still reveal a control
  assert(screen.isOn(30,0) && !screen.isOn(30,23));
  assert(display.dispatch(Action::Down)); display.render(screen,800000);
  assert(root.scrollOffset() == 8); // arrows reveal noninteractive content after the last control
  assert(display.dispatch(Action::Down)); display.render(screen,1000000);
  assert(root.scrollOffset() == 11 && !display.dispatch(Action::Down));

  // Horizontal content can be scrolled when the page has no focusable controls.
  VerticalLayout host;
  HorizontalLayout row(Frame::px(0,0,32,24)); row.setGap(0); row.setScrollable(true);
  Art first, second; row.add(first); row.add(second); host.add(row);
  mad::Page noFocus(host); noFocus.setNavigationAnimation(144,0); noFocus.draw(screen);
  assert(!noFocus.focusedComponent());
  assert(noFocus.dispatch(Action::Right)); noFocus.draw(screen); assert(row.scrollOffset() == 8);
  assert(noFocus.scrollAt({4,4},{100,0})); noFocus.draw(screen); assert(row.scrollOffset() == 32);
  assert(!screen.isOn(31,21) && screen.isOn(31,22) && screen.isOn(31,23));
  assert(!screen.isOn(0,22) && screen.isOn(0,23));
  assert(!noFocus.scrollAt({4,4},{100,0})); // no blank camera range from flow-container sizing
  assert(!noFocus.scrollAt({40,4},{-10,0})); // outside viewport
  row.setScrollable(false); noFocus.draw(screen); assert(row.scrollOffset() == 0);

  // Independent axes route to nested scrollers; a child at its edge bubbles up.
  VerticalLayout outer(Frame::px(0,0,32,24)); outer.setScrollable(true); outer.setGap(0);
  HorizontalLayout inner(Frame::px(0,0,29,20)); inner.setScrollable(true); inner.setGap(0);
  Art left,right,bottom; inner.add(left); inner.add(right); outer.add(inner); outer.add(bottom);
  mad::Page nested(outer); nested.setNavigationAnimation(144,0); nested.draw(screen);
  assert(nested.scrollAt({4,4},{8,5})); nested.draw(screen);
  assert(inner.scrollOffset() == 8 && outer.scrollOffset() == 5);
  assert(nested.scrollAt({4,4},{0,100})); nested.draw(screen);
  assert(outer.scrollOffset() == 20);

  // A camera over explicit-position content measures noninteractive leaves too.
  Document document = Document::createDefault(); document.safeArea = {0,0,0,0}; document.scrollMs = 0;
  auto& modelRoot = document.pages[0]->root; modelRoot->children.clear();
  auto board = document.createNode(NodeType::FreeLayout); board->hasFrame = true; board->frame = Frame::px(0,0,32,24);
  auto image = document.createNode(NodeType::Rectangle); image->hasFrame = true; image->frame = Frame::px(70,40,10,10); image->filled = true;
  board->children.push_back(std::move(image)); modelRoot->children.push_back(std::move(board));
  PreviewRuntime runtime(document); runtime.render(screen,0);
  assert(runtime.scrollAt({4,4},{100,100})); runtime.render(screen,1);
  const auto camera = runtime.compositor().currentPage()->scrollPosition();
  assert(camera.x == 48 && camera.y == 26 && screen.isOn(31,23));
  runtime.render(screen,200000); assert(runtime.compositor().currentPage()->scrollPosition().x == 48);
  assert(!runtime.scrollAt({4,4},{100,100}));

  // Bars do not appear when everything fits, and hidden children don't extend ranges.
  VerticalLayout fits(Frame::px(0,0,32,24)); fits.setScrollable(true); fits.setGap(0);
  Art only, hidden; hidden.setVisible(false); fits.add(only); fits.add(hidden);
  screen.clear(); fits.layout({0,0,32,24}); fits.draw(screen);
  assert(fits.scrollContentExtent() == 24 && !fits.scrollAt({4,4},{0,10}));
  assert(screen.isOn(31,23)); // no reserved lane when there is no overflow
  // Studio must preserve an explicit viewport instead of forcing fill on scroll.
  Document sizedDocument = Document::createDefault(); sizedDocument.safeArea = {0,0,0,0}; sizedDocument.scrollMs = 0;
  sizedDocument.pages[0]->root->children.clear(); sizedDocument.pages[0]->root->widthMode = SizeMode::Max;
  auto sizedScroll = sizedDocument.createNode(NodeType::VerticalLayout);
  Node* sizedModel = sizedScroll.get(); sizedScroll->hasFrame = true;
  sizedScroll->frame = {Length::px(0),Length::px(0),Length::percent(100),Length::px(12)};
  sizedScroll->scrollable = true;
  for (int i = 0; i < 3; ++i) sizedScroll->children.push_back(sizedDocument.createNode(NodeType::Button));
  sizedDocument.pages[0]->root->children.push_back(std::move(sizedScroll));
  PreviewRuntime sizedRuntime(sizedDocument); sizedRuntime.render(screen,0);
  auto* sizedLayout = dynamic_cast<Layout*>(sizedRuntime.find(*sizedModel)->component.get());
  assert(sizedLayout && sizedLayout->bounds().width == 32 && sizedLayout->bounds().height == 12);
  assert(sizedRuntime.dispatch(Action::Next) && sizedRuntime.dispatch(Action::Next));
  sizedRuntime.render(screen,200000);
  assert(sizedLayout->scrollOffset() == 23 && sizedRuntime.compositor().currentPage()->scrollPosition().y == 0);

}
