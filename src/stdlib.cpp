#include "bloa/stdlib.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <thread>
#include <unordered_set>
#ifdef BLOA_USE_CURL
#include <curl/curl.h>
#endif
#ifdef BLOA_USE_MYSQL
#include <mysql/mysql.h>
#endif
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <random>
#ifdef BLOA_USE_SQLITE
#include <sqlite3.h>
#endif
#include <sstream>
#include <regex>
#include <iomanip>

namespace fs = std::filesystem;

namespace bloa {

#ifdef BLOA_USE_MYSQL
static std::unordered_map<int, MYSQL *> mysql_connections;
static int next_mysql_connection_id = 1;
#endif

static Value value_as_number(const Value &v) {
  if (std::holds_alternative<int64_t>(v.v))
    return Value::make_double(static_cast<double>(std::get<int64_t>(v.v)));
  if (std::holds_alternative<double>(v.v)) return v;
  throw std::runtime_error("Value is not numeric");
}

static std::string value_to_string(const Value &v) {
  if (std::holds_alternative<std::monostate>(v.v)) return "None";
  if (std::holds_alternative<int64_t>(v.v))
    return std::to_string(std::get<int64_t>(v.v));
  if (std::holds_alternative<double>(v.v)) {
    double d = std::get<double>(v.v);
    if (std::floor(d) == d) {
      return std::to_string(static_cast<int64_t>(d));
    }
    return std::to_string(d);
  }
  if (std::holds_alternative<std::string>(v.v))
    return std::get<std::string>(v.v);
  if (std::holds_alternative<bool>(v.v))
    return std::get<bool>(v.v) ? "true" : "false";
  if (std::holds_alternative<std::vector<Value>>(v.v)) {
    std::string out = "[";
    const auto &list = std::get<std::vector<Value>>(v.v);
    for (size_t i = 0; i < list.size(); ++i) {
      if (i > 0) out += ", ";
      out += value_to_string(list[i]);
    }
    out += "]";
    return out;
  }
  return "<unknown>";
}

static const std::vector<Value> &as_list(const Value &v) {
  return std::get<std::vector<Value>>(v.v);
}

static Value copy_value(const Value &v);
static void skip_json_ws(const std::string &s, size_t &pos);
static std::string parse_json_string(const std::string &s, size_t &pos);
static Value parse_json_value(const std::string &s, size_t &pos);
static std::string json_stringify_value(const Value &v);
static std::string base64_encode(const std::string &data);
static std::string base64_decode(const std::string &data);
static std::string uuid4();
static bool glob_match(const std::string &pattern, const std::string &text);
static std::vector<Value> copy_list(const std::vector<Value> &list) {
  std::vector<Value> result;
  result.reserve(list.size());
  for (const auto &item : list) {
    result.push_back(copy_value(item));
  }
  return result;
}

static Value copy_value(const Value &v) {
  if (std::holds_alternative<std::vector<Value>>(v.v)) {
    return Value::make_list(copy_list(std::get<std::vector<Value>>(v.v)));
  }
  if (std::holds_alternative<std::shared_ptr<ObjectInstance>>(v.v)) {
    const auto &obj = std::get<std::shared_ptr<ObjectInstance>>(v.v);
    auto props = std::make_shared<Environment>(obj->properties->parent);
    for (const auto &name : obj->properties->local_keys()) {
      auto value_opt = obj->properties->get_local(name);
      if (value_opt) props->set_local(name, *value_opt);
    }
    return Value::make_object(obj->class_name, std::move(props));
  }
  if (std::holds_alternative<std::shared_ptr<Reference>>(v.v)) {
    const auto &ref = std::get<std::shared_ptr<Reference>>(v.v);
    return Value::make_ref(ref->env, ref->name);
  }
  return v;
}

#ifdef BLOA_USE_CURL
static size_t curl_write_callback(void *contents, size_t size, size_t nmemb,
                                  void *userp) {
  size_t total = size * nmemb;
  std::string *buffer = static_cast<std::string *>(userp);
  buffer->append(static_cast<char *>(contents), total);
  return total;
}
#endif

#ifdef BLOA_USE_SQLITE
static void sqlite_throw_if_error(int result, sqlite3 *db) {
  if (result != SQLITE_OK) {
    std::string err = sqlite3_errmsg(db);
    sqlite3_close(db);
    throw std::runtime_error("SQLite error: " + err);
  }
}
#endif

static const std::string archive_magic = "BLOAARCHIVE\n";

static void skip_json_ws(const std::string &s, size_t &pos) {
  while (pos < s.size() && std::isspace(static_cast<unsigned char>(s[pos]))) {
    ++pos;
  }
}

static std::string parse_json_string(const std::string &s, size_t &pos) {
  if (pos >= s.size() || s[pos] != '"')
    throw std::runtime_error("Invalid JSON string");
  ++pos;
  std::string result;
  while (pos < s.size()) {
    char c = s[pos++];
    if (c == '"') return result;
    if (c == '\\') {
      if (pos >= s.size()) throw std::runtime_error("Invalid JSON escape");
      char esc = s[pos++];
      switch (esc) {
        case '"': result.push_back('"'); break;
        case '\\': result.push_back('\\'); break;
        case '/': result.push_back('/'); break;
        case 'b': result.push_back('\b'); break;
        case 'f': result.push_back('\f'); break;
        case 'n': result.push_back('\n'); break;
        case 'r': result.push_back('\r'); break;
        case 't': result.push_back('\t'); break;
        default:
          throw std::runtime_error("Unsupported JSON escape sequence");
      }
    } else {
      result.push_back(c);
    }
  }
  throw std::runtime_error("Unterminated JSON string");
}

static Value parse_json_value(const std::string &s, size_t &pos) {
  skip_json_ws(s, pos);
  if (pos >= s.size()) throw std::runtime_error("Unexpected end of JSON");
  char c = s[pos];
  if (c == '"') {
    return Value::make_str(parse_json_string(s, pos));
  }
  if (c == '{') {
    ++pos;
    auto obj = std::make_shared<Environment>(nullptr);
    skip_json_ws(s, pos);
    if (pos < s.size() && s[pos] == '}') {
      ++pos;
      return Value::make_object("json", obj);
    }
    while (true) {
      skip_json_ws(s, pos);
      std::string key = parse_json_string(s, pos);
      skip_json_ws(s, pos);
      if (pos >= s.size() || s[pos] != ':')
        throw std::runtime_error("Invalid JSON object separator");
      ++pos;
      Value value = parse_json_value(s, pos);
      obj->set_local(key, value);
      skip_json_ws(s, pos);
      if (pos >= s.size()) throw std::runtime_error("Invalid JSON object");
      if (s[pos] == '}') {
        ++pos;
        break;
      }
      if (s[pos] != ',') throw std::runtime_error("Invalid JSON object delimiter");
      ++pos;
    }
    return Value::make_object("json", obj);
  }
  if (c == '[') {
    ++pos;
    std::vector<Value> list;
    skip_json_ws(s, pos);
    if (pos < s.size() && s[pos] == ']') {
      ++pos;
      return Value::make_list(std::move(list));
    }
    while (true) {
      list.push_back(parse_json_value(s, pos));
      skip_json_ws(s, pos);
      if (pos >= s.size()) throw std::runtime_error("Invalid JSON array");
      if (s[pos] == ']') {
        ++pos;
        break;
      }
      if (s[pos] != ',') throw std::runtime_error("Invalid JSON array delimiter");
      ++pos;
    }
    return Value::make_list(std::move(list));
  }
  if (s.compare(pos, 4, "true") == 0) {
    pos += 4;
    return Value::make_bool(true);
  }
  if (s.compare(pos, 5, "false") == 0) {
    pos += 5;
    return Value::make_bool(false);
  }
  if (s.compare(pos, 4, "null") == 0) {
    pos += 4;
    return Value();
  }
  size_t start = pos;
  if (s[pos] == '-') ++pos;
  bool is_float = false;
  while (pos < s.size() && std::isdigit(static_cast<unsigned char>(s[pos]))) ++pos;
  if (pos < s.size() && s[pos] == '.') {
    is_float = true;
    ++pos;
    while (pos < s.size() && std::isdigit(static_cast<unsigned char>(s[pos]))) ++pos;
  }
  if (pos < s.size() && (s[pos] == 'e' || s[pos] == 'E')) {
    is_float = true;
    ++pos;
    if (pos < s.size() && (s[pos] == '+' || s[pos] == '-')) ++pos;
    while (pos < s.size() && std::isdigit(static_cast<unsigned char>(s[pos]))) ++pos;
  }
  if (start == pos) throw std::runtime_error("Invalid JSON value");
  std::string token = s.substr(start, pos - start);
  try {
    if (is_float) {
      return Value::make_double(std::stod(token));
    }
    return Value::make_int(std::stoll(token));
  } catch (...) {
    throw std::runtime_error("Invalid JSON number: " + token);
  }
}

static std::string json_escape_string(const std::string &value) {
  std::string escaped;
  for (char c : value) {
    switch (c) {
      case '"': escaped += "\\\""; break;
      case '\\': escaped += "\\\\"; break;
      case '\b': escaped += "\\b"; break;
      case '\f': escaped += "\\f"; break;
      case '\n': escaped += "\\n"; break;
      case '\r': escaped += "\\r"; break;
      case '\t': escaped += "\\t"; break;
      default:
        if (static_cast<unsigned char>(c) < 0x20) {
          char buf[8];
          std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
          escaped += buf;
        } else {
          escaped.push_back(c);
        }
    }
  }
  return escaped;
}

static std::string json_stringify_value(const Value &v) {
  if (std::holds_alternative<std::monostate>(v.v)) return "null";
  if (std::holds_alternative<bool>(v.v)) return std::get<bool>(v.v) ? "true" : "false";
  if (std::holds_alternative<int64_t>(v.v)) return std::to_string(std::get<int64_t>(v.v));
  if (std::holds_alternative<double>(v.v)) {
    std::ostringstream oss;
    oss << std::setprecision(15) << std::get<double>(v.v);
    std::string out = oss.str();
    if (out.find('.') == std::string::npos && out.find('e') == std::string::npos && out.find('E') == std::string::npos) {
      out += ".0";
    }
    return out;
  }
  if (std::holds_alternative<std::string>(v.v)) {
    return std::string("\"") + json_escape_string(std::get<std::string>(v.v)) + "\"";
  }
  if (std::holds_alternative<std::vector<Value>>(v.v)) {
    const auto &list = std::get<std::vector<Value>>(v.v);
    std::string out = "[";
    for (size_t i = 0; i < list.size(); ++i) {
      if (i) out += ",";
      out += json_stringify_value(list[i]);
    }
    out += "]";
    return out;
  }
  if (std::holds_alternative<std::shared_ptr<ObjectInstance>>(v.v)) {
    auto obj = std::get<std::shared_ptr<ObjectInstance>>(v.v);
    std::string out = "{";
    const auto keys = obj->properties->local_keys();
    for (size_t i = 0; i < keys.size(); ++i) {
      if (i) out += ",";
      out += json_stringify_value(Value::make_str(keys[i]));
      out += ":";
      auto val = obj->properties->get_local(keys[i]);
      out += json_stringify_value(val ? *val : Value());
    }
    out += "}";
    return out;
  }
  return "null";
}

static const std::string base64_chars =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

static std::string base64_encode(const std::string &data) {
  std::string encoded;
  int val = 0;
  int valb = -6;
  for (unsigned char c : data) {
    val = (val << 8) + c;
    valb += 8;
    while (valb >= 0) {
      encoded.push_back(base64_chars[(val >> valb) & 0x3F]);
      valb -= 6;
    }
  }
  if (valb > -6) encoded.push_back(base64_chars[((val << 8) >> (valb + 8)) & 0x3F]);
  while (encoded.size() % 4) encoded.push_back('=');
  return encoded;
}

static std::string base64_decode(const std::string &data) {
  std::vector<int> T(256, -1);
  for (int i = 0; i < 64; i++) T[static_cast<unsigned char>(base64_chars[i])] = i;
  std::string decoded;
  int val = 0;
  int valb = -8;
  for (unsigned char c : data) {
    if (T[c] == -1) {
      if (c == '=') break;
      continue;
    }
    val = (val << 6) + T[c];
    valb += 6;
    if (valb >= 0) {
      decoded.push_back(char((val >> valb) & 0xFF));
      valb -= 8;
    }
  }
  return decoded;
}

static std::string uuid4() {
  static std::random_device rd;
  static std::mt19937_64 gen(rd());
  std::uniform_int_distribution<uint64_t> dist(0, 0xFFFFFFFFFFFFFFFFULL);
  uint64_t a = dist(gen);
  uint64_t b = dist(gen);
  a = (a & 0xFFFFFFFFFFFF0FFFULL) | 0x0000000000004000ULL;
  b = (b & 0x3FFFFFFFFFFFFFFFULL) | 0x8000000000000000ULL;
  std::ostringstream oss;
  oss << std::hex << std::setfill('0');
  oss << std::setw(8) << static_cast<uint32_t>(a >> 32) << "-";
  oss << std::setw(4) << static_cast<uint16_t>((a >> 16) & 0xFFFF) << "-";
  oss << std::setw(4) << static_cast<uint16_t>(a & 0xFFFF) << "-";
  oss << std::setw(4) << static_cast<uint16_t>(b >> 48) << "-";
  oss << std::setw(12) << (b & 0xFFFFFFFFFFFFULL);
  return oss.str();
}

static bool glob_match(const std::string &pattern, const std::string &text) {
  std::string regex_pattern;
  regex_pattern.reserve(pattern.size() * 2 + 4);
  regex_pattern.push_back('^');
  for (char c : pattern) {
    switch (c) {
      case '*': regex_pattern += ".*"; break;
      case '?': regex_pattern += '.'; break;
      case '.': regex_pattern += "\\."; break;
      case '\\': regex_pattern += "\\\\"; break;
      case '+': case '(': case ')': case '[': case ']': case '{': case '}': case '^': case '$': case '|': regex_pattern.push_back('\\'); regex_pattern.push_back(c); break;
      default: regex_pattern.push_back(c); break;
    }
  }
  regex_pattern.push_back('$');
  return std::regex_match(text, std::regex(regex_pattern));
}

static std::vector<Value> parse_csv_text(const std::string &text, char delim) {
  std::vector<Value> rows;
  std::vector<Value> current_row;
  std::string field;
  bool in_quotes = false;
  for (size_t i = 0; i <= text.size(); ++i) {
    char c = i < text.size() ? text[i] : '\n';
    if (in_quotes) {
      if (c == '"') {
        if (i + 1 < text.size() && text[i + 1] == '"') {
          field.push_back('"');
          ++i;
        } else {
          in_quotes = false;
        }
      } else {
        field.push_back(c);
      }
    } else {
      if (c == '"') {
        in_quotes = true;
      } else if (c == delim) {
        current_row.push_back(Value::make_str(field));
        field.clear();
      } else if (c == '\r') {
        continue;
      } else if (c == '\n') {
        current_row.push_back(Value::make_str(field));
        if (!current_row.empty() || !field.empty()) {
          if (!(current_row.size() == 1 && field.empty())) {
            rows.push_back(Value::make_list(std::move(current_row)));
          }
        }
        current_row.clear();
        field.clear();
      } else {
        field.push_back(c);
      }
    }
  }
  return rows;
}

static std::string csv_escape_field(const std::string &field, char delim) {
  bool needs_quotes = field.find(delim) != std::string::npos || field.find('"') != std::string::npos || field.find('\n') != std::string::npos || field.find('\r') != std::string::npos;
  std::string out = field;
  if (needs_quotes) {
    std::string escaped;
    for (char c : out) {
      if (c == '"') escaped += "\"\"";
      else escaped.push_back(c);
    }
    return std::string("\"") + escaped + "\"";
  }
  return out;
}

std::vector<std::pair<std::string, std::string>> read_archive(
    const std::string &path) {
  std::ifstream ifs(path, std::ios::binary);
  if (!ifs) throw std::runtime_error("Cannot open archive: " + path);
  std::string magic(archive_magic.size(), '\0');
  ifs.read(&magic[0], static_cast<std::streamsize>(magic.size()));
  if (magic != archive_magic) throw std::runtime_error("Invalid archive: " + path);
  uint64_t count = 0;
  ifs.read(reinterpret_cast<char *>(&count), sizeof(count));
  std::vector<std::pair<std::string, std::string>> entries;
  for (uint64_t i = 0; i < count; ++i) {
    uint64_t name_len = 0;
    uint64_t data_len = 0;
    ifs.read(reinterpret_cast<char *>(&name_len), sizeof(name_len));
    ifs.read(reinterpret_cast<char *>(&data_len), sizeof(data_len));
    std::string name(name_len, '\0');
    std::string data(data_len, '\0');
    ifs.read(&name[0], static_cast<std::streamsize>(name_len));
    ifs.read(&data[0], static_cast<std::streamsize>(data_len));
    entries.emplace_back(std::move(name), std::move(data));
  }
  return entries;
}

void write_archive(
    const std::string &path,
    const std::vector<std::pair<std::string, std::string>> &entries) {
  std::ofstream ofs(path, std::ios::binary);
  if (!ofs) throw std::runtime_error("Cannot create archive: " + path);
  ofs.write(archive_magic.data(), static_cast<std::streamsize>(archive_magic.size()));
  uint64_t count = entries.size();
  ofs.write(reinterpret_cast<const char *>(&count), sizeof(count));
  for (const auto &entry : entries) {
    uint64_t name_len = entry.first.size();
    uint64_t data_len = entry.second.size();
    ofs.write(reinterpret_cast<const char *>(&name_len), sizeof(name_len));
    ofs.write(reinterpret_cast<const char *>(&data_len), sizeof(data_len));
    ofs.write(entry.first.data(), static_cast<std::streamsize>(name_len));
    ofs.write(entry.second.data(), static_cast<std::streamsize>(data_len));
  }
}

void register_stdlib(std::shared_ptr<Environment> env) {
  // Core functions
  env->set("print", Value::make_str("__builtin_print"));
  env->set("range", Value::make_str("__builtin_range"));
  env->set("len", Value::make_str("__builtin_len"));
  env->set("str", Value::make_str("__builtin_str"));
  env->set("int", Value::make_str("__builtin_int"));
  env->set("float", Value::make_str("__builtin_float"));
  env->set("append", Value::make_str("__builtin_append"));

  // Math functions
  env->set("sqrt", Value::make_str("__builtin_sqrt"));
  env->set("pow", Value::make_str("__builtin_pow"));
  env->set("sin", Value::make_str("__builtin_sin"));
  env->set("cos", Value::make_str("__builtin_cos"));
  env->set("tan", Value::make_str("__builtin_tan"));
  env->set("log", Value::make_str("__builtin_log"));
  env->set("exp", Value::make_str("__builtin_exp"));
  env->set("abs", Value::make_str("__builtin_abs"));
  env->set("floor", Value::make_str("__builtin_floor"));
  env->set("ceil", Value::make_str("__builtin_ceil"));
  env->set("round", Value::make_str("__builtin_round"));
  env->set("pi", Value::make_str("__builtin_pi"));
  env->set("e", Value::make_str("__builtin_e"));

  // I/O functions
  env->set("read_file", Value::make_str("__builtin_read_file"));
  env->set("write_file", Value::make_str("__builtin_write_file"));
  env->set("exists", Value::make_str("__builtin_exists"));
  env->set("list_dir", Value::make_str("__builtin_list_dir"));
  env->set("mkdir", Value::make_str("__builtin_mkdir"));
  env->set("rmdir", Value::make_str("__builtin_rmdir"));
  env->set("remove", Value::make_str("__builtin_remove"));
  env->set("copy_file", Value::make_str("__builtin_copy_file"));
  env->set("move", Value::make_str("__builtin_move"));
  env->set("file_size", Value::make_str("__builtin_file_size"));
  env->set("is_dir", Value::make_str("__builtin_is_dir"));

  // String functions
  env->set("split", Value::make_str("__builtin_split"));
  env->set("join", Value::make_str("__builtin_join"));
  env->set("substr", Value::make_str("__builtin_substr"));
  env->set("find", Value::make_str("__builtin_find"));
  env->set("replace", Value::make_str("__builtin_replace"));
  env->set("to_upper", Value::make_str("__builtin_to_upper"));
  env->set("to_lower", Value::make_str("__builtin_to_lower"));
  env->set("trim", Value::make_str("__builtin_trim"));
  env->set("starts_with", Value::make_str("__builtin_starts_with"));
  env->set("ends_with", Value::make_str("__builtin_ends_with"));
  env->set("contains", Value::make_str("__builtin_contains"));
  env->set("reverse", Value::make_str("__builtin_reverse"));
  env->set("repeat", Value::make_str("__builtin_repeat"));

  // Utility functions
  env->set("random_int", Value::make_str("__builtin_random_int"));
  env->set("random_float", Value::make_str("__builtin_random_float"));
  env->set("now", Value::make_str("__builtin_now"));
  env->set("copy", Value::make_str("__builtin_copy"));
  env->set("clone", Value::make_str("__builtin_clone"));
  env->set("slice", Value::make_str("__builtin_slice"));
  env->set("sorted", Value::make_str("__builtin_sorted"));
  env->set("sum", Value::make_str("__builtin_sum"));
  env->set("min", Value::make_str("__builtin_min"));
  env->set("max", Value::make_str("__builtin_max"));
  env->set("type", Value::make_str("__builtin_type"));
  env->set("vars", Value::make_str("__builtin_vars"));
  env->set("keys", Value::make_str("__builtin_keys"));
  env->set("get", Value::make_str("__builtin_get"));
  env->set("set", Value::make_str("__builtin_set"));
  env->set("system", Value::make_str("__builtin_system"));
  env->set("shell", Value::make_str("__builtin_shell"));
  env->set("getenv", Value::make_str("__builtin_getenv"));
  env->set("setenv", Value::make_str("__builtin_setenv"));
  env->set("sleep", Value::make_str("__builtin_sleep"));
  env->set("pwd", Value::make_str("__builtin_pwd"));
  env->set("cwd", Value::make_str("__builtin_pwd"));
  env->set("path_join", Value::make_str("__builtin_path_join"));
  env->set("path_is_absolute", Value::make_str("__builtin_path_is_absolute"));
  env->set("path_normalize", Value::make_str("__builtin_path_normalize"));
  env->set("file_ext", Value::make_str("__builtin_file_ext"));
  env->set("basename", Value::make_str("__builtin_basename"));
  env->set("dirname", Value::make_str("__builtin_dirname"));
  env->set("mkdirs", Value::make_str("__builtin_mkdirs"));
  env->set("json_parse", Value::make_str("__builtin_json_parse"));
  env->set("json_stringify", Value::make_str("__builtin_json_stringify"));
  env->set("csv_parse", Value::make_str("__builtin_csv_parse"));
  env->set("csv_stringify", Value::make_str("__builtin_csv_stringify"));
  env->set("base64_encode", Value::make_str("__builtin_base64_encode"));
  env->set("base64_decode", Value::make_str("__builtin_base64_decode"));
  env->set("uuid4", Value::make_str("__builtin_uuid4"));
  env->set("glob", Value::make_str("__builtin_glob"));
  env->set("regex_match", Value::make_str("__builtin_regex_match"));
  env->set("regex_replace", Value::make_str("__builtin_regex_replace"));
#ifdef BLOA_USE_MYSQL
  env->set("mysql_connect", Value::make_str("__builtin_mysql_connect"));
  env->set("mysql_query", Value::make_str("__builtin_mysql_query"));
  env->set("mysql_exec", Value::make_str("__builtin_mysql_exec"));
  env->set("mysql_close", Value::make_str("__builtin_mysql_close"));
  env->set("mysql_escape", Value::make_str("__builtin_mysql_escape"));
#endif

  // PHP / Java-like helpers
  env->set("echo", Value::make_str("__builtin_echo"));
  env->set("isset", Value::make_str("__builtin_isset"));
  env->set("unset", Value::make_str("__builtin_unset"));
  env->set("ref", Value::make_str("__builtin_ref"));
  env->set("deref", Value::make_str("__builtin_deref"));
  env->set("set_ref", Value::make_str("__builtin_set_ref"));
  env->set("is_ref", Value::make_str("__builtin_is_ref"));
  env->set("baar_create", Value::make_str("__builtin_baar_create"));
  env->set("baar_extract", Value::make_str("__builtin_baar_extract"));
  env->set("baar_list", Value::make_str("__builtin_baar_list"));
  env->set("baar_read", Value::make_str("__builtin_baar_read"));

  // HTTP and network helpers
#ifdef BLOA_USE_CURL
  env->set("curl_get", Value::make_str("__builtin_curl_get"));
  env->set("curl_post", Value::make_str("__builtin_curl_post"));
  env->set("curl_request", Value::make_str("__builtin_curl_request"));
#endif

  // SQLite helpers
#ifdef BLOA_USE_SQLITE
  env->set("sqlite_query", Value::make_str("__builtin_sqlite_query"));
  env->set("sqlite_exec", Value::make_str("__builtin_sqlite_exec"));
#endif
}

Value handle_builtin(const std::string &marker,
                     const std::vector<Value> &args,
                     std::shared_ptr<Environment> env) {
  if (marker == "__builtin_print" || marker == "__builtin_echo") {
    for (size_t i = 0; i < args.size(); ++i) {
      if (i > 0) std::cout << ' ';
      std::cout << value_to_string(args[i]);
    }
    std::cout << '\n';
    return Value();
  }
  if (marker == "__builtin_isset") {
    if (!env) throw std::runtime_error("isset() requires environment access");
    if (args.size() == 1 && std::holds_alternative<std::string>(args[0].v)) {
      return Value::make_bool(env->has(std::get<std::string>(args[0].v)));
    }
    if (args.size() == 2 &&
        std::holds_alternative<std::shared_ptr<ObjectInstance>>(args[0].v) &&
        std::holds_alternative<std::string>(args[1].v)) {
      auto obj = std::get<std::shared_ptr<ObjectInstance>>(args[0].v);
      return Value::make_bool(obj->properties->has(std::get<std::string>(args[1].v)));
    }
    throw std::runtime_error("isset() requires 1 string argument or (object, member)");
  }
  if (marker == "__builtin_unset") {
    if (!env) throw std::runtime_error("unset() requires environment access");
    if (args.size() == 1 && std::holds_alternative<std::string>(args[0].v)) {
      return Value::make_bool(env->remove(std::get<std::string>(args[0].v)));
    }
    if (args.size() == 2 &&
        std::holds_alternative<std::shared_ptr<ObjectInstance>>(args[0].v) &&
        std::holds_alternative<std::string>(args[1].v)) {
      auto obj = std::get<std::shared_ptr<ObjectInstance>>(args[0].v);
      return Value::make_bool(obj->properties->remove(
          std::get<std::string>(args[1].v)));
    }
    throw std::runtime_error("unset() requires 1 string argument or (object, member)");
  }
  if (marker == "__builtin_ref") {
    if (!env || args.size() != 1 || !std::holds_alternative<std::string>(args[0].v))
      throw std::runtime_error("ref() requires 1 string argument");
    std::string name = std::get<std::string>(args[0].v);
    auto current = env;
    while (current && !current->has_local(name)) current = current->parent;
    if (!current) throw std::runtime_error("Undefined variable for ref(): " + name);
    return Value::make_ref(current, name);
  }
  if (marker == "__builtin_deref") {
    if (args.size() != 1 || !std::holds_alternative<std::shared_ptr<Reference>>(args[0].v))
      throw std::runtime_error("deref() requires 1 reference argument");
    auto ref = std::get<std::shared_ptr<Reference>>(args[0].v);
    auto target = ref->env->get(ref->name);
    if (!target) throw std::runtime_error("Invalid reference target: " + ref->name);
    return *target;
  }
  if (marker == "__builtin_set_ref") {
    if (args.size() != 2 ||
        !std::holds_alternative<std::shared_ptr<Reference>>(args[0].v))
      throw std::runtime_error("set_ref() requires 1 reference and 1 value");
    auto ref = std::get<std::shared_ptr<Reference>>(args[0].v);
    ref->env->set(ref->name, args[1]);
    return Value();
  }
  if (marker == "__builtin_is_ref") {
    if (args.size() != 1) throw std::runtime_error("is_ref() requires 1 argument");
    return Value::make_bool(std::holds_alternative<std::shared_ptr<Reference>>(args[0].v));
  }
  if (marker == "__builtin_range") {
    int64_t start = 0;
    int64_t stop = 0;
    int64_t step = 1;
    if (args.size() == 1) {
      stop = static_cast<int64_t>(value_as_number(args[0]).as_number());
    } else if (args.size() == 2) {
      start = static_cast<int64_t>(value_as_number(args[0]).as_number());
      stop = static_cast<int64_t>(value_as_number(args[1]).as_number());
    } else if (args.size() == 3) {
      start = static_cast<int64_t>(value_as_number(args[0]).as_number());
      stop = static_cast<int64_t>(value_as_number(args[1]).as_number());
      step = static_cast<int64_t>(value_as_number(args[2]).as_number());
    } else {
      throw std::runtime_error("range() requires 1 to 3 numeric arguments");
    }
    if (step == 0) throw std::runtime_error("range() step cannot be zero");
    std::vector<Value> list;
    if (step > 0) {
      for (int64_t i = start; i < stop; i += step) list.push_back(Value::make_int(i));
    } else {
      for (int64_t i = start; i > stop; i += step) list.push_back(Value::make_int(i));
    }
    return Value::make_list(list);
  }
  if (marker == "__builtin_len") {
    if (args.size() != 1) throw std::runtime_error("len() requires 1 argument");
    const auto &arg = args[0];
    if (std::holds_alternative<std::string>(arg.v)) {
      return Value::make_int(
          static_cast<int64_t>(std::get<std::string>(arg.v).size()));
    } else if (std::holds_alternative<std::vector<Value>>(arg.v)) {
      return Value::make_int(
          static_cast<int64_t>(std::get<std::vector<Value>>(arg.v).size()));
    } else if (std::holds_alternative<std::shared_ptr<ObjectInstance>>(arg.v)) {
      return Value::make_int(
          static_cast<int64_t>(std::get<std::shared_ptr<ObjectInstance>>(arg.v)->properties->local_keys().size()));
    } else {
      throw std::runtime_error("len() argument must be string, list, or object");
    }
  }
  if (marker == "__builtin_copy" || marker == "__builtin_clone") {
    if (args.size() != 1) throw std::runtime_error("copy() requires 1 argument");
    return copy_value(args[0]);
  }
  if (marker == "__builtin_slice") {
    if (args.size() < 2 || args.size() > 3)
      throw std::runtime_error("slice() requires 2 or 3 arguments");
    const auto &source = args[0];
    int64_t start = static_cast<int64_t>(value_as_number(args[1]).as_number());
    int64_t length = -1;
    if (args.size() == 3) length = static_cast<int64_t>(value_as_number(args[2]).as_number());
    if (std::holds_alternative<std::string>(source.v)) {
      std::string s = std::get<std::string>(source.v);
      if (start < 0) start = static_cast<int64_t>(s.size()) + start;
      if (start < 0) start = 0;
      if (length < 0) return Value::make_str(s.substr(static_cast<size_t>(start)));
      return Value::make_str(s.substr(static_cast<size_t>(start), static_cast<size_t>(length)));
    }
    if (std::holds_alternative<std::vector<Value>>(source.v)) {
      const auto &list = std::get<std::vector<Value>>(source.v);
      if (start < 0) start = static_cast<int64_t>(list.size()) + start;
      if (start < 0) start = 0;
      int64_t end = (length < 0) ? static_cast<int64_t>(list.size()) : start + length;
      if (end > static_cast<int64_t>(list.size())) end = static_cast<int64_t>(list.size());
      std::vector<Value> out;
      for (int64_t i = start; i < end; ++i) out.push_back(list[static_cast<size_t>(i)]);
      return Value::make_list(std::move(out));
    }
    throw std::runtime_error("slice() source must be string or list");
  }
  if (marker == "__builtin_sorted") {
    if (args.size() != 1)
      throw std::runtime_error("sorted() requires 1 list argument");
    if (!std::holds_alternative<std::vector<Value>>(args[0].v))
      throw std::runtime_error("sorted() requires a list");
    auto list = std::get<std::vector<Value>>(args[0].v);
    std::sort(list.begin(), list.end(), [](const Value &a, const Value &b) {
      if (std::holds_alternative<std::string>(a.v) &&
          std::holds_alternative<std::string>(b.v)) {
        return std::get<std::string>(a.v) < std::get<std::string>(b.v);
      }
      return value_as_number(a).as_number() < value_as_number(b).as_number();
    });
    return Value::make_list(std::move(list));
  }
  if (marker == "__builtin_sum" || marker == "__builtin_min" || marker == "__builtin_max") {
    if (args.size() != 1)
      throw std::runtime_error("sum/min/max() requires 1 list argument");
    if (!std::holds_alternative<std::vector<Value>>(args[0].v))
      throw std::runtime_error("sum/min/max() requires a list");
    const auto &list = std::get<std::vector<Value>>(args[0].v);
    if (list.empty()) throw std::runtime_error("List cannot be empty");
    double result = value_as_number(list[0]).as_number();
    if (marker == "__builtin_sum") {
      for (size_t i = 1; i < list.size(); ++i) result += value_as_number(list[i]).as_number();
      return Value::make_double(result);
    }
    for (size_t i = 1; i < list.size(); ++i) {
      double current = value_as_number(list[i]).as_number();
      if (marker == "__builtin_min") result = std::min(result, current);
      if (marker == "__builtin_max") result = std::max(result, current);
    }
    return Value::make_double(result);
  }
  if (marker == "__builtin_type") {
    if (args.size() != 1) throw std::runtime_error("type() requires 1 argument");
    const auto &arg = args[0];
    if (std::holds_alternative<std::monostate>(arg.v)) return Value::make_str("none");
    if (std::holds_alternative<int64_t>(arg.v)) return Value::make_str("int");
    if (std::holds_alternative<double>(arg.v)) return Value::make_str("float");
    if (std::holds_alternative<std::string>(arg.v)) return Value::make_str("string");
    if (std::holds_alternative<bool>(arg.v)) return Value::make_str("bool");
    if (std::holds_alternative<std::vector<Value>>(arg.v)) return Value::make_str("list");
    if (std::holds_alternative<std::shared_ptr<ObjectInstance>>(arg.v)) return Value::make_str("object");
    if (std::holds_alternative<std::shared_ptr<Reference>>(arg.v)) return Value::make_str("ref");
    return Value::make_str("unknown");
  }
  if (marker == "__builtin_vars") {
    if (!env) throw std::runtime_error("vars() requires environment access");
    if (args.empty()) {
      std::vector<std::string> names;
      std::unordered_set<std::string> seen;
      for (const auto &name : env->keys()) {
        if (seen.insert(name).second) names.push_back(name);
      }
      std::vector<Value> list;
      for (const auto &name : names) list.push_back(Value::make_str(name));
      return Value::make_list(std::move(list));
    }
    if (args.size() == 1 &&
        std::holds_alternative<std::shared_ptr<ObjectInstance>>(args[0].v)) {
      auto obj = std::get<std::shared_ptr<ObjectInstance>>(args[0].v);
      std::vector<Value> list;
      for (const auto &name : obj->properties->local_keys()) {
        list.push_back(Value::make_str(name));
      }
      return Value::make_list(std::move(list));
    }
    throw std::runtime_error("vars() accepts no args or an object instance");
  }
  if (marker == "__builtin_keys") {
    if (args.size() != 1) throw std::runtime_error("keys() requires 1 argument");
    if (std::holds_alternative<std::vector<Value>>(args[0].v)) {
      const auto &list = std::get<std::vector<Value>>(args[0].v);
      std::vector<Value> result;
      for (size_t i = 0; i < list.size(); ++i) {
        result.push_back(Value::make_int(static_cast<int64_t>(i)));
      }
      return Value::make_list(std::move(result));
    }
    if (std::holds_alternative<std::shared_ptr<ObjectInstance>>(args[0].v)) {
      auto obj = std::get<std::shared_ptr<ObjectInstance>>(args[0].v);
      std::vector<Value> result;
      for (const auto &name : obj->properties->local_keys()) {
        result.push_back(Value::make_str(name));
      }
      return Value::make_list(std::move(result));
    }
    throw std::runtime_error("keys() requires a list or object");
  }
  if (marker == "__builtin_get") {
    if (args.size() != 2)
      throw std::runtime_error("get() requires 2 arguments (object, key)");
    if (std::holds_alternative<std::shared_ptr<ObjectInstance>>(args[0].v) &&
        std::holds_alternative<std::string>(args[1].v)) {
      auto obj = std::get<std::shared_ptr<ObjectInstance>>(args[0].v);
      auto prop = obj->properties->get(std::get<std::string>(args[1].v));
      if (!prop) return Value();
      return *prop;
    }
    throw std::runtime_error("get() requires (object, string)");
  }
  if (marker == "__builtin_set") {
    if (args.size() != 3)
      throw std::runtime_error("set() requires 3 arguments (object, key, value)");
    if (std::holds_alternative<std::shared_ptr<ObjectInstance>>(args[0].v) &&
        std::holds_alternative<std::string>(args[1].v)) {
      auto obj = std::get<std::shared_ptr<ObjectInstance>>(args[0].v);
      obj->properties->set(std::get<std::string>(args[1].v), args[2]);
      return Value();
    }
    throw std::runtime_error("set() requires (object, string, value)");
  }
  if (marker == "__builtin_system") {
    if (args.size() != 1) throw std::runtime_error("system() requires 1 argument");
    std::string cmd = std::get<std::string>(args[0].v);
    int code = std::system(cmd.c_str());
    return Value::make_int(static_cast<int64_t>(code));
  }
  if (marker == "__builtin_shell") {
    if (args.size() != 1) throw std::runtime_error("shell() requires 1 argument");
    std::string cmd = std::get<std::string>(args[0].v);
    std::string output;
    FILE *pipe = popen(cmd.c_str(), "r");
    if (!pipe) throw std::runtime_error("shell() failed to open pipe");
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe)) {
      output += buffer;
    }
    int status = pclose(pipe);
    if (status == -1) throw std::runtime_error("shell() failed to close pipe");
    return Value::make_str(output);
  }
  if (marker == "__builtin_getenv") {
    if (args.size() != 1) throw std::runtime_error("getenv() requires 1 argument");
    std::string name = std::get<std::string>(args[0].v);
    const char *value = std::getenv(name.c_str());
    return value ? Value::make_str(value) : Value();
  }
  if (marker == "__builtin_setenv") {
    if (args.size() != 2) throw std::runtime_error("setenv() requires 2 arguments");
    std::string name = std::get<std::string>(args[0].v);
    std::string value = std::get<std::string>(args[1].v);
    if (setenv(name.c_str(), value.c_str(), 1) != 0)
      throw std::runtime_error("setenv() failed");
    return Value();
  }
  if (marker == "__builtin_sleep") {
    if (args.size() != 1) throw std::runtime_error("sleep() requires 1 argument");
    int64_t ms = static_cast<int64_t>(value_as_number(args[0]).as_number());
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    return Value();
  }
  if (marker == "__builtin_pwd") {
    if (!args.empty()) throw std::runtime_error("pwd() takes no arguments");
    return Value::make_str(fs::current_path().string());
  }
  if (marker == "__builtin_path_join") {
    if (args.empty()) throw std::runtime_error("path_join() requires at least 1 argument");
    fs::path result;
    for (const auto &arg : args) {
      result /= std::get<std::string>(arg.v);
    }
    return Value::make_str(result.string());
  }
  if (marker == "__builtin_basename") {
    if (args.size() != 1) throw std::runtime_error("basename() requires 1 argument");
    fs::path p = std::get<std::string>(args[0].v);
    return Value::make_str(p.filename().string());
  }
  if (marker == "__builtin_dirname") {
    if (args.size() != 1) throw std::runtime_error("dirname() requires 1 argument");
    fs::path p = std::get<std::string>(args[0].v);
    return Value::make_str(p.parent_path().string());
  }
  if (marker == "__builtin_path_is_absolute") {
    if (args.size() != 1) throw std::runtime_error("path_is_absolute() requires 1 argument");
    fs::path p = std::get<std::string>(args[0].v);
    return Value::make_bool(p.is_absolute());
  }
  if (marker == "__builtin_path_normalize") {
    if (args.size() != 1) throw std::runtime_error("path_normalize() requires 1 argument");
    fs::path p = std::get<std::string>(args[0].v);
    return Value::make_str(p.lexically_normal().string());
  }
  if (marker == "__builtin_file_ext") {
    if (args.size() != 1) throw std::runtime_error("file_ext() requires 1 argument");
    fs::path p = std::get<std::string>(args[0].v);
    return Value::make_str(p.extension().string());
  }
  if (marker == "__builtin_json_parse") {
    if (args.size() != 1) throw std::runtime_error("json_parse() requires 1 argument");
    const std::string &text = std::get<std::string>(args[0].v);
    size_t pos = 0;
    Value result = parse_json_value(text, pos);
    skip_json_ws(text, pos);
    if (pos != text.size()) throw std::runtime_error("Invalid JSON input");
    return result;
  }
  if (marker == "__builtin_json_stringify") {
    if (args.size() != 1) throw std::runtime_error("json_stringify() requires 1 argument");
    return Value::make_str(json_stringify_value(args[0]));
  }
  if (marker == "__builtin_csv_parse") {
    if (args.size() < 1 || args.size() > 2)
      throw std::runtime_error("csv_parse() requires 1 or 2 arguments");
    std::string text = std::get<std::string>(args[0].v);
    std::string delim = (args.size() == 2) ? std::get<std::string>(args[1].v) : ",";
    if (delim.empty()) throw std::runtime_error("csv_parse() delimiter cannot be empty");
    auto rows = parse_csv_text(text, delim[0]);
    return Value::make_list(std::move(rows));
  }
  if (marker == "__builtin_csv_stringify") {
    if (args.size() < 1 || args.size() > 2)
      throw std::runtime_error("csv_stringify() requires 1 or 2 arguments");
    const auto &rows = as_list(args[0]);
    std::string delim = (args.size() == 2) ? std::get<std::string>(args[1].v) : ",";
    if (delim.empty()) throw std::runtime_error("csv_stringify() delimiter cannot be empty");
    std::string output;
    for (size_t ri = 0; ri < rows.size(); ++ri) {
      const auto &row = rows[ri];
      if (!std::holds_alternative<std::vector<Value>>(row.v))
        throw std::runtime_error("csv_stringify() requires a list of rows");
      const auto &fields = std::get<std::vector<Value>>(row.v);
      for (size_t fi = 0; fi < fields.size(); ++fi) {
        if (fi) output += delim;
        output += csv_escape_field(value_to_string(fields[fi]), delim[0]);
      }
      if (ri + 1 < rows.size()) output += '\n';
    }
    return Value::make_str(output);
  }
  if (marker == "__builtin_mkdirs") {
    if (args.size() != 1) throw std::runtime_error("mkdirs() requires 1 argument");
    std::string path = std::get<std::string>(args[0].v);
    return Value::make_bool(fs::create_directories(path));
  }
  if (marker == "__builtin_base64_encode") {
    if (args.size() != 1) throw std::runtime_error("base64_encode() requires 1 argument");
    return Value::make_str(base64_encode(std::get<std::string>(args[0].v)));
  }
  if (marker == "__builtin_base64_decode") {
    if (args.size() != 1) throw std::runtime_error("base64_decode() requires 1 argument");
    return Value::make_str(base64_decode(std::get<std::string>(args[0].v)));
  }
  if (marker == "__builtin_uuid4") {
    if (!args.empty()) throw std::runtime_error("uuid4() takes no arguments");
    return Value::make_str(uuid4());
  }
  if (marker == "__builtin_glob") {
    if (args.size() != 1) throw std::runtime_error("glob() requires 1 argument");
    std::string pattern = std::get<std::string>(args[0].v);
    fs::path p(pattern);
    fs::path dir = p.parent_path();
    std::string filename_pattern = p.filename().string();
    if (dir.empty()) dir = fs::current_path();
    std::vector<Value> matches;
    if (fs::exists(dir) && fs::is_directory(dir)) {
      for (auto &entry : fs::directory_iterator(dir)) {
        if (glob_match(filename_pattern, entry.path().filename().string())) {
          matches.push_back(Value::make_str(entry.path().string()));
        }
      }
    }
    return Value::make_list(std::move(matches));
  }
  if (marker == "__builtin_regex_match") {
    if (args.size() != 2) throw std::runtime_error("regex_match() requires 2 arguments");
    std::string text = std::get<std::string>(args[0].v);
    std::string pattern = std::get<std::string>(args[1].v);
    try {
      return Value::make_bool(std::regex_match(text, std::regex(pattern)));
    } catch (const std::regex_error &e) {
      throw std::runtime_error(std::string("Invalid regex: ") + e.what());
    }
  }
  if (marker == "__builtin_regex_replace") {
    if (args.size() != 3) throw std::runtime_error("regex_replace() requires 3 arguments");
    std::string text = std::get<std::string>(args[0].v);
    std::string pattern = std::get<std::string>(args[1].v);
    std::string replacement = std::get<std::string>(args[2].v);
    try {
      return Value::make_str(std::regex_replace(text, std::regex(pattern), replacement));
    } catch (const std::regex_error &e) {
      throw std::runtime_error(std::string("Invalid regex: ") + e.what());
    }
  }
