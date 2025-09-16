import ast
import os
import sys
from typing import Any, List, Optional, Tuple, Dict

class ParseError(Exception):
    pass

class ReturnSignal(Exception):
    def __init__(self, value: Any):
        self.value = value

class Node:
    pass

class Say(Node):
    def __init__(self, expr: str): self.expr = expr

class Ask(Node):
    def __init__(self, prompt: str, var: str): self.prompt = prompt; self.var = var

class Assign(Node):
    def __init__(self, name: str, expr: str): self.name = name; self.expr = expr

class If(Node):
    def __init__(self, cond: str, then_block: List[Node], else_block: List[Node]):
        self.cond = cond; self.then_block = then_block; self.else_block = else_block

class Repeat(Node):
    def __init__(self, times_expr: str, block: List[Node]):
        self.times_expr = times_expr; self.block = block

class FunctionDef(Node):
    def __init__(self, name: str, params: List[str], block: List[Node]):
        self.name = name; self.params = params; self.block = block

class FunctionCall(Node):
    def __init__(self, name: str, args: List[str]): self.name = name; self.args = args

class Return(Node):
    def __init__(self, expr: Optional[str]): self.expr = expr

class Import(Node):
    def __init__(self, name: str): self.name = name

class ExprStmt(Node):
    def __init__(self, expr: str): self.expr = expr

def split_lines(code: str) -> List[str]:
    return code.replace("\r\n", "\n").replace("\r", "\n").split("\n")

def indent_level(line: str) -> int:
    count = 0
    for ch in line:
        if ch == ' ': count += 1
        elif ch == '\t': count += 4
        else: break
    return count

def parse_block(lines: List[str], start_idx: int = 0, base_indent: int = 0) -> Tuple[List[Node], int]:
    idx = start_idx
    nodes = []
    while idx < len(lines):
        raw_line = lines[idx]
        if not raw_line.strip() or raw_line.strip().startswith("#"):
            idx += 1
            continue
        indent = indent_level(raw_line)
        if indent < base_indent:
            break
        if indent > base_indent:
            raise ParseError(f"Unexpected indent at line {idx + 1}: {raw_line!r}")
        line = raw_line.strip()
        STATEMENT_RULES = [
            ("say ", lambda l: Say(l[4:].strip())),
            ("ask ", lambda l: Ask(*[p.strip() for p in l[4:].strip().split("->", 1)]) if "->" in l else None),
            ("import ", lambda l: Import(l[len("import "):].strip())),
            ("return ", lambda l: Return(l[len("return "):].strip())),
        ]
        matched_node = None
        for prefix, handler in STATEMENT_RULES:
            if line.startswith(prefix):
                node_result = handler(line)
                if node_result is None:
                    raise ParseError(f"Invalid syntax at line {idx + 1}: {line!r}")
                matched_node = node_result
                idx += 1
                break
        if matched_node:
            nodes.append(matched_node)
            continue
        if line.startswith("if ") and line.endswith(":"):
            cond = line[3:-1].strip()
            then_block, next_idx = parse_block(lines, idx + 1, base_indent + 4)
            else_block = []
            if next_idx < len(lines) and indent_level(lines[next_idx]) == base_indent and lines[next_idx].strip() == "else:":
                else_block, next_idx = parse_block(lines, next_idx + 1, base_indent + 4)
            nodes.append(If(cond, then_block, else_block))
            idx = next_idx
            continue
        if line.startswith("repeat ") and line.endswith(" times:"):
            times_part = line[len("repeat "):-len(" times:")].strip()
            block, next_idx = parse_block(lines, idx + 1, base_indent + 4)
            nodes.append(Repeat(times_part, block))
            idx = next_idx
            continue
        if line.startswith("function ") and line.endswith(":"):
            header = line[len("function "):-1].strip()
            if "(" not in header or not header.endswith(")"):
                raise ParseError(f"Invalid function header at line {idx+1}")
            name, params_raw = header.split("(", 1)
            name = name.strip()
            params = [p.strip() for p in params_raw[:-1].split(",") if p.strip()]
            block, next_idx = parse_block(lines, idx + 1, base_indent + 4)
            nodes.append(FunctionDef(name, params, block))
            idx = next_idx
            continue
        if line == "else:":
            raise ParseError(f"Unexpected 'else:' at line {idx + 1}")
        if "=" in line and not (line.strip().startswith("==") or " if " in line):
            parts = line.split("=", 1)
            left, right = parts[0].strip(), parts[1].strip()
            if left.replace("_", "").isalnum():
                nodes.append(Assign(left, right))
                idx += 1
                continue
        if "(" in line and line.endswith(")"):
            name, args_raw = line.split("(", 1)
            name = name.strip()
            if name.isidentifier():
                args = [a.strip() for a in args_raw[:-1].split(",") if a.strip()]
                nodes.append(FunctionCall(name, args))
                idx += 1
                continue
        nodes.append(ExprStmt(line))
        idx += 1
    return nodes, idx

class Environment:
    def __init__(self, parent: Optional['Environment'] = None):
        self.vars: Dict[str, Any] = {}
        self.parent = parent
    def get(self, name: str) -> Any:
        if name in self.vars:
            return self.vars[name]
        if self.parent:
            return self.parent.get(name)
        raise NameError(f"Name '{name}' is not defined")
    def set(self, name: str, value: Any):
        self.vars[name] = value
    def get_all_vars(self) -> Dict[str, Any]:
        all_vars = {}
        if self.parent:
            all_vars.update(self.parent.get_all_vars())
        all_vars.update(self.vars)
        return all_vars

