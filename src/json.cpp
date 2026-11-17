#include <MadDisplayStudio/json.hpp>
#include <MadDisplay/unicode.hpp>

#include <ctype.h>

#include <cstdlib>
#include <limits>
#include <sstream>

namespace madstudio {
namespace json {

namespace {

class Parser {
 public:
  explicit Parser(const std::string& source) : source_(source) {}

  bool parse(Value& result, std::string& error) {
    skipWhitespace();
    if (!parseValue(result, error)) return false;
    skipWhitespace();
    if (position_ != source_.size()) return fail(error, "unexpected data");
    return true;
  }

 private:
  bool parseValue(Value& result, std::string& error) {
    skipWhitespace();
    if (position_ == source_.size()) return fail(error, "unexpected end of file");
    const char current = source_[position_];
    if (current == '{') return parseObject(result, error);
    if (current == '[') return parseArray(result, error);
    if (current == '"') {
      result.type = Value::String;
      return parseString(result.string, error);
    }
    if (source_.compare(position_, 4, "true") == 0) {
      position_ += 4;
      result.type = Value::Boolean;
      result.boolean = true;
      return true;
    }
    if (source_.compare(position_, 5, "false") == 0) {
      position_ += 5;
      result.type = Value::Boolean;
      result.boolean = false;
      return true;
    }
    if (source_.compare(position_, 4, "null") == 0) {
      position_ += 4;
      result.type = Value::Null;
      return true;
    }
    return parseNumber(result, error);
  }

  bool parseObject(Value& result, std::string& error) {
    ++position_;
    result.type = Value::Object;
    skipWhitespace();
    if (take('}')) return true;
    while (position_ < source_.size()) {
      skipWhitespace();
      if (position_ == source_.size() || source_[position_] != '"')
        return fail(error, "object key must be a string");
      std::string key;
      if (!parseString(key, error)) return false;
      skipWhitespace();
      if (!take(':')) return fail(error, "expected ':'");
      Value value;
      if (!parseValue(value, error)) return false;
      result.object[key] = value;
      skipWhitespace();
      if (take('}')) return true;
      if (!take(',')) return fail(error, "expected ',' or '}'");
    }
    return fail(error, "unterminated object");
  }

  bool parseArray(Value& result, std::string& error) {
    ++position_;
    result.type = Value::Array;
    skipWhitespace();
    if (take(']')) return true;
    while (position_ < source_.size()) {
      Value value;
      if (!parseValue(value, error)) return false;
      result.array.push_back(value);
      skipWhitespace();
      if (take(']')) return true;
      if (!take(',')) return fail(error, "expected ',' or ']'");
    }
    return fail(error, "unterminated array");
  }

  bool parseNumber(Value& result, std::string& error) {
    const size_t start = position_;
    if (position_ < source_.size() && source_[position_] == '-') ++position_;
    if (position_ < source_.size() && source_[position_] == '0') ++position_;
    else {
      const size_t digits = position_;
      while (position_ < source_.size() && isdigit(static_cast<unsigned char>(source_[position_]))) ++position_;
      if (digits == position_) return fail(error, "expected value");
    }
    if (position_ < source_.size() && source_[position_] == '.') {
      ++position_;
      const size_t fraction = position_;
      while (position_ < source_.size() && isdigit(static_cast<unsigned char>(source_[position_]))) ++position_;
      if (fraction == position_) return fail(error, "invalid number");
    }
    if (position_ < source_.size() && (source_[position_] == 'e' || source_[position_] == 'E')) {
      ++position_;
      if (position_ < source_.size() && (source_[position_] == '+' || source_[position_] == '-')) ++position_;
      const size_t exponent = position_;
      while (position_ < source_.size() && isdigit(static_cast<unsigned char>(source_[position_]))) ++position_;
      if (exponent == position_) return fail(error, "invalid number");
    }
    char* end = nullptr;
    const double value = strtod(source_.c_str() + start, &end);
    if (end != source_.c_str() + position_) return fail(error, "invalid number");
    result.type = Value::Number;
    result.number = value;
    return true;
  }

