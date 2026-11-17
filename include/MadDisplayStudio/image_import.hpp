#pragma once
#include "madui.hpp"
namespace madstudio {
std::shared_ptr<const ImageSource> decodeImage(const std::string& name,const std::vector<uint8_t>& bytes,std::string& error);
bool importImage(Node& node,const std::string& path,const Document& document,std::string& error);
bool processImage(Node& node,const Document& document);
void refreshImportedImages(Document& document);
uint64_t imageFilterKey(const Node& node,const Document& document);
std::string imageSourceHex(const ImageSource& source);
std::shared_ptr<const ImageSource> imageSourceFromHex(const std::string& name,const std::string& hex,std::string& error);
}
