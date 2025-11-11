#include "bloa/interpreter.hpp"
#include "bloa/parser.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cctype>

namespace fs = std::filesystem;

namespace bloa {

using NodePtr = std::shared_ptr<Node>;

Environment::Environment(std::shared_ptr<Environment> parent_): parent(parent_), vars() {}
std::optional<Value> Environment::get(const std::string &name) const {
    auto it = vars.find(name);
    if (it!=vars.end()) return it->second;
    if (parent) return parent->get(name);
    return std::nullopt;
}
void Environment::set(const std::string &name, Value val) {
    vars[name] = std::move(val);
}

std::string Value::to_string() const {
    if (std::holds_alternative<std::monostate>(v)) return "None";
    if (std::holds_alternative<int64_t>(v)) return std::to_string(std::get<int64_t>(v));
    if (std::holds_alternative<double>(v)) return std::to_string(std::get<double>(v));
    if (std::holds_alternative<std::string>(v)) return std::get<std::string>(v);
    if (std::holds_alternative<bool>(v)) return std::get<bool>(v) ? "true" : "false";
    if (std::holds_alternative<std::vector<Value>>(v)) {
        std::string out = "[";
        bool first=true;
        for (auto &x : std::get<std::vector<Value>>(v)) {
            if (!first) out += ", ";
            out += x.to_string();
            first = false;
        }
        out += "]";
        return out;
    }
    return "Unknown";
}

Interpreter::Interpreter(std::string stdlib_path_): global_env(std::make_shared<Environment>(nullptr)), functions(), loaded_modules(), stdlib_path(std::move(stdlib_path_)), s(std::string()) {
    // expose some builtins
    global_env->set("print", Value::make_str("__builtin_print")); // marker, handled specially
    global_env->set("True", Value::make_bool(true));
    global_env->set("False", Value::make_bool(false));
}

NodeList Interpreter::parse(const std::string &source) {
    auto lines = split_lines(source);
    auto res = parse_block(lines, 0, 0);
    return res.first;
}

void Interpreter::run(const std::string &code, const std::string &filename) {
    try {
        auto nodes = parse(code);
        execute_block(nodes, global_env);
    } catch (const std::exception &e) {
        std::cerr << "Error in " << filename << ": " << e.what() << std::endl;
    }
}

// A tiny tokenizer for expression parsing
static bool is_ident_char(char c) { return std::isalnum((unsigned char)c) || c=='_' || c=='\\\'' || c=='\\'; }
static bool is_space(char c) { return c==' ' || c=='\t' || c=='\r' || c=='\n'; }

static std::string trim(const std::string &s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a==std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b-a+1);
}

