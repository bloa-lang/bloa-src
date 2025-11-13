#include "bloa/interpreter.hpp"
#include "bloa/parser.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cctype>
#include <cmath>
#include <stdexcept>

namespace fs = std::filesystem;

namespace bloa {

using NodePtr = std::shared_ptr<Node>;

// ===== VALUE HELPERS (since methods may be missing in header) =====
static std::string value_to_string(const Value& v) {
    if (std::holds_alternative<std::monostate>(v.v)) return "None";
    if (std::holds_alternative<int64_t>(v.v)) return std::to_string(std::get<int64_t>(v.v));
    if (std::holds_alternative<double>(v.v)) {
        double d = std::get<double>(v.v);
        if (std::floor(d) == d) {
            return std::to_string(static_cast<int64_t>(d));
        }
        return std::to_string(d);
    }
    if (std::holds_alternative<std::string>(v.v)) return std::get<std::string>(v.v);
    if (std::holds_alternative<bool>(v.v)) return std::get<bool>(v.v) ? "true" : "false";
    if (std::holds_alternative<std::vector<Value>>(v.v)) {
        std::string out = "[";
        const auto& list = std::get<std::vector<Value>>(v.v);
        for (size_t i = 0; i < list.size(); ++i) {
            if (i > 0) out += ", ";
            out += value_to_string(list[i]);
        }
        out += "]";
        return out;
    }
    return "<unknown>";
}

static bool is_list_value(const Value& v) {
    return std::holds_alternative<std::vector<Value>>(v.v);
}

static double value_as_number(const Value& v) {
    if (std::holds_alternative<int64_t>(v.v)) return static_cast<double>(std::get<int64_t>(v.v));
    if (std::holds_alternative<double>(v.v)) return std::get<double>(v.v);
    if (std::holds_alternative<std::string>(v.v)) {
        const std::string& s = std::get<std::string>(v.v);
        try {
            return std::stod(s);
        } catch (...) {
            // fall through to error
        }
    }
    throw std::runtime_error("Value is not numeric");
}

static bool value_is_true(const Value& v) {
    if (std::holds_alternative<std::monostate>(v.v)) return false;
    if (std::holds_alternative<bool>(v.v)) return std::get<bool>(v.v);
    if (std::holds_alternative<int64_t>(v.v)) return std::get<int64_t>(v.v) != 0;
    if (std::holds_alternative<double>(v.v)) return std::get<double>(v.v) != 0.0;
    if (std::holds_alternative<std::string>(v.v)) return !std::get<std::string>(v.v).empty();
    if (std::holds_alternative<std::vector<Value>>(v.v)) return !std::get<std::vector<Value>>(v.v).empty();
    return false;
}

static const std::vector<Value>& as_list(const Value& v) {
    return std::get<std::vector<Value>>(v.v);
}

// ===== ENVIRONMENT =====
Environment::Environment(std::shared_ptr<Environment> parent_)
    : parent(std::move(parent_)), vars() {}

std::optional<Value> Environment::get(const std::string& name) const {
    auto it = vars.find(name);
    if (it != vars.end()) return it->second;
    if (parent) return parent->get(name);
    return std::nullopt;
}

void Environment::set(const std::string& name, Value val) {
    // Allow initial binding of constants in global env (before any user code)
    // Reject re-assignment (i.e., if already exists and is a constant)
    if (name == "true" || name == "false" || name == "None") {
        // Only allow if not yet defined (e.g., during interpreter init)
        if (vars.find(name) != vars.end()) {
            throw std::runtime_error("Cannot reassign constant '" + name + "'");
        }
        // First-time binding: allow
    }
    vars[name] = std::move(val);
}

// ===== INTERPRETER =====
Interpreter::Interpreter(std::string stdlib_path_, const std::string& source)
    : global_env(std::make_shared<Environment>(nullptr)),
      functions(),
      loaded_modules(),
      stdlib_path(std::move(stdlib_path_)),
      s(source) {

    global_env->set("None", Value());
    global_env->set("true", Value::make_bool(true));
    global_env->set("false", Value::make_bool(false));

    global_env->set("print", Value::make_str("__builtin_print"));
    global_env->set("range", Value::make_str("__builtin_range"));
    global_env->set("len", Value::make_str("__builtin_len"));
    global_env->set("str", Value::make_str("__builtin_str"));
    global_env->set("int", Value::make_str("__builtin_int"));
    global_env->set("float", Value::make_str("__builtin_float"));
    global_env->set("append", Value::make_str("__builtin_append"));
}

NodeList Interpreter::parse(const std::string& source) {
    auto lines = split_lines(source);
    auto res = parse_block(lines, 0, 0);
    return res.first;
}

void Interpreter::run(const std::string& code, const std::string& filename) {
    try {
        auto nodes = parse(code);
        execute_block(nodes, global_env);
    } catch (const std::exception& e) {
        std::cerr << "[BLOA Error] " << e.what() << "\n";
        std::cerr << "  File: " << filename << "\n";
    }
}

static std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, (b - a + 1));
}