#ifdef BLOA_USE_MYSQL
  if (marker == "__builtin_mysql_connect") {
    if (args.size() < 4 || args.size() > 5)
      throw std::runtime_error("mysql_connect() requires 4 or 5 arguments");
    std::string host = std::get<std::string>(args[0].v);
    std::string user = std::get<std::string>(args[1].v);
    std::string pass = std::get<std::string>(args[2].v);
    std::string db = std::get<std::string>(args[3].v);
    unsigned int port = 3306;
    if (args.size() == 5) port = static_cast<unsigned int>(value_as_number(args[4]).as_number());
    MYSQL *conn = mysql_init(nullptr);
    if (!conn) throw std::runtime_error("mysql_init() failed");
    if (!mysql_real_connect(conn, host.c_str(), user.c_str(), pass.c_str(), db.c_str(), port, nullptr, 0)) {
      std::string err = mysql_error(conn);
      mysql_close(conn);
      throw std::runtime_error("MySQL connection failed: " + err);
    }
    int id = next_mysql_connection_id++;
    mysql_connections[id] = conn;
    return Value::make_int(id);
  }
  if (marker == "__builtin_mysql_close") {
    if (args.size() != 1)
      throw std::runtime_error("mysql_close() requires 1 connection id");
    int id = static_cast<int>(value_as_number(args[0]).as_number());
    auto it = mysql_connections.find(id);
    if (it == mysql_connections.end()) throw std::runtime_error("Invalid MySQL connection id");
    mysql_close(it->second);
    mysql_connections.erase(it);
    return Value();
  }
  if (marker == "__builtin_mysql_escape") {
    if (args.size() != 2)
      throw std::runtime_error("mysql_escape() requires 2 arguments");
    int id = static_cast<int>(value_as_number(args[0]).as_number());
    auto it = mysql_connections.find(id);
    if (it == mysql_connections.end()) throw std::runtime_error("Invalid MySQL connection id");
    std::string value = std::get<std::string>(args[1].v);
    std::string out(value.size() * 2 + 1, '\0');
    unsigned long len = mysql_real_escape_string(it->second, out.data(), value.c_str(), static_cast<unsigned long>(value.size()));
    out.resize(len);
    return Value::make_str(out);
  }
  if (marker == "__builtin_mysql_query") {
    if (args.size() != 2)
      throw std::runtime_error("mysql_query() requires 2 arguments");
    int id = static_cast<int>(value_as_number(args[0]).as_number());
    auto it = mysql_connections.find(id);
    if (it == mysql_connections.end()) throw std::runtime_error("Invalid MySQL connection id");
    std::string sql = std::get<std::string>(args[1].v);
    if (mysql_query(it->second, sql.c_str()) != 0) {
      throw std::runtime_error(std::string("MySQL query failed: ") + mysql_error(it->second));
    }
    MYSQL_RES *result = mysql_store_result(it->second);
    if (!result) return Value::make_list({});
    std::vector<Value> rows;
    MYSQL_ROW row;
    unsigned int num_fields = mysql_num_fields(result);
    while ((row = mysql_fetch_row(result))) {
      std::vector<Value> row_values;
      unsigned long *lengths = mysql_fetch_lengths(result);
      for (unsigned int i = 0; i < num_fields; ++i) {
        if (row[i]) row_values.push_back(Value::make_str(std::string(row[i], lengths[i])));
        else row_values.push_back(Value());
      }
      rows.push_back(Value::make_list(std::move(row_values)));
    }
    mysql_free_result(result);
    return Value::make_list(std::move(rows));
  }
  if (marker == "__builtin_mysql_exec") {
    if (args.size() != 2)
      throw std::runtime_error("mysql_exec() requires 2 arguments");
    int id = static_cast<int>(value_as_number(args[0]).as_number());
    auto it = mysql_connections.find(id);
    if (it == mysql_connections.end()) throw std::runtime_error("Invalid MySQL connection id");
    std::string sql = std::get<std::string>(args[1].v);
    if (mysql_query(it->second, sql.c_str()) != 0) {
      throw std::runtime_error(std::string("MySQL exec failed: ") + mysql_error(it->second));
    }
    auto affected = mysql_affected_rows(it->second);
    return Value::make_int(static_cast<int64_t>(affected));
  }
