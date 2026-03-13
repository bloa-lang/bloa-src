#include "bloa/stdlib.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <random>
#include <sstream>

namespace fs = std::filesystem;

namespace bloa {

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
}

Value handle_builtin(const std::string &marker,
                     const std::vector<Value> &args) {
  if (marker == "__builtin_print") {
    for (size_t i = 0; i < args.size(); ++i) {
      if (i > 0) std::cout << ' ';
      std::cout << value_to_string(args[i]);
    }
    std::cout << '\n';
    return Value();
  }
  if (marker == "__builtin_range") {
    if (args.size() != 2)
      throw std::runtime_error("range() requires 2 arguments");
    int64_t start = static_cast<int64_t>(value_as_number(args[0]).as_number());
    int64_t stop = static_cast<int64_t>(value_as_number(args[1]).as_number());
    std::vector<Value> list;
    for (int64_t i = start; i < stop; ++i) list.push_back(Value::make_int(i));
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
    } else {
      throw std::runtime_error("len() argument must be string or list");
    }
  }
  if (marker == "__builtin_str") {
    if (args.size() != 1) throw std::runtime_error("str() requires 1 argument");
    return Value::make_str(value_to_string(args[0]));
  }
  if (marker == "__builtin_int") {
    if (args.size() != 1) throw std::runtime_error("int() requires 1 argument");
    return Value::make_int(
        static_cast<int64_t>(value_as_number(args[0]).as_number()));
  }
  if (marker == "__builtin_float") {
    if (args.size() != 1)
      throw std::runtime_error("float() requires 1 argument");
    return Value::make_double(value_as_number(args[0]).as_number());
  }
  if (marker == "__builtin_append") {
    if (args.size() != 2)
      throw std::runtime_error("append() requires 2 arguments (list, value)");
    if (!std::holds_alternative<std::vector<Value>>(args[0].v))
      throw std::runtime_error("First argument to append() must be a list");
    auto list = std::get<std::vector<Value>>(args[0].v);
    std::vector<Value> new_list = list;
    new_list.push_back(args[1]);
    return Value::make_list(std::move(new_list));
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