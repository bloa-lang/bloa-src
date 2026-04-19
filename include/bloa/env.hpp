#pragma once
#include <cmath>
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

struct Reference {
  std::shared_ptr<Environment> env;
  std::string name;
  Reference(std::shared_ptr<Environment> e, std::string n)
      : env(std::move(e)), name(std::move(n)) {}
};

struct Value;
using ValuePtr = std::shared_ptr<Value>;

struct Value {
  std::variant<std::monostate, int64_t, double, std::string, bool,
               std::vector<Value>, std::shared_ptr<ObjectInstance>,
               std::shared_ptr<Reference>>
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

  static Value make_ref(std::shared_ptr<Environment> env,
                        std::string name) {
    Value val;
    val.v = std::make_shared<Reference>(std::move(env), std::move(name));
    return val;
  }

  bool is_reference() const {
    return std::holds_alternative<std::shared_ptr<Reference>>(v);
  }

  const Reference &as_reference() const {
    return *std::get<std::shared_ptr<Reference>>(v);
  }

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

  std::string to_string() const {
    if (std::holds_alternative<std::monostate>(v)) return "None";
    if (std::holds_alternative<int64_t>(v)) return std::to_string(std::get<int64_t>(v));
    if (std::holds_alternative<double>(v)) {
      double d = std::get<double>(v);
      if (std::floor(d) == d) return std::to_string(static_cast<int64_t>(d));
      return std::to_string(d);
    }
    if (std::holds_alternative<std::string>(v)) return std::get<std::string>(v);
    if (std::holds_alternative<bool>(v)) return std::get<bool>(v) ? "true" : "false";
    if (std::holds_alternative<std::vector<Value>>(v)) {
      std::string out = "[";
      const auto &list = std::get<std::vector<Value>>(v);
      for (size_t i = 0; i < list.size(); ++i) {
        if (i > 0) out += ", ";
        out += list[i].to_string();
      }
      out += "]";
      return out;
    }
    if (std::holds_alternative<std::shared_ptr<ObjectInstance>>(v)) {
      const auto &obj = std::get<std::shared_ptr<ObjectInstance>>(v);
      return "<" + obj->class_name + " object>";
    }
    if (std::holds_alternative<std::shared_ptr<Reference>>(v)) {
      const auto &ref = std::get<std::shared_ptr<Reference>>(v);
      return "<ref " + ref->name + ">";
    }
    return "<unknown>";
  }
};

struct Variable {
  Value value;
  std::string visibility;
};

struct Environment {
  Environment(std::shared_ptr<Environment> parent = nullptr);
  std::optional<Value> get(const std::string &name) const;
  std::optional<Value> get_local(const std::string &name) const;
  void set(const std::string &name, Value val);
  void set_local(const std::string &name, Value val);
  bool has(const std::string &name) const;
  bool has_local(const std::string &name) const;
  bool remove(const std::string &name);
  std::vector<std::string> local_keys() const;
  std::vector<std::string> keys() const;
  std::shared_ptr<Environment> parent;

 private:
  std::unordered_map<std::string, Variable> vars;
};

inline std::optional<Value> Environment::get_local(const std::string &name) const {
  auto it = vars.find(name);
  if (it != vars.end()) return it->second.value;
  return std::nullopt;
}

inline void Environment::set_local(const std::string &name, Value val) {
  vars[name] = Variable{std::move(val), std::string{}};
}

inline std::vector<std::string> Environment::local_keys() const {
  std::vector<std::string> result;
  result.reserve(vars.size());
  for (const auto &entry : vars) {
    result.push_back(entry.first);
  }
  return result;
}

inline std::vector<std::string> Environment::keys() const {
  std::vector<std::string> result = local_keys();
  if (parent) {
    std::vector<std::string> parent_keys = parent->keys();
    result.insert(result.end(), parent_keys.begin(), parent_keys.end());
  }
  return result;
}

inline bool Environment::has_local(const std::string &name) const {
  return vars.find(name) != vars.end();
}

inline bool Environment::has(const std::string &name) const {
  if (vars.find(name) != vars.end()) return true;
  if (parent) return parent->has(name);
  return false;
}

inline bool Environment::remove(const std::string &name) {
  auto it = vars.find(name);
  if (it != vars.end()) {
    vars.erase(it);
    return true;
  }
  if (parent) return parent->remove(name);
  return false;
}

}  // namespace bloa
