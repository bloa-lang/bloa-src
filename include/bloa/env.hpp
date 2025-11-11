#pragma once
#include <string>
#include <unordered_map>
#include <memory>
#include <variant>
#include <vector>
#include <optional>

namespace bloa {

struct Value;
using ValuePtr = std::shared_ptr<Value>;

struct Value {
    // simple variant: int64, double, std::string, bool, vector<Value>, nullptr
    std::variant<std::monostate, int64_t, double, std::string, bool, std::vector<Value>> v;
    Value() = default;
    static Value make_int(int64_t i){ Value val; val.v = i; return val; }
    static Value make_double(double d){ Value val; val.v = d; return val; }
    static Value make_str(std::string s){ Value val; val.v = std::move(s); return val; }
    static Value make_bool(bool b){ Value val; val.v = b; return val; }
    static Value make_list(std::vector<Value> list){ Value val; val.v = std::move(list); return val; }
    bool is_true() const {
        if (std::holds_alternative<std::monostate>(v)) return false;
        if (std::holds_alternative<bool>(v)) return std::get<bool>(v);
        if (std::holds_alternative<int64_t>(v)) return std::get<int64_t>(v) != 0;
        if (std::holds_alternative<double>(v)) return std::get<double>(v) != 0.0;
        if (std::holds_alternative<std::string>(v)) return !std::get<std::string>(v).empty();
        if (std::holds_alternative<std::vector<Value>>(v)) return !std::get<std::vector<Value>>(v).empty();
        return false;
    }
    std::string to_string() const;
};

struct Environment {
    Environment(std::shared_ptr<Environment> parent = nullptr);
    std::optional<Value> get(const std::string &name) const;
    void set(const std::string &name, Value val);
    std::shared_ptr<Environment> parent;
private:
    std::unordered_map<std::string, Value> vars;
};

} // namespace bloa
