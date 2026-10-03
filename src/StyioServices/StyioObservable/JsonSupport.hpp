#pragma once
#ifndef STYIO_OBSERVABLE_JSON_SUPPORT_HPP_
#define STYIO_OBSERVABLE_JSON_SUPPORT_HPP_

#include <cctype>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace styio::observable {
namespace json_detail {

struct CompactJson
{
  std::string buf;
  std::vector<char> need_comma;

  void comma() {
    if (!need_comma.empty() && need_comma.back() != 0) {
      buf.push_back(',');
    }
    if (!need_comma.empty()) {
      need_comma.back() = 1;
    }
  }

  void begin_object() {
    comma();
    buf.push_back('{');
    need_comma.push_back(0);
  }

  void end_object() {
    buf.push_back('}');
    need_comma.pop_back();
  }

  void begin_array() {
    comma();
    buf.push_back('[');
    need_comma.push_back(0);
  }

  void end_array() {
    buf.push_back(']');
    need_comma.pop_back();
  }

  void key(std::string_view name) {
    comma();
    emit_string(name);
    buf.push_back(':');
    if (!need_comma.empty()) {
      need_comma.back() = 0;
    }
  }

  void emit_string(std::string_view value) {
    buf.push_back('"');
    for (unsigned char ch : value) {
      switch (ch) {
        case '"': buf += "\\\""; break;
        case '\\': buf += "\\\\"; break;
        case '\b': buf += "\\b"; break;
        case '\f': buf += "\\f"; break;
        case '\n': buf += "\\n"; break;
        case '\r': buf += "\\r"; break;
        case '\t': buf += "\\t"; break;
        default:
          if (ch < 0x20) {
            static constexpr char kHex[] = "0123456789abcdef";
            buf += "\\u00";
            buf.push_back(kHex[ch >> 4]);
            buf.push_back(kHex[ch & 0x0fu]);
          } else {
            buf.push_back(static_cast<char>(ch));
          }
          break;
      }
    }
    buf.push_back('"');
  }

  void string_value(std::string_view value) {
    comma();
    emit_string(value);
  }

  void null_value() {
    comma();
    buf += "null";
  }

  void bool_value(bool value) {
    comma();
    buf += value ? "true" : "false";
  }

  void integer_value(long long value) {
    comma();
    buf += std::to_string(value);
  }

  void raw_value(std::string_view json) {
    comma();
    buf.append(json.data(), json.size());
  }
};

inline std::string quote_string(std::string_view value) {
  CompactJson json;
  json.emit_string(value);
  return json.buf;
}

struct JsonValue
{
  enum class Kind { Null, Bool, Int, String, Array, Object };

  Kind kind = Kind::Null;
  bool bool_value = false;
  long long int_value = 0;
  std::string string_value;
  std::vector<JsonValue> array_value;
  // unique_ptr keeps the pair complete while JsonValue is still being defined.
  // GCC 14 libstdc++ rejects vector<pair<string, JsonValue>> as incomplete.
  std::vector<std::pair<std::string, std::unique_ptr<JsonValue>>> object_value;

  JsonValue() = default;
  JsonValue(const JsonValue& other);
  JsonValue(JsonValue&&) noexcept = default;
  JsonValue& operator=(const JsonValue& other);
  JsonValue& operator=(JsonValue&&) noexcept = default;
  ~JsonValue();

  bool is_null() const { return kind == Kind::Null; }
  bool is_object() const { return kind == Kind::Object; }
  bool is_array() const { return kind == Kind::Array; }
  bool is_string() const { return kind == Kind::String; }

  const JsonValue* field(std::string_view name) const {
    if (kind != Kind::Object) {
      return nullptr;
    }
    for (const auto& entry : object_value) {
      if (entry.second && entry.first == name) {
        return entry.second.get();
      }
    }
    return nullptr;
  }

  const std::string* as_string() const {
    return kind == Kind::String ? &string_value : nullptr;
  }

