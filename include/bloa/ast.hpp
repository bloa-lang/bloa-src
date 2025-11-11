#pragma once
#include <string>
#include <vector>
#include <memory>
#include <variant>

namespace bloa {

struct Node;
using NodePtr = std::shared_ptr<Node>;
using NodeList = std::vector<NodePtr>;

struct Node {
    virtual ~Node() = default;
};

struct Say : Node { std::string expr; Say(std::string e): expr(std::move(e)){} };
struct Ask : Node { std::string prompt; std::string var; Ask(std::string p, std::string v): prompt(std::move(p)), var(std::move(v)){} };
struct Assign : Node { std::string name; std::string expr; Assign(std::string n, std::string e): name(std::move(n)), expr(std::move(e)){} };
struct If : Node { std::string cond; NodeList then_block; NodeList else_block; If(std::string c, NodeList t, NodeList e): cond(std::move(c)), then_block(std::move(t)), else_block(std::move(e)){} };
struct Repeat : Node { std::string times_expr; NodeList block; Repeat(std::string t, NodeList b): times_expr(std::move(t)), block(std::move(b)){} };
struct FunctionDef : Node { std::string name; std::vector<std::string> params; NodeList block; FunctionDef(std::string n, std::vector<std::string> p, NodeList b): name(std::move(n)), params(std::move(p)), block(std::move(b)){} };
struct FunctionCall : Node { std::string name; std::vector<std::string> args; FunctionCall(std::string n, std::vector<std::string> a): name(std::move(n)), args(std::move(a)){} };
struct Return : Node { std::optional<std::string> expr; Return(std::optional<std::string> e): expr(std::move(e)){} };
struct Import : Node { std::string name; Import(std::string n): name(std::move(n)){} };
struct ExprStmt : Node { std::string expr; ExprStmt(std::string e): expr(std::move(e)){} };

struct While : Node { std::string cond; NodeList block; While(std::string c, NodeList b): cond(std::move(c)), block(std::move(b)){} };
struct Break : Node {};
struct Continue : Node {};
struct ForIn : Node { std::string var; std::string iterable; NodeList block; ForIn(std::string v, std::string it, NodeList b): var(std::move(v)), iterable(std::move(it)), block(std::move(b)){} };
struct TryExcept : Node { NodeList try_block; NodeList except_block; TryExcept(NodeList t, NodeList e): try_block(std::move(t)), except_block(std::move(e)){} };

} // namespace bloa
