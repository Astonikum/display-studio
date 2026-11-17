#pragma once

#include <map>
#include <string>
#include <vector>

namespace madstudio {
namespace json {

struct Value {
  enum Type { Null, Boolean, Number, String, Object, Array } type = Null;
  bool boolean = false;
  double number = 0.0;
  std::string string;
  std::map<std::string, Value> object;
  std::vector<Value> array;
};

bool parse(const std::string& source, Value& result, std::string& error);
const Value* field(const Value& value, const char* key);
std::string stringField(const Value& value, const char* key, const std::string& fallback);
int numberField(const Value& value, const char* key, int fallback);
double realField(const Value& value, const char* key, double fallback);
bool boolField(const Value& value, const char* key, bool fallback);
void appendString(std::string& output, const std::string& value);

}  // namespace json
}  // namespace madstudio