  const std::vector<JsonValue>* as_array() const {
    return kind == Kind::Array ? &array_value : nullptr;
  }
};

inline JsonValue::~JsonValue() = default;

inline JsonValue::JsonValue(const JsonValue& other)
  : kind(other.kind),
    bool_value(other.bool_value),
    int_value(other.int_value),
    string_value(other.string_value),
    array_value(other.array_value)
{
  object_value.reserve(other.object_value.size());
  for (const auto& entry : other.object_value) {
    object_value.emplace_back(
      entry.first,
      entry.second ? std::make_unique<JsonValue>(*entry.second) : std::unique_ptr<JsonValue>{}
    );
  }
}

inline JsonValue& JsonValue::operator=(const JsonValue& other) {
  if (this != &other) {
    JsonValue copy(other);
    *this = std::move(copy);
  }
  return *this;
}

class JsonParser
{
public:
  explicit JsonParser(std::string_view text) : text_(text) {}

  JsonValue parse_value() {
    skip_ws();
    JsonValue value = parse_value_raw();
    skip_ws();
    return value;
  }

  bool at_end() {
    skip_ws();
    return pos_ >= text_.size();
  }

  std::string error() const { return error_; }

private:
  std::string_view text_;
  std::size_t pos_ = 0;
  std::string error_;

  [[noreturn]] void fail(const char* message) {
    error_ = message;
    throw std::runtime_error(message);
  }

