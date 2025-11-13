#pragma once
#include "bloa/ast.hpp"
#include "bloa/env.hpp"
#include <string>
#include <unordered_map>
#include <functional>

namespace bloa {

class Interpreter {
public:
    Interpreter(std::string stdlib_path = "", const std::string &source = "");
    NodeList parse(const std::string &source);
    void run(const std::string &code, const std::string &filename = "<string>");
    Value eval_expr(const std::string &expr, std::shared_ptr<Environment> env);
    void execute_block(const NodeList &nodes, std::shared_ptr<Environment> env);
private:
    std::shared_ptr<Environment> global_env;
    struct FunctionDefEntry { std::vector<std::string> params; NodeList block; std::shared_ptr<Environment> def_env; };
    std::unordered_map<std::string, FunctionDefEntry> functions;
    std::unordered_map<std::string, std::shared_ptr<Environment>> loaded_modules;
    std::string stdlib_path;

    // helpers for expression parsing
    const std::string &s;
    size_t pos = 0;
    // parse expression helpers (implemented in .cpp)
    Value parse_expression(std::string expr, std::shared_ptr<Environment> env);
};

} // namespace bloa
