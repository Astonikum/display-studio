#include <assert.h>
#include <cstdio>
#include <fstream>
#include <sstream>

#include <MadDisplayStudio/madui.hpp>

int main() {
  madstudio::Document original = madstudio::Document::createDefault();
  assert(original.pages[0]->root->hasFrame);
  assert(original.pages[0]->root->frame.width.unit == mad::Unit::Percent && original.pages[0]->root->frame.width.value == 100);
  assert(original.pages[0]->root->frame.height.unit == mad::Unit::Percent && original.pages[0]->root->frame.height.value == 100);
  original.name = "Тестовый UI";
  original.displayProfile = "ssd1306-64x48";
  original.screenWidth = 64;
  original.screenHeight = 48;
  original.pages[0]->root->children[0]->frame = mad::Frame::percent(10, 20, 80, 25);
  original.pages[0]->root->children[0]->hasFrame = true;
  original.pages[0]->root->children[0]->frame.width.unit = mad::Unit::Percent;
  std::unique_ptr<madstudio::Node> panel = original.createNode(madstudio::NodeType::Panel);
  panel->children.push_back(original.createNode(madstudio::NodeType::Button));
  panel->children.back()->text = "Nested";
  original.pages[0]->root->children.push_back(std::move(panel));

  const char* path = "/tmp/madui-roundtrip.madui";
  std::string error;
  assert(madstudio::saveMadUi(original, path, &error));
  madstudio::Document loaded;
  assert(madstudio::loadMadUi(path, loaded, &error));
  assert(loaded.name == original.name);
  assert(loaded.screenWidth == 64 && loaded.screenHeight == 48);
  assert(loaded.pages.size() == 1);
  assert(loaded.pages[0]->root->children[0]->frame.x.unit == mad::Unit::Percent);
  assert(loaded.pages[0]->root->children[2]->children.size() == 1);
  assert(loaded.pages[0]->root->children[2]->children[0]->children[0]->text == "Nested");
  assert(loaded.displayProfile == original.displayProfile);

  const char* projectPath = "/tmp/mad-studio-roundtrip.mad";
  const std::string legacy = R"({"format":"mad","version":1,"name":"Legacy display","uiFile":"madui-roundtrip.madui","graphs":[{"old":"logic"}]})";
  { std::ofstream file(projectPath); file << legacy; }
  assert(madstudio::loadMadUi(projectPath, loaded, &error));
  assert(loaded.name == original.name && loaded.displayProfile == original.displayProfile);
  assert(madstudio::saveMadUi(loaded, projectPath, &error));
  const std::string backupPath = std::string(projectPath) + ".legacy.bak";
  { std::ifstream backup(backupPath); std::ostringstream bytes; bytes << backup.rdbuf(); assert(bytes.str() == legacy); }
  { std::ifstream saved(projectPath); std::ostringstream bytes; bytes << saved.rdbuf();
    assert(bytes.str().find("uiFile") == std::string::npos);
    assert(bytes.str().find("\"format\": \"mad\"") != std::string::npos);
  }
  assert(madstudio::loadMadUi(projectPath, loaded, &error));
  // Legacy UI files remain readable and infer their named display profile.
  { std::ifstream saved(path); std::ostringstream bytes; bytes << saved.rdbuf();
    std::string old = bytes.str();
    old.replace(old.find("\"format\": \"mad\""), 15, "\"format\": \"madui\"");
    old.replace(old.find("\"version\": 6"), 12, "\"version\": 2");
    { std::ofstream output(path); output << old; }
  }
  assert(madstudio::loadMadUi(path, loaded, &error));
  for (const char* invalid : {R"({"format":"mad","version":99,"screen":{},"pages":[]})",
                            R"({"format":"mad","version":2,"uiFile":""})",
                            R"({"format":"mad","version":2,"uiFile":"mad-studio-roundtrip.mad"})"}) {
    { std::ofstream file(projectPath); file << invalid; }
    assert(!madstudio::loadMadUi(projectPath, loaded, &error));
    assert(loaded.name == original.name);
  }
  { std::ofstream file(projectPath); file << legacy << " "; }
  assert(!madstudio::saveMadUi(loaded, projectPath, &error));
  { std::ifstream saved(projectPath); std::ostringstream bytes; bytes << saved.rdbuf(); assert(bytes.str() == legacy + " "); }
  // Each axis keeps its own sizing unit through persistence.
  auto& sized = *original.pages[0]->root->children[0];
  sized.hasFrame = true;
  sized.frame.width = mad::Length::autoSize(); sized.frame.height = mad::Length::percent(50);
  assert(madstudio::saveMadUi(original,path,&error));
  assert(madstudio::loadMadUi(path,loaded,&error));
  assert(loaded.pages[0]->root->children[0]->frame.width.unit == mad::Unit::Auto);
  assert(loaded.pages[0]->root->children[0]->frame.height.unit == mad::Unit::Percent);
  std::remove(path);
  std::remove(backupPath.c_str());
  std::remove(projectPath);
  return 0;
}