  void skip_ws() {
    while (pos_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[pos_]))) {
      ++pos_;
    }
  }

  char peek() {
    if (pos_ >= text_.size()) {
      fail("unexpected end of JSON");
    }
    return text_[pos_];
  }

  char next() {
    char ch = peek();
    ++pos_;
    return ch;
  }

  bool consume(char expected) {
    skip_ws();
    if (pos_ < text_.size() && text_[pos_] == expected) {
      ++pos_;
      return true;
    }
    return false;
  }

  JsonValue parse_value_raw() {
    skip_ws();
    const char ch = peek();
    if (ch == '{') {
      return parse_object();
    }
    if (ch == '[') {
      return parse_array();
    }
    if (ch == '"') {
      JsonValue value;
      value.kind = JsonValue::Kind::String;
      value.string_value = parse_string();
      return value;
    }
    if (ch == 'n') {
      expect_literal("null");
      return JsonValue{};
    }
    if (ch == 't') {
      expect_literal("true");
      JsonValue value;
      value.kind = JsonValue::Kind::Bool;
      value.bool_value = true;
      return value;
    }
    if (ch == 'f') {
      expect_literal("false");
      JsonValue value;
      value.kind = JsonValue::Kind::Bool;
      value.bool_value = false;
      return value;
    }
    if (ch == '-' || (ch >= '0' && ch <= '9')) {
      return parse_number();
    }
    fail("unexpected JSON token");
  }

  JsonValue parse_object() {
    next();
    JsonValue value;
    value.kind = JsonValue::Kind::Object;
    skip_ws();
    if (consume('}')) {
      return value;
    }
    while (true) {
      skip_ws();
      if (peek() != '"') {
        fail("object key must be a string");
      }
      std::string key = parse_string();
      skip_ws();
      if (next() != ':') {
        fail("expected ':' after object key");
      }
      value.object_value.emplace_back(
        std::move(key),
        std::make_unique<JsonValue>(parse_value_raw())
      );
      skip_ws();
      if (consume('}')) {
        break;
      }
      if (next() != ',') {
        fail("expected ',' or '}' in object");
      }
    }
    return value;
  }

  JsonValue parse_array() {
    next();
    JsonValue value;
    value.kind = JsonValue::Kind::Array;
    skip_ws();
    if (consume(']')) {
      return value;
    }
    while (true) {
      value.array_value.push_back(parse_value_raw());
      skip_ws();
      if (consume(']')) {
        break;
      }
      if (next() != ',') {
        fail("expected ',' or ']' in array");
      }
    }
    return value;
  }

  JsonValue parse_number() {
    const std::size_t begin = pos_;
    if (peek() == '-') {
      next();
    }
    if (pos_ >= text_.size() || !std::isdigit(static_cast<unsigned char>(peek()))) {
      fail("invalid number");
    }
    if (peek() == '0') {
      next();
    } else {
      while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) {
        ++pos_;
      }
    }
    if (pos_ < text_.size() && (text_[pos_] == '.' || text_[pos_] == 'e' || text_[pos_] == 'E')) {
      fail("non-integer JSON numbers are not used by this contract");
    }
    JsonValue value;
    value.kind = JsonValue::Kind::Int;
    value.int_value = std::stoll(std::string(text_.substr(begin, pos_ - begin)));
    return value;
  }

  std::string parse_string() {
    if (next() != '"') {
      fail("expected string");
    }
    std::string out;
    while (true) {
      if (pos_ >= text_.size()) {
        fail("unterminated string");
      }
      const char ch = next();
      if (ch == '"') {
        return out;
      }
      if (ch != '\\') {
        out.push_back(ch);
        continue;
      }
      if (pos_ >= text_.size()) {
        fail("unterminated escape");
      }
      const char esc = next();
      switch (esc) {
        case '"':
        case '\\':
        case '/':
          out.push_back(esc);
          break;
        case 'b': out.push_back('\b'); break;
        case 'f': out.push_back('\f'); break;
        case 'n': out.push_back('\n'); break;
        case 'r': out.push_back('\r'); break;
        case 't': out.push_back('\t'); break;
        case 'u': {
          auto hex4 = [&]() {
            unsigned code = 0;
            for (int i = 0; i < 4; ++i) {
              const char hex = next();
              code <<= 4;
              if (hex >= '0' && hex <= '9') {
                code += static_cast<unsigned>(hex - '0');
              } else if (hex >= 'a' && hex <= 'f') {
                code += static_cast<unsigned>(hex - 'a' + 10);
              } else if (hex >= 'A' && hex <= 'F') {
                code += static_cast<unsigned>(hex - 'A' + 10);
              } else {
                fail("invalid unicode escape");
              }
            }
            return code;
          };
          unsigned code = hex4();
          if (code >= 0xd800 && code <= 0xdbff) {
            if (next() != '\\' || next() != 'u') {
              fail("unpaired high surrogate");
            }
            const unsigned low = hex4();
            if (low < 0xdc00 || low > 0xdfff) {
              fail("invalid low surrogate");
            }
            code = 0x10000 + ((code - 0xd800) << 10) + (low - 0xdc00);
          } else if (code >= 0xdc00 && code <= 0xdfff) {
            fail("unpaired low surrogate");
          }
          if (code < 0x80) {
            out.push_back(static_cast<char>(code));
          } else if (code < 0x800) {
            out.push_back(static_cast<char>(0xc0 | (code >> 6)));
            out.push_back(static_cast<char>(0x80 | (code & 0x3f)));
          } else if (code < 0x10000) {
            out.push_back(static_cast<char>(0xe0 | (code >> 12)));
            out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3f)));
            out.push_back(static_cast<char>(0x80 | (code & 0x3f)));
          } else {
            out.push_back(static_cast<char>(0xf0 | (code >> 18)));
            out.push_back(static_cast<char>(0x80 | ((code >> 12) & 0x3f)));
            out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3f)));
            out.push_back(static_cast<char>(0x80 | (code & 0x3f)));
          }
          break;
        }
        default:
          fail("invalid escape");
      }
    }
  }

  void expect_literal(const char* literal) {
    for (const char* p = literal; *p != '\0'; ++p) {
      if (next() != *p) {
        fail("invalid literal");
      }
    }
  }
};

inline bool parse_json(std::string_view text, JsonValue& out, std::string& error) {
  try {
    JsonParser parser(text);
    out = parser.parse_value();
    if (!parser.at_end()) {
      error = "trailing JSON content";
      return false;
    }
    return true;
  } catch (const std::exception& ex) {
    error = ex.what();
    return false;
  }
}