  bool parseString(std::string& result, std::string& error) {
    if (!take('"')) return fail(error, "expected string");
    result.clear();
    while (position_ < source_.size()) {
      const unsigned char current = static_cast<unsigned char>(source_[position_++]);
      if (current == '"') {
        size_t offset = 0;
        while (offset < result.size()) {
          const auto c = static_cast<unsigned char>(result[offset]);
          if (c < 128) { ++offset; continue; }
          const char* start = result.c_str()+offset; const char* cursor = start;
          const uint32_t cp = mad::nextUtf8Codepoint(cursor);
          if (cp == 0xfffd && !(cursor-start == 3 && c == 0xef && static_cast<unsigned char>(start[1]) == 0xbf && static_cast<unsigned char>(start[2]) == 0xbd)) return fail(error,"invalid UTF-8 string");
          offset = static_cast<size_t>(cursor-result.c_str());
        }
        return true;
      }
      if (current < 0x20) return fail(error,"unescaped control character");
      if (current != '\\') {
        result.push_back(static_cast<char>(current));
        continue;
      }
      if (position_ == source_.size()) return fail(error, "unterminated escape");
      const char escaped = source_[position_++];
      switch (escaped) {
        case '"': result.push_back('"'); break;
        case '\\': result.push_back('\\'); break;
        case '/': result.push_back('/'); break;
        case 'b': result.push_back('\b'); break;
        case 'f': result.push_back('\f'); break;
        case 'n': result.push_back('\n'); break;
        case 'r': result.push_back('\r'); break;
        case 't': result.push_back('\t'); break;
        case 'u': {
          uint32_t codepoint = 0;
          if (!unicodeUnit(codepoint,error)) return false;
          if (codepoint >= 0xd800 && codepoint <= 0xdbff) {
            if (position_+2 > source_.size() || source_[position_] != '\\' || source_[position_+1] != 'u') return fail(error,"unpaired high surrogate");
            position_ += 2; uint32_t low = 0; if (!unicodeUnit(low,error)) return false;
            if (low < 0xdc00 || low > 0xdfff) return fail(error,"invalid low surrogate");
            codepoint = 0x10000+((codepoint-0xd800)<<10)+(low-0xdc00);
          } else if (codepoint >= 0xdc00 && codepoint <= 0xdfff) return fail(error,"unpaired low surrogate");
          appendUtf8(result, codepoint);
          break;
        }
        default: return fail(error, "invalid escape");
      }
    }
    return fail(error, "unterminated string");
  }

  bool unicodeUnit(uint32_t& value,std::string& error) {
    value = 0;
    for (int i = 0; i < 4; ++i) {
      if (position_ == source_.size()) return fail(error,"short unicode escape");
      const char digit = source_[position_++]; value <<= 4;
      if (digit >= '0' && digit <= '9') value += digit-'0';
      else if (digit >= 'a' && digit <= 'f') value += digit-'a'+10;
      else if (digit >= 'A' && digit <= 'F') value += digit-'A'+10;
      else return fail(error,"invalid unicode escape");
    }
    return true;
  }
  static void appendUtf8(std::string& output, uint32_t codepoint) {
    if (codepoint <= 0x7F) output.push_back(static_cast<char>(codepoint));
    else if (codepoint <= 0x7FF) {
      output.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
      output.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else if (codepoint <= 0xFFFF) {
      output.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
      output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
      output.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else {
      output.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
      output.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
      output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
      output.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    }
  }

  void skipWhitespace() {
    while (position_ < source_.size() && isspace(static_cast<unsigned char>(source_[position_]))) ++position_;
  }

  bool take(char expected) {
    if (position_ < source_.size() && source_[position_] == expected) {
      ++position_;
      return true;
    }
    return false;
  }

  bool fail(std::string& error, const char* message) const {
    std::ostringstream output;
    output << message << " at byte " << position_;
    error = output.str();
    return false;
  }

  const std::string& source_;
  size_t position_ = 0;
};

}  // namespace

bool parse(const std::string& source, Value& result, std::string& error) {
  return Parser(source).parse(result, error);
}

const Value* field(const Value& value, const char* key) {
  if (value.type != Value::Object) return nullptr;
  const std::map<std::string, Value>::const_iterator found = value.object.find(key);
  return found == value.object.end() ? nullptr : &found->second;
}

std::string stringField(const Value& value, const char* key, const std::string& fallback) {
  const Value* item = field(value, key);
  return item && item->type == Value::String ? item->string : fallback;
}

int numberField(const Value& value, const char* key, int fallback) {
  const double number = realField(value, key, static_cast<double>(fallback));
  if (number < static_cast<double>(std::numeric_limits<int>::min())) return std::numeric_limits<int>::min();
  if (number > static_cast<double>(std::numeric_limits<int>::max())) return std::numeric_limits<int>::max();
  return static_cast<int>(number);
}

double realField(const Value& value, const char* key, double fallback) {
  const Value* item = field(value, key);
  return item && item->type == Value::Number ? item->number : fallback;
}

bool boolField(const Value& value, const char* key, bool fallback) {
  const Value* item = field(value, key);
  return item && item->type == Value::Boolean ? item->boolean : fallback;
}

void appendString(std::string& output, const std::string& value) {
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

}  // namespace json
}  // namespace madstudio