#endif
  if (marker == "__builtin_str") {
    if (args.size() != 1) throw std::runtime_error("str() requires 1 argument");
    return Value::make_str(value_to_string(args[0]));
  }
  if (marker == "__builtin_sqrt") {
    if (args.size() != 1)
      throw std::runtime_error("sqrt() requires 1 argument");
    double x = value_as_number(args[0]).as_number();
    return Value::make_double(std::sqrt(x));
  }
  if (marker == "__builtin_pow") {
    if (args.size() != 2)
      throw std::runtime_error("pow() requires 2 arguments");
    double base = value_as_number(args[0]).as_number();
    double exp = value_as_number(args[1]).as_number();
    return Value::make_double(std::pow(base, exp));
  }
  if (marker == "__builtin_sin") {
    if (args.size() != 1) throw std::runtime_error("sin() requires 1 argument");
    double x = value_as_number(args[0]).as_number();
    return Value::make_double(std::sin(x));
  }
  if (marker == "__builtin_cos") {
    if (args.size() != 1) throw std::runtime_error("cos() requires 1 argument");
    double x = value_as_number(args[0]).as_number();
    return Value::make_double(std::cos(x));
  }
  if (marker == "__builtin_tan") {
    if (args.size() != 1) throw std::runtime_error("tan() requires 1 argument");
    double x = value_as_number(args[0]).as_number();
    return Value::make_double(std::tan(x));
  }
  if (marker == "__builtin_log") {
    if (args.size() != 1) throw std::runtime_error("log() requires 1 argument");
    double x = value_as_number(args[0]).as_number();
    return Value::make_double(std::log(x));
  }
  if (marker == "__builtin_exp") {
    if (args.size() != 1) throw std::runtime_error("exp() requires 1 argument");
    double x = value_as_number(args[0]).as_number();
    return Value::make_double(std::exp(x));
  }
  if (marker == "__builtin_abs") {
    if (args.size() != 1) throw std::runtime_error("abs() requires 1 argument");
    double x = value_as_number(args[0]).as_number();
    return Value::make_double(std::abs(x));
  }
  if (marker == "__builtin_floor") {
    if (args.size() != 1)
      throw std::runtime_error("floor() requires 1 argument");
    double x = value_as_number(args[0]).as_number();
    return Value::make_double(std::floor(x));
  }
  if (marker == "__builtin_ceil") {
    if (args.size() != 1)
      throw std::runtime_error("ceil() requires 1 argument");
    double x = value_as_number(args[0]).as_number();
    return Value::make_double(std::ceil(x));
  }
  if (marker == "__builtin_round") {
    if (args.size() != 1)
      throw std::runtime_error("round() requires 1 argument");
    double x = value_as_number(args[0]).as_number();
    return Value::make_double(std::round(x));
  }
  if (marker == "__builtin_pi") {
    return Value::make_double(3.141592653589793);
  }
  if (marker == "__builtin_e") {
    return Value::make_double(2.718281828459045);
  }
  if (marker == "__builtin_read_file") {
    if (args.size() != 1)
      throw std::runtime_error("read_file() requires 1 argument");
    std::string path = std::get<std::string>(args[0].v);
    std::ifstream ifs(path);
    if (!ifs) throw std::runtime_error("Cannot open file: " + path);
    std::string content((std::istreambuf_iterator<char>(ifs)),
                        std::istreambuf_iterator<char>());
    return Value::make_str(content);
  }
  if (marker == "__builtin_write_file") {
    if (args.size() != 2)
      throw std::runtime_error("write_file() requires 2 arguments");
    std::string path = std::get<std::string>(args[0].v);
    std::string content = std::get<std::string>(args[1].v);
    std::ofstream ofs(path);
    if (!ofs) throw std::runtime_error("Cannot open file for writing: " + path);
    ofs << content;
    return Value();
  }
  if (marker == "__builtin_exists") {
    if (args.size() != 1)
      throw std::runtime_error("exists() requires 1 argument");
    std::string path = std::get<std::string>(args[0].v);
    return Value::make_bool(fs::exists(path));
  }
  if (marker == "__builtin_list_dir") {
    if (args.size() != 1)
      throw std::runtime_error("list_dir() requires 1 argument");
    std::string path = std::get<std::string>(args[0].v);
    std::vector<Value> list;
    std::transform(fs::directory_iterator(path), fs::directory_iterator{}, std::back_inserter(list), [](const auto& entry) {
      return Value::make_str(entry.path().string());
    });
    return Value::make_list(list);
  }
  if (marker == "__builtin_mkdir") {
    if (args.size() != 1)
      throw std::runtime_error("mkdir() requires 1 argument");
    std::string path = std::get<std::string>(args[0].v);
    return Value::make_bool(fs::create_directory(path));
  }
  if (marker == "__builtin_rmdir") {
    if (args.size() != 1)
      throw std::runtime_error("rmdir() requires 1 argument");
    std::string path = std::get<std::string>(args[0].v);
    return Value::make_bool(fs::remove(path));
  }
  if (marker == "__builtin_remove") {
    if (args.size() != 1)
      throw std::runtime_error("remove() requires 1 argument");
    std::string path = std::get<std::string>(args[0].v);
    return Value::make_bool(fs::remove(path));
  }
  if (marker == "__builtin_copy_file") {
    if (args.size() != 2)
      throw std::runtime_error("copy_file() requires 2 arguments");
    std::string from = std::get<std::string>(args[0].v);
    std::string to = std::get<std::string>(args[1].v);
    fs::copy_file(from, to);
    return Value();
  }
  if (marker == "__builtin_move") {
    if (args.size() != 2)
      throw std::runtime_error("move() requires 2 arguments");
    std::string from = std::get<std::string>(args[0].v);
    std::string to = std::get<std::string>(args[1].v);
    fs::rename(from, to);
    return Value();
  }
  if (marker == "__builtin_file_size") {
    if (args.size() != 1)
      throw std::runtime_error("file_size() requires 1 argument");
    std::string path = std::get<std::string>(args[0].v);
    return Value::make_int(fs::file_size(path));
  }
  if (marker == "__builtin_is_dir") {
    if (args.size() != 1)
      throw std::runtime_error("is_dir() requires 1 argument");
    std::string path = std::get<std::string>(args[0].v);
    return Value::make_bool(fs::is_directory(path));
  }
  if (marker == "__builtin_split") {
    if (args.size() < 1 || args.size() > 2)
      throw std::runtime_error("split() requires 1 or 2 arguments");
    std::string s = std::get<std::string>(args[0].v);
    std::string delim =
        (args.size() == 2) ? std::get<std::string>(args[1].v) : " ";
    std::vector<Value> list;
    size_t pos = 0;
    while ((pos = s.find(delim)) != std::string::npos) {
      list.push_back(Value::make_str(s.substr(0, pos)));
      s.erase(0, pos + delim.length());
    }
    list.push_back(Value::make_str(s));
    return Value::make_list(list);
  }
  if (marker == "__builtin_join") {
    if (args.size() != 2)
      throw std::runtime_error("join() requires 2 arguments");
    const auto &list = as_list(args[0]);
    std::string sep = std::get<std::string>(args[1].v);
    std::string result;
    for (size_t i = 0; i < list.size(); ++i) {
      if (i > 0) result += sep;
      result += value_to_string(list[i]);
    }
    return Value::make_str(result);
  }
  if (marker == "__builtin_substr") {
    if (args.size() < 2 || args.size() > 3)
      throw std::runtime_error("substr() requires 2 or 3 arguments");
    std::string s = std::get<std::string>(args[0].v);
    size_t start = static_cast<size_t>(value_as_number(args[1]).as_number());
    size_t len = (args.size() == 3)
                     ? static_cast<size_t>(value_as_number(args[2]).as_number())
                     : std::string::npos;
    return Value::make_str(s.substr(start, len));
  }
  if (marker == "__builtin_find") {
    if (args.size() != 2)
      throw std::runtime_error("find() requires 2 arguments");
    std::string s = std::get<std::string>(args[0].v);
    std::string sub = std::get<std::string>(args[1].v);
    size_t pos = s.find(sub);
    return Value::make_int(
        pos == std::string::npos ? -1 : static_cast<int64_t>(pos));
  }
  if (marker == "__builtin_replace") {
    if (args.size() != 3)
      throw std::runtime_error("replace() requires 3 arguments");
    std::string s = std::get<std::string>(args[0].v);
    std::string old_str = std::get<std::string>(args[1].v);
    std::string new_str = std::get<std::string>(args[2].v);
    size_t pos = 0;
    while ((pos = s.find(old_str, pos)) != std::string::npos) {
      s.replace(pos, old_str.length(), new_str);
      pos += new_str.length();
    }
    return Value::make_str(s);
  }
  if (marker == "__builtin_to_upper") {
    if (args.size() != 1)
      throw std::runtime_error("to_upper() requires 1 argument");
    std::string s = std::get<std::string>(args[0].v);
    std::transform(s.begin(), s.end(), s.begin(), ::toupper);
    return Value::make_str(s);
  }
  if (marker == "__builtin_to_lower") {
    if (args.size() != 1)
      throw std::runtime_error("to_lower() requires 1 argument");
    std::string s = std::get<std::string>(args[0].v);
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return Value::make_str(s);
  }
  if (marker == "__builtin_trim") {
    if (args.size() != 1)
      throw std::runtime_error("trim() requires 1 argument");
    std::string s = std::get<std::string>(args[0].v);
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
              return !std::isspace(ch);
            }));
    s.erase(std::find_if(s.rbegin(), s.rend(),
                         [](unsigned char ch) { return !std::isspace(ch); })
                .base(),
            s.end());
    return Value::make_str(s);
  }
  if (marker == "__builtin_starts_with") {
    if (args.size() != 2)
      throw std::runtime_error("starts_with() requires 2 arguments");
    std::string s = std::get<std::string>(args[0].v);
    std::string prefix = std::get<std::string>(args[1].v);
    return Value::make_bool(s.starts_with(prefix));
  }
  if (marker == "__builtin_ends_with") {
    if (args.size() != 2)
      throw std::runtime_error("ends_with() requires 2 arguments");
    std::string s = std::get<std::string>(args[0].v);
    std::string suffix = std::get<std::string>(args[1].v);
    return Value::make_bool(s.ends_with(suffix));
  }
  if (marker == "__builtin_contains") {
    if (args.size() != 2)
      throw std::runtime_error("contains() requires 2 arguments");
    std::string s = std::get<std::string>(args[0].v);
    std::string sub = std::get<std::string>(args[1].v);
    return Value::make_bool(s.find(sub) != std::string::npos);
  }
  if (marker == "__builtin_reverse") {
    if (args.size() != 1)
      throw std::runtime_error("reverse() requires 1 argument");
    std::string s = std::get<std::string>(args[0].v);
    std::reverse(s.begin(), s.end());
    return Value::make_str(s);
  }
  if (marker == "__builtin_repeat") {
    if (args.size() != 2)
      throw std::runtime_error("repeat() requires 2 arguments");
    std::string s = std::get<std::string>(args[0].v);
    int64_t n = static_cast<int64_t>(value_as_number(args[1]).as_number());
    std::string result;
    for (int64_t i = 0; i < n; ++i) result += s;
    return Value::make_str(result);
  }
  if (marker == "__builtin_baar_create") {
    if (args.size() != 2)
      throw std::runtime_error("baar_create() requires 2 arguments");
    std::string path = std::get<std::string>(args[0].v);
    const auto &files = as_list(args[1]);
    std::vector<std::pair<std::string, std::string>> entries;
    for (const auto &entry_val : files) {
      if (!std::holds_alternative<std::vector<Value>>(entry_val.v))
        throw std::runtime_error("baar_create() file list must contain [name, data] entries");
      const auto &entry = std::get<std::vector<Value>>(entry_val.v);
      if (entry.size() != 2)
        throw std::runtime_error("baar_create() entry must be [name, data]");
      if (!std::holds_alternative<std::string>(entry[0].v))
        throw std::runtime_error("baar_create() entry name must be a string");
      entries.emplace_back(std::get<std::string>(entry[0].v),
                           value_to_string(entry[1]));
    }
    write_archive(path, entries);
    return Value();
  }
  if (marker == "__builtin_baar_extract") {
    if (args.size() != 2)
      throw std::runtime_error("baar_extract() requires 2 arguments");
    std::string path = std::get<std::string>(args[0].v);
    std::string dest = std::get<std::string>(args[1].v);
    auto entries = read_archive(path);
    fs::create_directories(dest);
    for (const auto &entry : entries) {
      fs::path out_path = fs::path(dest) / entry.first;
      fs::create_directories(out_path.parent_path());
      std::ofstream ofs(out_path, std::ios::binary);
      ofs << entry.second;
    }
    return Value();
  }
  if (marker == "__builtin_baar_list") {
    if (args.size() != 1)
      throw std::runtime_error("baar_list() requires 1 argument");
    std::string path = std::get<std::string>(args[0].v);
    auto entries = read_archive(path);
    std::vector<Value> list;
    for (const auto &entry : entries) {
      list.push_back(Value::make_str(entry.first));
    }
    return Value::make_list(list);
  }
  if (marker == "__builtin_baar_read") {
    if (args.size() != 2)
      throw std::runtime_error("baar_read() requires 2 arguments");
    std::string path = std::get<std::string>(args[0].v);
    std::string name = std::get<std::string>(args[1].v);
    auto entries = read_archive(path);
    for (const auto &entry : entries) {
      if (entry.first == name) return Value::make_str(entry.second);
    }
    throw std::runtime_error("Baar entry not found: " + name);
  }