static bool is_ident_start(char c) {
    return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
}

static bool is_ident_continue(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '\'';
}

Value Interpreter::parse_expression(std::string expr, std::shared_ptr<Environment> env) {
    struct Parser {
        std::string s;
        size_t pos;
        Interpreter* interp;
        std::shared_ptr<Environment> env;

        Parser(const std::string& str, Interpreter* i, std::shared_ptr<Environment> e)
            : s(str), pos(0), interp(i), env(std::move(e)) {}

        [[noreturn]] void error(const std::string& msg) const {
            throw std::runtime_error(msg);
        }

        void skip_space() {
            while (pos < s.size() && std::isspace(static_cast<unsigned char>(s[pos]))) ++pos;
        }

        bool match(char c) {
            skip_space();
            if (pos < s.size() && s[pos] == c) {
                ++pos;
                return true;
            }
            return false;
        }

        Value parse_primary() {
            skip_space();
            if (pos >= s.size()) error("Unexpected end of expression");

            if (match('(')) {
                Value v = parse_expr();
                if (!match(')')) error("Expected ')'");
                return v;
            }

            if (match('[')) {
                std::vector<Value> elems;
                if (match(']')) return Value::make_list(elems);
                while (true) {
                    elems.push_back(parse_expr());
                    if (match(']')) break;
                    if (!match(',')) error("Expected ',' or ']' in list literal");
                }
                return Value::make_list(elems);
            }

            if (s[pos] == '\"' || s[pos] == '\'') {
                char quote = s[pos++];
                std::string out;
                while (pos < s.size() && s[pos] != quote) {
                    if (s[pos] == '\\' && pos + 1 < s.size()) {
                        ++pos;
                        switch (s[pos]) {
                            case 'n': out.push_back('\n'); break;
                            case 't': out.push_back('\t'); break;
                            case 'r': out.push_back('\r'); break;
                            case '\\': out.push_back('\\'); break;
                            case '\'': out.push_back('\''); break;
                            case '\"': out.push_back('\"'); break;
                            default: out.push_back('\\'); out.push_back(s[pos]); break;
                        }
                        ++pos;
                    } else {
                        out.push_back(s[pos++]);
                    }
                }
                if (pos >= s.size()) error("Unterminated string literal");
                ++pos;
                return Value::make_str(out);
            }

            if (std::isdigit(static_cast<unsigned char>(s[pos])) ||
                (s[pos] == '-' && pos + 1 < s.size() &&
                 std::isdigit(static_cast<unsigned char>(s[pos + 1])))) {
                size_t start = pos;
                if (s[pos] == '-') ++pos;
                while (pos < s.size() && std::isdigit(static_cast<unsigned char>(s[pos]))) ++pos;

                bool is_float = false;
                if (pos < s.size() && s[pos] == '.') {
                    is_float = true;
                    ++pos;
                    while (pos < s.size() && std::isdigit(static_cast<unsigned char>(s[pos]))) ++pos;
                }

                std::string num_str = s.substr(start, pos - start);
                if (num_str.empty() || num_str == "-" || num_str == ".") error("Invalid number");

                try {
                    if (is_float) {
                        double d = std::stod(num_str);
                        return Value::make_double(d);
                    } else {
                        int64_t i = std::stoll(num_str);
                        return Value::make_int(i);
                    }
                } catch (...) {
                    error("Invalid number: '" + num_str + "'");
                }
            }

            if (is_ident_start(s[pos])) {
                size_t start = pos;
                ++pos;
                while (pos < s.size() && is_ident_continue(s[pos])) ++pos;
                std::string id = s.substr(start, pos - start);

                if (id == "true") return Value::make_bool(true);
                if (id == "false") return Value::make_bool(false);
                if (id == "None") return Value();

                auto valopt = env->get(id);
                if (!valopt.has_value()) {
                    error("Name '" + id + "' is not defined");
                }
                Value base_val = std::move(*valopt);

                while (true) {
                    skip_space();

                    if (match('(')) {
                        std::vector<Value> args;
                        if (!match(')')) {
                            while (true) {
                                args.push_back(parse_expr());
                                if (match(')')) break;
                                if (!match(',')) error("Expected ',' or ')' in argument list");
                            }
                        }

                        if (std::holds_alternative<std::string>(base_val.v)) {
                            std::string marker = std::get<std::string>(base_val.v);
                            if (marker == "__builtin_print") {
                                for (size_t i = 0; i < args.size(); ++i) {
                                    if (i > 0) std::cout << ' ';
                                    std::cout << value_to_string(args[i]);
                                }
                                std::cout << '\n';
                                base_val = Value();
                                continue;
                            }
                            if (marker == "__builtin_range") {
                                if (args.size() != 2) error("range() requires 2 arguments");
                                int64_t start = static_cast<int64_t>(value_as_number(args[0]));
                                int64_t stop = static_cast<int64_t>(value_as_number(args[1]));
                                std::vector<Value> list;
                                for (int64_t i = start; i < stop; ++i)
                                    list.push_back(Value::make_int(i));
                                base_val = Value::make_list(list);
                                continue;
                            }
                            if (marker == "__builtin_len") {
                                if (args.size() != 1) error("len() requires 1 argument");
                                const auto& arg = args[0];
                                if (std::holds_alternative<std::string>(arg.v)) {
                                    base_val = Value::make_int(static_cast<int64_t>(std::get<std::string>(arg.v).size()));
                                } else if (is_list_value(arg)) {
                                    base_val = Value::make_int(static_cast<int64_t>(as_list(arg).size()));
                                } else {
                                    error("len() argument must be string or list");
                                }
                                continue;
                            }
                            if (marker == "__builtin_str") {
                                if (args.size() != 1) error("str() requires 1 argument");
                                base_val = Value::make_str(value_to_string(args[0]));
                                continue;
                            }
                            if (marker == "__builtin_int") {
                                if (args.size() != 1) error("int() requires 1 argument");
                                base_val = Value::make_int(static_cast<int64_t>(value_as_number(args[0])));
                                continue;
                            }
                            if (marker == "__builtin_float") {
                                if (args.size() != 1) error("float() requires 1 argument");
                                base_val = Value::make_double(value_as_number(args[0]));
                                continue;
                            }
                            if (marker == "__builtin_append") {
                                if (args.size() != 2) error("append() requires 2 arguments (list, value)");
                                if (!is_list_value(args[0])) error("First argument to append() must be a list");
                                auto list = as_list(args[0]);
                                std::vector<Value> new_list = list;
                                new_list.push_back(args[1]);
                                base_val = Value::make_list(std::move(new_list));
                                continue;
                            }
                        }

                        auto it = interp->functions.find(id);
                        if (it == interp->functions.end()) {
                            error("'" + id + "' is not callable");
                        }
                        const auto& entry = it->second;
                        if (entry.params.size() != args.size()) {
                            error("Function '" + id + "' expects " + std::to_string(entry.params.size()) +
                                  " arguments but got " + std::to_string(args.size()));
                        }

                        auto call_env = std::make_shared<Environment>(entry.def_env);
                        for (size_t i = 0; i < args.size(); ++i) {
                            call_env->set(entry.params[i], args[i]);
                        }
                        try {
                            interp->execute_block(entry.block, call_env);
                        } catch (const std::string&) {}
                        base_val = Value();
                        continue;
                    }

                    if (match('[')) {
                        if (!is_list_value(base_val)) {
                            error("Object is not subscriptable (not a list)");
                        }
                        Value idx_val = parse_expr();
                        if (!match(']')) error("Expected ']'");
                        int64_t idx = static_cast<int64_t>(value_as_number(idx_val));
                        const auto& list = as_list(base_val);
                        if (idx < 0 || idx >= static_cast<int64_t>(list.size())) {
                            error("List index " + std::to_string(idx) + " out of range [0, " +
                                  std::to_string(list.size()) + ")");
                        }
                        base_val = list[static_cast<size_t>(idx)];
                        continue;
                    }

                    break;
                }

                return base_val;
            }

            error("Unexpected token: '" + std::string(1, s[pos]) + "'");
        }

        Value parse_power() {
            Value left = parse_primary();
            while (match('^')) {
                Value right = parse_primary();
                double a = value_as_number(left);
                double b = value_as_number(right);
                left = Value::make_double(std::pow(a, b));
            }
            return left;
        }

        Value parse_term() {
            Value left = parse_power();
            while (true) {
                skip_space();
                if (pos < s.size() && (s[pos] == '*' || s[pos] == '/' || s[pos] == '%')) {
                    char op = s[pos++];
                    Value right = parse_power();
                    double a = value_as_number(left);
                    double b = value_as_number(right);
                    if (op == '*') {
                        left = Value::make_double(a * b);
                    } else if (op == '/') {
                        if (b == 0.0) error("Division by zero");
                        left = Value::make_double(a / b);
                    } else if (op == '%') {
                        if (b == 0.0) error("Modulo by zero");
                        left = Value::make_double(std::fmod(a, b));
                    }
                    continue;
                }
                break;
            }
            return left;
        }

        Value parse_expr() {
            Value left = parse_term();
            while (true) {
                skip_space();
                if (pos < s.size() && (s[pos] == '+' || s[pos] == '-')) {
                    char op = s[pos++];
                    Value right = parse_term();
                    if (op == '+') {
                        if (std::holds_alternative<std::string>(left.v) ||
                            std::holds_alternative<std::string>(right.v)) {
	                        left = Value::make_str(value_to_string(left) + value_to_string(right));
                        } else {
							double a = value_as_number(left);
							double b = value_as_number(right);
                            left = Value::make_double(a + b);
                        }
                    } else {
						double a = value_as_number(left);
						double b = value_as_number(right);
                        left = Value::make_double(a - b);
                    }
                    continue;
                }
                break;
            }
            return left;
        }
    };

    Parser p(expr, this, env);
    return p.parse_expr();
}

