#include <MadDisplayStudio/runtime.hpp>
#include <cassert>
#include <cstdio>
#include <cstring>
using namespace madstudio;

int main() {
  Document doc = Document::createDefault();
  doc.safeArea = mad::Insets(0); doc.focusMs = doc.scrollMs = doc.transitionMs = 0;
  auto& root = *doc.pages[0]->root;
  const auto* first = root.children[0].get();
  const auto* second = root.children[1].get();
  const std::string rootId = root.id, firstId = first->id;
  mad::BareBuffer<128,64> screen, before;
  const char* path = "/tmp/mad-layout-switch-roundtrip.mad";
  std::string error;

  for (auto type : kLayoutTypes) {
    PreviewRuntime current(doc); current.render(screen,0);
    if (root.type != type) assert(current.setLayoutType(root,type));
    assert(!current.setLayoutType(root,type));
    assert(!current.setLayoutType(root,NodeType::Button));
    assert(!current.setLayoutType(*root.children[0],NodeType::VerticalLayout));
    assert(root.children[0].get() == first && root.children[1].get() == second);
    assert(root.id == rootId && root.children[0]->id == firstId);
    assert(root.frame.width.unit == mad::Unit::Percent && root.frame.width.value == 100);
    assert(root.frame.height.unit == mad::Unit::Percent && root.frame.height.value == 100);
    PreviewRuntime changed(doc); assert(changed.valid()); changed.render(screen,0);
    assert(changed.find(root)->view().bounds().width == 128 && changed.find(root)->view().bounds().height == 64);
    assert(saveMadUi(doc,path,&error)); Document loaded;
    assert(loadMadUi(path,loaded,&error));
    assert(loaded.pages[0]->root->type == type && loaded.pages[0]->root->children.size() == 2);
    assert(loaded.pages[0]->root->children[0]->id == firstId);
    PreviewRuntime restored(loaded); assert(restored.valid()); restored.render(screen,0);
  }

  // Free placement starts at the actual arranged positions, retaining the image.
  { PreviewRuntime current(doc); current.render(screen,0); assert(current.setLayoutType(root,NodeType::HorizontalLayout)); }
  { PreviewRuntime current(doc); current.render(before,0);
    const auto oldFirst = current.find(*first)->view().bounds();
    const auto oldSecond = current.find(*second)->view().bounds();
    assert(current.setLayoutType(root,NodeType::FreeLayout));
    assert(root.children[0]->frame.x.value == oldFirst.x && root.children[1]->frame.x.value == oldSecond.x);
    assert(root.children[1]->frame.y.value == oldSecond.y);
  }
  { PreviewRuntime free(doc); free.render(screen,0);
    assert(std::memcmp(before.pixels(),screen.pixels(),screen.byteCount()) == 0);
  }

  // Hidden children have no arranged geometry to capture.
  auto hidden = doc.createNode(NodeType::Label); hidden->visible = false; hidden->hasFrame = true;
  hidden->frame = mad::Frame::px(4,5,42,12); Node* hiddenModel = hidden.get();
  root.children.push_back(std::move(hidden));
  { PreviewRuntime current(doc); current.render(screen,0); assert(current.setLayoutType(root,NodeType::VerticalLayout)); }
  { PreviewRuntime current(doc); current.render(screen,0); assert(current.setLayoutType(root,NodeType::FreeLayout)); }
  assert(hiddenModel->frame.x.value == 4 && hiddenModel->frame.width.value == 42 && hiddenModel->frame.height.value == 12);

  // A non-layout root still cannot replace a valid document.
  Document invalid = Document::createDefault(); invalid.pages[0]->root->type = NodeType::Label;
  invalid.pages[0]->root->children.clear();
  assert(saveMadUi(invalid,path,&error));
  Document loaded = Document::createDefault();
  assert(!loadMadUi(path,loaded,&error));
  assert(error == "page root must be a layout" && loaded.pages[0]->root->type == NodeType::VerticalLayout);
  std::remove(path);
}
