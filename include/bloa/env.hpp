#pragma once
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace bloa {

struct Environment;  // forward declaration

struct ObjectInstance {
  std::string class_name;
  std::shared_ptr<Environment> properties;
  ObjectInstance(std::string c, std::shared_ptr<Environment> p)
      : class_name(std::move(c)), properties(std::move(p)) {}
};

struct Value;
using ValuePtr = std::shared_ptr<Value>;

struct Value {
  std::variant<std::monostate, int64_t, double, std::string, bool,
               std::vector<Value>, std::shared_ptr<ObjectInstance>>
      v;

  Value() = default;

  static Value make_int(int64_t i) {
    Value val;
    val.v = i;
    return val;
  }
  static Value make_double(double d) {
    Value val;
    val.v = d;
    return val;
  }
  static Value make_str(std::string s) {
    Value val;
    val.v = std::move(s);
    return val;
  }
  static Value make_bool(bool b) {
    Value val;
    val.v = b;
    return val;
  }
  static Value make_list(std::vector<Value> list) {
    Value val;
    val.v = std::move(list);
    return val;
  }

  static Value make_object(std::string class_name,
                           std::shared_ptr<Environment> properties) {
    Value val;
    val.v = std::make_shared<ObjectInstance>(std::move(class_name),
                                             std::move(properties));
    return val;
  }

  std::string to_string() const;

  double as_number() const {
    if (std::holds_alternative<int64_t>(v)) return (double)std::get<int64_t>(v);
    if (std::holds_alternative<double>(v)) return std::get<double>(v);
    throw std::runtime_error("Value is not a number");
  }

  std::vector<Value> as_list() const {
    if (std::holds_alternative<std::vector<Value>>(v))
      return std::get<std::vector<Value>>(v);
    throw std::runtime_error("Value is not a list");
  }
};

struct Variable {
  Value value;
  std::string visibility;
};

struct Environment {
  Environment(std::shared_ptr<Environment> parent = nullptr);
  std::optional<Value> get(const std::string &name) const;
  void set(const std::string &name, Value val);
  std::shared_ptr<Environment> parent;

 private:
  std::unordered_map<std::string, Variable> vars;
};

}  // namespace bloa