Value Interpreter::eval_expr(const std::string& expr, std::shared_ptr<Environment> env) {
    return parse_expression(trim(expr), env);
}

void Interpreter::execute_block(const NodeList& nodes, std::shared_ptr<Environment> env) {
    for (const auto& node : nodes) {
        try {
            if (auto s = std::dynamic_pointer_cast<Say>(node)) {
                Value v = eval_expr(s->expr, env);
                std::cout << value_to_string(v) << '\n';
            } else if (auto a = std::dynamic_pointer_cast<Ask>(node)) {
                Value prompt = eval_expr(a->prompt, env);
                std::cout << value_to_string(prompt) << " ";
                std::string input;
                std::getline(std::cin, input);
                try {
                    std::size_t pos;
                    long long xi = std::stoll(input, &pos);
                    if (pos == input.size()) {
                        env->set(a->var, Value::make_int(xi));
                        continue;
                    }
                    double xd = std::stod(input, &pos);
                    if (pos == input.size()) {
                        env->set(a->var, Value::make_double(xd));
                        continue;
                    }
                } catch (...) {}
                env->set(a->var, Value::make_str(input));
            } else if (auto asg = std::dynamic_pointer_cast<Assign>(node)) {
                Value val = eval_expr(asg->expr, env);
                env->set(asg->name, val);
            } else if (auto iff = std::dynamic_pointer_cast<If>(node)) {
                Value cond = eval_expr(iff->cond, env);
                if (value_is_true(cond)) {
                    execute_block(iff->then_block, std::make_shared<Environment>(env));
                } else if (!iff->else_block.empty()) {
                    execute_block(iff->else_block, std::make_shared<Environment>(env));
                }
            } else if (auto rep = std::dynamic_pointer_cast<Repeat>(node)) {
                Value timesv = eval_expr(rep->times_expr, env);
                int64_t times = static_cast<int64_t>(value_as_number(timesv));
                if (times < 0) throw std::runtime_error("repeat count must be non-negative");
                for (int64_t j = 0; j < times; ++j) {
                    auto loop_env = std::make_shared<Environment>(env);
                    loop_env->set("count", Value::make_int(j + 1));
                    execute_block(rep->block, loop_env);
                }
            } else if (auto fd = std::dynamic_pointer_cast<FunctionDef>(node)) {
                FunctionDefEntry entry;
                entry.params = fd->params;
                entry.block = fd->block;
                entry.def_env = env;
                functions[fd->name] = std::move(entry);
            } else if (auto fc = std::dynamic_pointer_cast<FunctionCall>(node)) {
                std::ostringstream fake_expr;
                fake_expr << fc->name << '(';
                for (size_t i = 0; i < fc->args.size(); ++i) {
                    if (i > 0) fake_expr << ", ";
                    fake_expr << fc->args[i];
                }
                fake_expr << ')';
                eval_expr(fake_expr.str(), env);
            } else if (auto ret = std::dynamic_pointer_cast<Return>(node)) {
                // ignored
            } else if (auto imp = std::dynamic_pointer_cast<Import>(node)) {
                std::string mod = imp->name;
                std::replace(mod.begin(), mod.end(), '\\', fs::path::preferred_separator);
                fs::path p = stdlib_path.empty()
                    ? (fs::path(mod) += ".bloa")
                    : (fs::path(stdlib_path) / mod) += ".bloa";
                if (!fs::exists(p)) {
                    throw std::runtime_error("Module not found: '" + imp->name + "'");
                }
                std::ifstream ifs(p);
                std::string code((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
                auto mod_nodes = parse(code);
                auto mod_env = std::make_shared<Environment>(global_env);
                Interpreter mod_interp("");
                mod_interp.execute_block(mod_nodes, mod_env);
                loaded_modules[imp->name] = mod_env;
                env->set(imp->name, Value::make_str("<module '" + imp->name + "'>"));
            } else if (auto ex = std::dynamic_pointer_cast<ExprStmt>(node)) {
                eval_expr(ex->expr, env);
            } else if (auto wh = std::dynamic_pointer_cast<While>(node)) {
                while (true) {
                    Value cond = eval_expr(wh->cond, env);
                    if (!value_is_true(cond)) break;
                    execute_block(wh->block, std::make_shared<Environment>(env));
                }
            } else if (auto fin = std::dynamic_pointer_cast<ForIn>(node)) {
                Value itv = eval_expr(fin->iterable, env);
                if (!is_list_value(itv)) {
                    throw std::runtime_error("For-in requires a list");
                }
                const auto& list = as_list(itv);
                for (const auto& item : list) {
                    auto loop_env = std::make_shared<Environment>(env);
                    loop_env->set(fin->var, item);
                    execute_block(fin->block, loop_env);
                }
            } else if (auto te = std::dynamic_pointer_cast<TryExcept>(node)) {
                try {
                    execute_block(te->try_block, std::make_shared<Environment>(env));
                } catch (...) {
                    if (!te->except_block.empty()) {
                        execute_block(te->except_block, std::make_shared<Environment>(env));
                    } else {
                        throw;
                    }
                }
            } else {
                throw std::runtime_error("Unknown AST node");
            }
        } catch (const std::exception& e) {
            throw;
        }
    }
}

} // namespace bloa