#ifdef BLOA_USE_CURL
  if (marker == "__builtin_curl_get" || marker == "__builtin_curl_post" ||
      marker == "__builtin_curl_request") {
    if (args.empty() || args.size() > 3)
      throw std::runtime_error("curl_get/curl_post/curl_request() requires 1-3 arguments");
    std::string url = std::get<std::string>(args[0].v);
    std::string method = "GET";
    std::string body;
    if (marker == "__builtin_curl_post") {
      method = "POST";
      if (args.size() < 2)
        throw std::runtime_error("curl_post() requires url and body");
      body = std::get<std::string>(args[1].v);
    }
    if (marker == "__builtin_curl_request") {
      if (args.size() >= 2)
        method = std::get<std::string>(args[1].v);
      if (args.size() == 3)
        body = std::get<std::string>(args[2].v);
    }
    CURL *curl = curl_easy_init();
    if (!curl) throw std::runtime_error("Failed to initialize curl");
    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    if (method == "POST") {
      curl_easy_setopt(curl, CURLOPT_POST, 1L);
      curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    } else if (method != "GET") {
      curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method.c_str());
      if (!body.empty()) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
      }
    }
    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup(curl);
    if (res != CURLE_OK) {
      throw std::runtime_error(std::string("curl failed: ") + curl_easy_strerror(res));
    }
    return Value::make_str(response);
  }