class ModuleProxy:
    def __init__(self, env: Environment, functions: Dict):
        self._env = env
        self._functions = functions
    def __getattr__(self, item: str) -> Any:
        if item in self._functions:
            return self._functions[item]
        try:
            return self._env.get(item)
        except NameError as e:
            raise AttributeError(str(e))

class BLOAInterpreter:
    def __init__(self, stdlib_path: Optional[str] = None):
        self.global_env = Environment()
        self.functions: Dict[str, Tuple[List[str], List[Node], Environment]] = {}
        self.loaded_modules: Dict[str, ModuleProxy] = {}
        self.stdlib_path = stdlib_path
        self.global_env.set("print", print)
        self.global_env.set("len", len)
        self.global_env.set("int", int)
        self.global_env.set("str", str)
        self.global_env.set("float", float)
    def run(self, code: str, filename: str = "<string>"):
        try:
            lines = split_lines(code)
            ast_nodes, _ = parse_block(lines)
            self.execute_block(ast_nodes, self.global_env)
        except (ParseError, RuntimeError, NameError, TypeError) as e:
            print(f"Error in {filename}: {e}", file=sys.stderr)
        except ReturnSignal:
            pass
    def execute_block(self, nodes: List[Node], env: Environment):
        for node in nodes:
            self.execute(node, env)
    def eval_expr(self, expr: str, env: Environment) -> Any:
        if not isinstance(expr, str): return expr
        try:
            return ast.literal_eval(expr)
        except (ValueError, SyntaxError):
            pass
        try:
            local_vars = env.get_all_vars()
            return eval(expr, {"__builtins__": {}}, local_vars)
        except Exception as e:
            clean_expr = expr.strip()
            if clean_expr.isidentifier():
                try:
                    return env.get(clean_expr)
                except NameError:
                    pass
            raise RuntimeError(f"Failed to evaluate expression '{expr}': {e}")
    def execute(self, node: Node, env: Environment) -> Optional[Any]:
        node_type = type(node)
        if node_type is Say:
            val = self.eval_expr(node.expr, env)
            print(val)
        elif node_type is Ask:
            prompt = self.eval_expr(node.prompt, env)
            raw_input = input(str(prompt) + " ")
            try:
                val = ast.literal_eval(raw_input)
            except (ValueError, SyntaxError):
                val = raw_input
            env.set(node.var, val)
        elif node_type is Assign:
            value = self.eval_expr(node.expr, env)
            env.set(node.name, value)
        elif node_type is If:
            cond = self.eval_expr(node.cond, env)
            if cond:
                self.execute_block(node.then_block, Environment(parent=env))
            elif node.else_block:
                self.execute_block(node.else_block, Environment(parent=env))
        elif node_type is Repeat:
            times = self.eval_expr(node.times_expr, env)
            if not isinstance(times, int) or times < 0:
                raise RuntimeError(f"repeat times must be a non-negative integer, got {times!r}")
            loop_env = Environment(parent=env)
            for i in range(times):
                loop_env.set("count", i + 1)
                self.execute_block(node.block, loop_env)
        elif node_type is FunctionDef:
            self.functions[node.name] = (node.params, node.block, env)
        elif node_type is FunctionCall:
            try:
                func = env.get(node.name)
                if callable(func):
                    args = [self.eval_expr(a, env) for a in node.args]
                    return func(*args)
            except NameError:
                pass
            if node.name in self.functions:
                params, block, def_env = self.functions[node.name]
                if len(params) != len(node.args):
                    raise TypeError(f"Function {node.name} expects {len(params)} arguments, but got {len(node.args)}")
                call_env = Environment(parent=def_env)
                for p, a_expr in zip(params, node.args):
                    call_env.set(p, self.eval_expr(a_expr, env))
                try:
                    self.execute_block(block, call_env)
                    return None
                except ReturnSignal as r:
                    return r.value
            raise NameError(f"Function or name '{node.name}' not found")
        elif node_type is Return:
            val = self.eval_expr(node.expr, env) if node.expr else None
            raise ReturnSignal(val)
        elif node_type is Import:
            if node.name in self.loaded_modules:
                env.set(node.name, self.loaded_modules[node.name])
                return
            if not self.stdlib_path:
                raise RuntimeError("Standard library path is not configured.")
            path = os.path.join(self.stdlib_path, node.name + ".bloa")
            if not os.path.isfile(path):
                raise RuntimeError(f"Library '{node.name}' not found in stdlib path: {self.stdlib_path}")
            with open(path, "r", encoding="utf-8") as f:
                code = f.read()
            module_interpreter = BLOAInterpreter()
            module_env = Environment(parent=self.global_env)
            module_lines = split_lines(code)
            module_ast, _ = parse_block(module_lines)
            module_interpreter.execute_block(module_ast, module_env)
            module_proxy = ModuleProxy(module_env, module_interpreter.functions)
            self.loaded_modules[node.name] = module_proxy
            env.set(node.name, module_proxy)
        elif node_type is ExprStmt:
            self.eval_expr(node.expr, env)
        else:
            raise RuntimeError(f"Unknown AST node type: {node_type.__name__}")