// Forward declare
Value Interpreter::parse_expression(std::string expr, std::shared_ptr<Environment> env) {
    // We'll implement a recursive descent parser for +,-,*,/, parentheses, numbers, strings, identifiers, and function calls
    struct Parser {
        std::string s;
        size_t pos;
        Interpreter *interp;
        std::shared_ptr<Environment> env;
        Parser(const std::string &str, Interpreter *i, std::shared_ptr<Environment> e): s(str), pos(0), interp(i), env(e) {}
        void skip_space(){ while (pos < s.size() && isspace((unsigned char)s[pos])) pos++; }
        bool match(char c){ skip_space(); if (pos<s.size() && s[pos]==c){ pos++; return true;} return false;}
        std::string parse_ident() {
            skip_space();
            size_t start = pos;
            if (pos<s.size() && (isalpha((unsigned char)s[pos]) || s[pos]=='_')) {
                pos++;
                while (pos<s.size() && (isalnum((unsigned char)s[pos]) || s[pos]=='_')) pos++;
                return s.substr(start, pos-start);
            }
            return {};
        }
        Value parse_primary(){
            skip_space();
            if (pos>=s.size()) throw std::runtime_error("Unexpected end of expression");
            if (s[pos]=='(') {
                pos++;
                Value v = parse_expr();
                skip_space();
                if (pos>=s.size() || s[pos]!=')') throw std::runtime_error("Expected )");
                pos++;
                return v;
            }
            if (s[pos]=='\"' || s[pos]=='\'') {
                char quote = s[pos++];
                std::string out;
                while (pos<s.size() && s[pos]!=quote) {
                    if (s[pos]=='\\' && pos+1<s.size()) { out.push_back(s[pos+1]); pos+=2; }
                    else out.push_back(s[pos++]);
                }
                if (pos>=s.size() || s[pos]!=quote) throw std::runtime_error("Unterminated string literal");
                pos++;
                return Value::make_str(out);
            }
            // number?
            if (isdigit((unsigned char)s[pos]) || (s[pos]=='-' && pos+1<s.size() && isdigit((unsigned char)s[pos+1]))) {
                size_t start = pos;
                if (s[pos]=='-') pos++;
                while (pos<s.size() && isdigit((unsigned char)s[pos])) pos++;
                bool is_double = false;
                if (pos<s.size() && s[pos]=='.') { is_double = true; pos++; while (pos<s.size() && isdigit((unsigned char)s[pos])) pos++; }
                std::string num = s.substr(start, pos-start);
                if (is_double) return Value::make_double(std::stod(num));
                return Value::make_int(std::stoll(num));
            }
            // identifier or function call or boolean
            if (isalpha((unsigned char)s[pos]) || s[pos]=='_') {
                std::string id;
                size_t start = pos;
                pos++;
                while (pos<s.size() && (isalnum((unsigned char)s[pos]) || s[pos]=='_')) pos++;
                id = s.substr(start, pos-start);
                // boolean literals
                if (id=="true") return Value::make_bool(true);
                if (id=="false") return Value::make_bool(false);
                // function call?
                skip_space();
                if (pos<s.size() && s[pos]=='(') {
                    pos++; // consume '('
                    std::vector<Value> args;
                    while (true) {
                        skip_space();
                        if (pos<s.size() && s[pos]==')') { pos++; break; }
                        // parse argument expression by finding sub-expression until comma at top level or closing paren
                        size_t arg_start = pos;
                        int depth = 0;
                        while (pos<s.size()) {
                            if (s[pos]=='(') { depth++; pos++; continue; }
                            if (s[pos]==')') { if (depth==0) break; depth--; pos++; continue; }
                            if (s[pos]==',' && depth==0) break;
                            pos++;
                        }
                        std::string argstr = s.substr(arg_start, pos-arg_start);
                        args.push_back(interp->parse_expression(trim(argstr), env));
                        if (pos<s.size() && s[pos]==',') { pos++; continue; }
                        if (pos<s.size() && s[pos]==')') { pos++; break; }
                    }
                    // call function: first look in environment for callable marker (we simplified builtins)
                    auto maybe = env->get(id);
                    if (maybe.has_value()) {
                        Value val = maybe.value();
                        // handle built-in 'print' by special marker string
                        if (std::holds_alternative<std::string>(val.v) && std::get<std::string>(val.v)=="__builtin_print") {
                            // print all args
                            for (size_t i=0;i<args.size();++i) {
                                if (i) std::cout << ' ';
                                std::cout << args[i].to_string();
                            }
                            std::cout << std::endl;
                            return Value();
                        }
                    }
                    // interpreter-defined functions
                    auto it = interp->functions.find(id);
                    if (it!=interp->functions.end()) {
                        auto &entry = it->second;
                        if (entry.params.size() != args.size()) throw std::runtime_error("Function arg count mismatch");
                        auto call_env = std::make_shared<Environment>(entry.def_env);
                        for (size_t i=0;i<args.size();++i) call_env->set(entry.params[i], args[i]);
                        try {
                            interp->execute_block(entry.block, call_env);
                            return Value();
                        } catch (const std::string &ret) {
                            // not used
                        } catch (...) {
                            // return void
                            return Value();
                        }
                    }
                    throw std::runtime_error("Function or name '" + id + "' not found");
                }
                // else identifier lookup
                auto valopt = env->get(id);
                if (valopt.has_value()) return valopt.value();
                throw std::runtime_error("Name '" + id + "' is not defined");
            }
            throw std::runtime_error("Unexpected token in expression at pos " + std::to_string(pos));
        }
        Value parse_term(){
            Value left = parse_primary();
            while (true) {
                skip_space();
                if (pos < s.size() && (s[pos]=='*' || s[pos]=='/')) {
                    char op = s[pos++];
                    Value right = parse_primary();
                    // arithmetic on numbers only (int/double)
                    double a = 0, b = 0;
                    bool a_double=false, b_double=false;
                    if (std::holds_alternative<int64_t>(left.v)) { a = (double)std::get<int64_t>(left.v); }
                    else if (std::holds_alternative<double>(left.v)) { a = std::get<double>(left.v); a_double=true; }
                    if (std::holds_alternative<int64_t>(right.v)) { b = (double)std::get<int64_t>(right.v); }
                    else if (std::holds_alternative<double>(right.v)) { b = std::get<double>(right.v); b_double=true; }
                    double res = (op=='*') ? (a*b) : (a/b);
                    if (!a_double && !b_double && op=='*') left = Value::make_int((int64_t)res);
                    else left = Value::make_double(res);
                    continue;
                }
                break;
            }
            return left;
        }
        Value parse_expr(){
            Value left = parse_term();
            while (true) {
                skip_space();
                if (pos < s.size() && (s[pos]=='+' || s[pos]=='-')) {
                    char op = s[pos++];
                    Value right = parse_term();
                    double a=0,b=0; bool a_double=false,b_double=false;
                    if (std::holds_alternative<int64_t>(left.v)) a=(double)std::get<int64_t>(left.v); else if (std::holds_alternative<double>(left.v)) { a=std::get<double>(left.v); a_double=true; }
                    if (std::holds_alternative<int64_t>(right.v)) b=(double)std::get<int64_t>(right.v); else if (std::holds_alternative<double>(right.v)) { b=std::get<double>(right.v); b_double=true; }
                    double res = (op=='+') ? (a+b) : (a-b);
                    if (!a_double && !b_double && (op=='+')) left = Value::make_int((int64_t)res);
                    else left = Value::make_double(res);
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

Value Interpreter::eval_expr(const std::string &expr, std::shared_ptr<Environment> env) {
    return parse_expression(trim(expr), env);
}

void Interpreter::execute_block(const NodeList &nodes, std::shared_ptr<Environment> env) {
    for (auto &node : nodes) {
        if (auto s = std::dynamic_pointer_cast<Say>(node)) {
            Value v = eval_expr(s->expr, env);
            std::cout << v.to_string() << std::endl;
        } else if (auto a = std::dynamic_pointer_cast<Ask>(node)) {
            Value v = eval_expr(a->prompt, env);
            std::cout << v.to_string() << " "; std::string input; std::getline(std::cin, input);
            // try parse number
            try {
                long long xi = std::stoll(input);
                env->set(a->var, Value::make_int(xi));
            } catch(...) {
                env->set(a->var, Value::make_str(input));
            }
        } else if (auto asg = std::dynamic_pointer_cast<Assign>(node)) {
            Value val = eval_expr(asg->expr, env);
            env->set(asg->name, val);
        } else if (auto iff = std::dynamic_pointer_cast<If>(node)) {
            Value cond = eval_expr(iff->cond, env);
            if (cond.is_true()) {
                execute_block(iff->then_block, std::make_shared<Environment>(env));
            } else if (!iff->else_block.empty()) {
                execute_block(iff->else_block, std::make_shared<Environment>(env));
            }
        } else if (auto rep = std::dynamic_pointer_cast<Repeat>(node)) {
            Value timesv = eval_expr(rep->times_expr, env);
            int times = 0;
            if (std::holds_alternative<int64_t>(timesv.v)) times = (int)std::get<int64_t>(timesv.v);
            if (times < 0) throw std::runtime_error("repeat times must be non-negative");
            for (int i=0;i<times;i++) {
                auto loop_env = std::make_shared<Environment>(env);
                loop_env->set("count", Value::make_int(i+1));
                execute_block(rep->block, loop_env);
            }
        } else if (auto fd = std::dynamic_pointer_cast<FunctionDef>(node)) {
            FunctionDefEntry entry;
            entry.params = fd->params;
            entry.block = fd->block;
            entry.def_env = env;
            functions[fd->name] = std::move(entry);
        } else if (auto fc = std::dynamic_pointer_cast<FunctionCall>(node)) {
            // attempt builtin or env callable (we treat print as builtin marker)
            auto maybe = env->get(fc->name);
            if (maybe.has_value()) {
                Value v = maybe.value();
                if (std::holds_alternative<std::string>(v.v) && std::get<std::string>(v.v)=="__builtin_print") {
                    // evaluate args and print
                    for (size_t i=0;i<fc->args.size();++i) {
                        Value av = eval_expr(fc->args[i], env);
                        if (i) std::cout << ' ';
                        std::cout << av.to_string();
                    }
                    std::cout << std::endl;
                    continue;
                }
            }
            // interpreter-defined functions
            auto it = functions.find(fc->name);
            if (it!=functions.end()) {
                auto &entry = it->second;
                if (entry.params.size() != fc->args.size()) throw std::runtime_error("Function arg count mismatch");
                auto call_env = std::make_shared<Environment>(entry.def_env);
                for (size_t i=0;i<fc->args.size();++i) call_env->set(entry.params[i], eval_expr(fc->args[i], env));
                execute_block(entry.block, call_env);
                continue;
            }
            throw std::runtime_error("Function or name '" + fc->name + "' not found");
        } else if (auto ret = std::dynamic_pointer_cast<Return>(node)) {
            if (ret->expr.has_value()) {
                // Not implementing cross-function return exception; returns ignored in this minimal interpreter
                // Could throw a custom ReturnException with Value
            }
        } else if (auto imp = std::dynamic_pointer_cast<Import>(node)) {
            // import: support backslashes in path. module name may be like a\b\c
            std::string mod = imp->name;
            for (char &c : mod) if (c=='\\') c = fs::path::preferred_separator;
            fs::path p = std::filesystem::path(stdlib_path).empty() ? fs::path(mod + std::string(".bloa")) : fs::path(stdlib_path) / mod;
            if (!p.has_extension()) p.replace_extension(".bloa");
            if (!fs::exists(p)) throw std::runtime_error("Library '" + imp->name + "' not found: " + p.string());
            std::ifstream ifs(p);
            std::stringstream ss; ss << ifs.rdbuf();
            auto nodes = parse(ss.str());
            auto module_env = std::make_shared<Environment>(global_env);
            Interpreter module_interpreter(std::string()); // empty stdlib path for module
            module_interpreter.execute_block(nodes, module_env);
            loaded_modules[imp->name] = module_env;
            // store module in current env by its name
            env->set(imp->name, Value::make_str("<module>")); // placeholder
        } else if (auto ex = std::dynamic_pointer_cast<ExprStmt>(node)) {
            eval_expr(ex->expr, env);
        } else if (auto wh = std::dynamic_pointer_cast<While>(node)) {
            while (eval_expr(wh->cond, env).is_true()) {
                execute_block(wh->block, std::make_shared<Environment>(env));
            }
        } else if (auto fin = std::dynamic_pointer_cast<ForIn>(node)) {
            Value itv = eval_expr(fin->iterable, env);
            if (!std::holds_alternative<std::vector<Value>>(itv.v)) throw std::runtime_error("Object is not iterable");
            for (auto &item : std::get<std::vector<Value>>(itv.v)) {
                auto loop_env = std::make_shared<Environment>(env);
                loop_env->set(fin->var, item);
                execute_block(fin->block, loop_env);
            }
        } else if (auto te = std::dynamic_pointer_cast<TryExcept>(node)) {
            try {
                execute_block(te->try_block, std::make_shared<Environment>(env));
            } catch (...) {
                if (!te->except_block.empty()) execute_block(te->except_block, std::make_shared<Environment>(env));
                else throw;
            }
        } else {
            throw std::runtime_error("Unknown AST node type encountered");
        }
    }
}

} // namespace bloa