#endif
#ifdef BLOA_USE_SQLITE
  if (marker == "__builtin_sqlite_query" || marker == "__builtin_sqlite_exec") {
    if (args.size() != 2)
      throw std::runtime_error("sqlite_query/sqlite_exec() requires 2 arguments");
    std::string path = std::get<std::string>(args[0].v);
    std::string sql = std::get<std::string>(args[1].v);
    sqlite3 *db = nullptr;
    int rc = sqlite3_open(path.c_str(), &db);
    if (rc != SQLITE_OK) {
      std::string err = sqlite3_errmsg(db);
      sqlite3_close(db);
      throw std::runtime_error("SQLite open failed: " + err);
    }
    if (marker == "__builtin_sqlite_exec") {
      char *errmsg = nullptr;
      rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errmsg);
      if (rc != SQLITE_OK) {
        std::string err = errmsg ? errmsg : "unknown";
        sqlite3_free(errmsg);
        sqlite3_close(db);
        throw std::runtime_error("SQLite exec failed: " + err);
      }
      sqlite3_close(db);
      return Value::make_int(1);
    }
    sqlite3_stmt *stmt = nullptr;
    rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
      std::string err = sqlite3_errmsg(db);
      sqlite3_close(db);
      throw std::runtime_error("SQLite prepare failed: " + err);
    }
    std::vector<Value> rows;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
      std::vector<Value> row;
      int cols = sqlite3_column_count(stmt);
      for (int col = 0; col < cols; ++col) {
        const unsigned char *text = sqlite3_column_text(stmt, col);
        row.push_back(Value::make_str(text ? reinterpret_cast<const char *>(text) : ""));
      }
      rows.push_back(Value::make_list(std::move(row)));
    }
    if (rc != SQLITE_DONE) {
      std::string err = sqlite3_errmsg(db);
      sqlite3_finalize(stmt);
      sqlite3_close(db);
      throw std::runtime_error("SQLite step failed: " + err);
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return Value::make_list(rows);
  }