inline std::string require_string(const JsonValue& object, std::string_view key, std::string& error) {
  const JsonValue* field = object.field(key);
  if (field == nullptr || field->kind != JsonValue::Kind::String) {
    error = std::string("missing string field: ") + std::string(key);
    return {};
  }
  return field->string_value;
}

inline std::vector<std::string> require_string_array(
  const JsonValue& object,
  std::string_view key,
  std::string& error
) {
  const JsonValue* field = object.field(key);
  if (field == nullptr || field->kind != JsonValue::Kind::Array) {
    error = std::string("missing array field: ") + std::string(key);
    return {};
  }
  std::vector<std::string> values;
  values.reserve(field->array_value.size());
  for (const auto& item : field->array_value) {
    if (item.kind != JsonValue::Kind::String) {
      error = std::string("array field is not all strings: ") + std::string(key);
      return {};
    }
    values.push_back(item.string_value);
  }
  return values;
}

inline std::string sha256_hex16(std::string_view message) {
  static constexpr std::uint32_t kK[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4,
    0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe,
    0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f,
    0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc,
    0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
    0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116,
    0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7,
    0xc67178f2
  };

  auto rotr = [](std::uint32_t value, std::uint32_t bits) {
    return (value >> bits) | (value << (32 - bits));
  };

  std::uint32_t state[8] = {
    0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
    0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
  };

  std::vector<unsigned char> data(message.begin(), message.end());
  const std::uint64_t bit_len = static_cast<std::uint64_t>(data.size()) * 8ull;
  data.push_back(0x80);
  while ((data.size() % 64) != 56) {
    data.push_back(0);
  }
  for (int shift = 56; shift >= 0; shift -= 8) {
    data.push_back(static_cast<unsigned char>((bit_len >> shift) & 0xffu));
  }

  for (std::size_t offset = 0; offset < data.size(); offset += 64) {
    std::uint32_t w[64];
    for (int i = 0; i < 16; ++i) {
      w[i] = (static_cast<std::uint32_t>(data[offset + i * 4]) << 24)
        | (static_cast<std::uint32_t>(data[offset + i * 4 + 1]) << 16)
        | (static_cast<std::uint32_t>(data[offset + i * 4 + 2]) << 8)
        | static_cast<std::uint32_t>(data[offset + i * 4 + 3]);
    }
    for (int i = 16; i < 64; ++i) {
      const std::uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
      const std::uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
      w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    std::uint32_t a = state[0];
    std::uint32_t b = state[1];
    std::uint32_t c = state[2];
    std::uint32_t d = state[3];
    std::uint32_t e = state[4];
    std::uint32_t f = state[5];
    std::uint32_t g = state[6];
    std::uint32_t h = state[7];
    for (int i = 0; i < 64; ++i) {
      const std::uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
      const std::uint32_t ch = (e & f) ^ ((~e) & g);
      const std::uint32_t temp1 = h + S1 + ch + kK[i] + w[i];
      const std::uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
      const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
      const std::uint32_t temp2 = S0 + maj;
      h = g;
      g = f;
      f = e;
      e = d + temp1;
      d = c;
      c = b;
      b = a;
      a = temp1 + temp2;
    }
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
  }

  static constexpr char kHex[] = "0123456789abcdef";
  std::string hex;
  hex.resize(32);
  for (int i = 0; i < 4; ++i) {
    const std::uint32_t word = state[i];
    for (int byte = 0; byte < 4; ++byte) {
      const unsigned value = (word >> (24 - byte * 8)) & 0xffu;
      hex[static_cast<std::size_t>(i * 8 + byte * 2)] = kHex[value >> 4];
      hex[static_cast<std::size_t>(i * 8 + byte * 2 + 1)] = kHex[value & 0x0fu];
    }
  }
  return hex;
}

} // namespace json_detail
} // namespace styio::observable

#endif