#endif
  if (marker == "__builtin_random_int") {
    if (args.size() < 1 || args.size() > 2)
      throw std::runtime_error("random_int() requires 1 or 2 arguments");
    int64_t min =
        (args.size() == 2)
            ? static_cast<int64_t>(value_as_number(args[0]).as_number())
            : 0;
    int64_t max =
        (args.size() == 2)
            ? static_cast<int64_t>(value_as_number(args[1]).as_number())
            : static_cast<int64_t>(value_as_number(args[0]).as_number());
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<int64_t> dist(min, max);
    return Value::make_int(dist(gen));
  }
  if (marker == "__builtin_random_float") {
    if (args.size() < 1 || args.size() > 2)
      throw std::runtime_error("random_float() requires 1 or 2 arguments");
    double min =
        (args.size() == 2) ? value_as_number(args[0]).as_number() : 0.0;
    double max = (args.size() == 2) ? value_as_number(args[1]).as_number()
                                    : value_as_number(args[0]).as_number();
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_real_distribution<double> dist(min, max);
    return Value::make_double(dist(gen));
  }
  if (marker == "__builtin_now") {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    auto millis =
        std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    return Value::make_int(millis);
  }
  throw std::runtime_error("Unknown built-in: " + marker);
}

}  // namespace bloa