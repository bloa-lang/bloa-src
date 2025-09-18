import ast
import os
import sys

from typing import Any
from typing import List
from typing import Optional
from typing import Tuple
from typing import Dict


# -------- Exceptions / Signals --------
class ParseError(Exception):
    pass


class ReturnSignal(Exception):
    def __init__(self, value: Any):
        self.value = value


class BreakSignal(Exception):
    pass


class ContinueSignal(Exception):
    pass


# -------- AST Node classes --------
class Node:
    pass


class Say(Node):
    def __init__(self, expr: str):
        self.expr = expr


class Ask(Node):
    def __init__(self, prompt: str, var: str):
        self.prompt = prompt
        self.var = var


class Assign(Node):
    def __init__(self, name: str, expr: str):
        self.name = name
        self.expr = expr


class If(Node):
    def __init__(self, cond: str, then_block: List[Node], else_block: List[Node]):
        self.cond = cond
        self.then_block = then_block
        self.else_block = else_block


class Repeat(Node):
    def __init__(self, times_expr: str, block: List[Node]):
        self.times_expr = times_expr
        self.block = block


class FunctionDef(Node):
    def __init__(self, name: str, params: List[str], block: List[Node]):
        self.name = name
        self.params = params
        self.block = block


class FunctionCall(Node):
    def __init__(self, name: str, args: List[str]):
        self.name = name
        self.args = args


class Return(Node):
    def __init__(self, expr: Optional[str]):
        self.expr = expr


class Import(Node):
    def __init__(self, name: str):
        self.name = name


class ExprStmt(Node):
    def __init__(self, expr: str):
        self.expr = expr


class While(Node):
    def __init__(self, cond: str, block: List[Node]):
        self.cond = cond
        self.block = block


class Break(Node):
    pass


class Continue(Node):
    pass


class ForIn(Node):
    def __init__(self, var: str, iterable: str, block: List[Node]):
        self.var = var
        self.iterable = iterable
        self.block = block


class TryExcept(Node):
    def __init__(self, try_block: List[Node], except_block: List[Node]):
        self.try_block = try_block
        self.except_block = except_block

def split_lines(code: str) -> List[str]:
    return code.replace("\r\n", "\n").replace("\r", "\n").split("\n")


def indent_level(line: str) -> int:
    count = 0
    for ch in line:
        if ch == " ":
            count += 1
        elif ch == "\t":
            count += 4
        else:
            break
    return count


# -------- Parser --------
def parse_block(lines: List[str], start_idx: int = 0, base_indent: int = 0) -> Tuple[List[Node], int]:
    idx = start_idx
    nodes: List[Node] = []

    while idx < len(lines):
        raw_line = lines[idx]

        # Skip blank lines and comments
        if not raw_line.strip() or raw_line.strip().startswith("#"):
            idx += 1
            continue

        indent = indent_level(raw_line)

        # End of this block
        if indent < base_indent:
            break

        # Unexpected additional indent (must be handled by caller)
        if indent > base_indent:
            raise ParseError(f"Unexpected indent at line {idx + 1}: {raw_line!r}")

        line = raw_line.strip()

        # Simple statements
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

        # control structures: if
        if line.startswith("if ") and line.endswith(":"):
            cond = line[3:-1].strip()
            then_block, next_idx = parse_block(lines, idx + 1, base_indent + 4)
            else_block: List[Node] = []
            if next_idx < len(lines) and indent_level(lines[next_idx]) == base_indent and lines[next_idx].strip() == "else:":
                else_block, next_idx = parse_block(lines, next_idx + 1, base_indent + 4)
            nodes.append(If(cond, then_block, else_block))
            idx = next_idx
            continue

        # repeat ... times:
        if line.startswith("repeat ") and line.endswith(" times:"):
            times_part = line[len("repeat "):-len(" times:")].strip()
            block, next_idx = parse_block(lines, idx + 1, base_indent + 4)
            nodes.append(Repeat(times_part, block))
            idx = next_idx
            continue

        # function ...
        if line.startswith("function ") and line.endswith(":"):
            header = line[len("function "):-1].strip()
            if "(" not in header or not header.endswith(")"):
                raise ParseError(f"Invalid function header at line {idx + 1}")
            name, params_raw = header.split("(", 1)
            name = name.strip()
            params = [p.strip() for p in params_raw[:-1].split(",") if p.strip()]
            block, next_idx = parse_block(lines, idx + 1, base_indent + 4)
            nodes.append(FunctionDef(name, params, block))
            idx = next_idx
            continue

        if line == "else:":
            raise ParseError(f"Unexpected 'else:' at line {idx + 1}")

        # while
        if line.startswith("while ") and line.endswith(":"):
            cond = line[6:-1].strip()
            block, next_idx = parse_block(lines, idx + 1, base_indent + 4)
            nodes.append(While(cond, block))
            idx = next_idx
            continue

        # break / continue
        if line == "break":
            nodes.append(Break())
            idx += 1
            continue

        if line == "continue":
            nodes.append(Continue())
            idx += 1
            continue

        # for-in
        if line.startswith("for ") and " in " in line and line.endswith(":"):
            header = line[4:-1].strip()
            var, iterable = header.split(" in ", 1)
            block, next_idx = parse_block(lines, idx + 1, base_indent + 4)
            nodes.append(ForIn(var.strip(), iterable.strip(), block))
            idx = next_idx
            continue

        # try/except
        if line == "try:":
            try_block, next_idx = parse_block(lines, idx + 1, base_indent + 4)
            except_block: List[Node] = []
            if next_idx < len(lines) and indent_level(lines[next_idx]) == base_indent and lines[next_idx].strip() == "except:":
                except_block, next_idx = parse_block(lines, next_idx + 1, base_indent + 4)
            nodes.append(TryExcept(try_block, except_block))
            idx = next_idx
            continue

        # assignment (simple)
        if "=" in line and not (line.strip().startswith("==") or " if " in line):
            parts = line.split("=", 1)
            left, right = parts[0].strip(), parts[1].strip()
            if left.replace("_", "").isalnum():
                nodes.append(Assign(left, right))
                idx += 1
                continue

        # function call form: name(arg1, arg2)
        if "(" in line and line.endswith(")"):
            name, args_raw = line.split("(", 1)
            name = name.strip()
            if name.isidentifier():
                args = [a.strip() for a in args_raw[:-1].split(",") if a.strip()]
                nodes.append(FunctionCall(name, args))
                idx += 1
                continue

        # fallback: expression statement
        nodes.append(ExprStmt(line))
        idx += 1

    return nodes, idx


# -------- Environment / Module Proxy --------
class Environment:
    def __init__(self, parent: Optional["Environment"] = None):
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

    def update(self, name: str, value: Any):
        if name in self.vars:
            self.vars[name] = value
            return
        if self.parent:
            try:
                self.parent.update(name, value)
                return
            except NameError:
                pass
        self.vars[name] = value

    def get_all_vars(self) -> Dict[str, Any]:
        all_vars: Dict[str, Any] = {}
        if self.parent:
            all_vars.update(self.parent.get_all_vars())
        all_vars.update(self.vars)
        return all_vars


class ModuleProxy:
    def __init__(self, env: Environment, functions: Dict[str, Tuple[List[str], List[Node], Environment]]):
        self._env = env
        self._functions = functions

    def __getattr__(self, item: str) -> Any:
        # functions first
        if item in self._functions:
            def wrapper(*args, **kwargs):
                params, block, def_env = self._functions[item]
                call_env = Environment(parent=def_env)
                # map args positionally
                for p, a in zip(params, args):
                    call_env.set(p, a)
                # evaluate kwargs into env if provided (simple support)
                for k, v in kwargs.items():
                    call_env.set(k, v)
                inter = BLOAInterpreter(stdlib_path=None)
                # execute block in call_env - do not re-run imports in module context here
                inter.execute_block(block, call_env)
            return wrapper
        # otherwise variables from module env
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

        # expose safe builtins into global_env
        self.global_env.set("print", print)
        self.global_env.set("len", len)
        self.global_env.set("int", int)
        self.global_env.set("str", str)
        self.global_env.set("float", float)
        self.global_env.set("range", range)

        # constants
        self.global_env.set("True", True)
        self.global_env.set("False", False)
        self.global_env.set("None", None)

    def parse(self, source: str) -> List[Node]:
        lines = split_lines(source)
        ast_nodes, _ = parse_block(lines)
        return ast_nodes

    def run(self, code: str, filename: str = "<string>"):
        try:
            nodes = self.parse(code)
            self.execute_block(nodes, self.global_env)
        except (ParseError, RuntimeError, NameError, TypeError) as e:
            print(f"Error in {filename}: {e}", file=sys.stderr)
        except ReturnSignal:
            # top-level return ignored
            pass

    def execute_block(self, nodes: List[Node], env: Environment):
        for node in nodes:
            self.execute(node, env)

    def _preprocess_expr(self, expr: str) -> str:
        # convert literal true/false to Python True/False
        if not isinstance(expr, str):
            return expr
        cleaned = expr.strip()
        if cleaned.lower() == "true":
            return "True"
        if cleaned.lower() == "false":
            return "False"
        return expr

    def eval_expr(self, expr: str, env: Environment) -> Any:
        if not isinstance(expr, str):
            return expr

        expr = expr.strip()

        # attempt literal eval (numbers, strings, lists, dicts, tuples)
        try:
            return ast.literal_eval(expr)
        except Exception:
            pass

        # boolean literals
        lowered = expr.lower()
        if lowered == "true":
            return True
        if lowered == "false":
            return False

        # prepare locals and safe builtins
        local_vars = env.get_all_vars()
        safe_builtins = {}
        for name, val in self.global_env.get_all_vars().items():
            if name in ("print", "len", "int", "str", "float", "range", "True", "False", "None"):
                safe_builtins[name] = val

        try:
            expr_to_eval = self._preprocess_expr(expr)
            return eval(expr_to_eval, {"__builtins__": safe_builtins}, local_vars)
        except Exception as e:
            # last resort: if single identifier, return variable
            if expr.isidentifier():
                try:
                    return env.get(expr)
                except NameError:
                    pass
            raise RuntimeError(f"Failed to evaluate expression '{expr}': {e}")

    def execute(self, node: Node, env: Environment) -> Optional[Any]:
        """
        Execute a single AST node in the provided environment.
        """
        node_type = type(node)

        if node_type is Say:
            val = self.eval_expr(node.expr, env)
            print(val)
            return None

        if node_type is Ask:
            prompt = self.eval_expr(node.prompt, env)
            raw_input_text = input(str(prompt) + " ")
            try:
                val = ast.literal_eval(raw_input_text)
            except (ValueError, SyntaxError):
                val = raw_input_text
            env.set(node.var, val)
            return None

        if node_type is Assign:
            value = self.eval_expr(node.expr, env)
            env.set(node.name, value)
            return None

        if node_type is If:
            cond = self.eval_expr(node.cond, env)
            if cond:
                self.execute_block(node.then_block, Environment(parent=env))
            elif node.else_block:
                self.execute_block(node.else_block, Environment(parent=env))
            return None

        if node_type is Repeat:
            times = self.eval_expr(node.times_expr, env)
            if not isinstance(times, int) or times < 0:
                raise RuntimeError(f"repeat times must be a non-negative integer, got {times!r}")
            for i in range(times):
                loop_env = Environment(parent=env)
                loop_env.set("count", i + 1)
                try:
                    self.execute_block(node.block, loop_env)
                except BreakSignal:
                    break
                except ContinueSignal:
                    # continue outer loop
                    continue
            return None

        if node_type is FunctionDef:
            # store params + block + closure env
            self.functions[node.name] = (node.params, node.block, env)
            return None

        if node_type is FunctionCall:
            # try call builtin or env-defined callable (like print)
            try:
                func = env.get(node.name)
                if callable(func):
                    args = [self.eval_expr(a, env) for a in node.args]
                    return func(*args)
            except NameError:
                pass

            # interpreter-defined functions
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

        if node_type is Return:
            val = self.eval_expr(node.expr, env) if node.expr else None
            raise ReturnSignal(val)

        if node_type is Import:
            # If module already loaded, set proxy in env and return
            if node.name in self.loaded_modules:
                env.set(node.name, self.loaded_modules[node.name])
                return None
            if not self.stdlib_path:
                raise RuntimeError("Standard library path is not configured.")
            path = os.path.join(self.stdlib_path, node.name + ".bloa")
            if not os.path.isfile(path):
                # allow .py fallback for convenience
                alt_path_py = os.path.join(self.stdlib_path, node.name + ".py")
                if os.path.isfile(alt_path_py):
                    path = alt_path_py
                else:
                    raise RuntimeError(f"Library '{node.name}' not found in stdlib path: {self.stdlib_path}")
            with open(path, "r", encoding="utf-8") as f:
                code = f.read()
            # create interpreter for module (shared global env as parent)
            module_interpreter = BLOAInterpreter(stdlib_path=self.stdlib_path)
            module_env = Environment(parent=self.global_env)
            module_ast, _ = parse_block(split_lines(code))
            # execute module top-level to populate module_env
            module_interpreter.execute_block(module_ast, module_env)
            module_proxy = ModuleProxy(module_env, module_interpreter.functions)
            self.loaded_modules[node.name] = module_proxy
            env.set(node.name, module_proxy)
            return None

        if node_type is ExprStmt:
            # evaluate for side effects (e.g. a function call)
            self.eval_expr(node.expr, env)
            return None

        if node_type is While:
            # while loop: evaluate condition each iteration
            while self.eval_expr(node.cond, env):
                loop_env = Environment(parent=env)
                try:
                    self.execute_block(node.block, loop_env)
                except BreakSignal:
                    break
                except ContinueSignal:
                    continue
            return None

        if node_type is Break:
            raise BreakSignal()

        if node_type is Continue:
            raise ContinueSignal()

        if node_type is ForIn:
            iterable = self.eval_expr(node.iterable, env)
            if iterable is None:
                raise RuntimeError(f"Iterable expression evaluated to None: {node.iterable!r}")
            if not hasattr(iterable, "__iter__"):
                raise RuntimeError(f"Object {iterable!r} is not iterable")
            for item in iterable:
                loop_env = Environment(parent=env)
                loop_env.set(node.var, item)
                try:
                    self.execute_block(node.block, loop_env)
                except BreakSignal:
                    break
                except ContinueSignal:
                    continue
            return None

        if node_type is TryExcept:
            try:
                self.execute_block(node.try_block, Environment(parent=env))
            except Exception:
                if node.except_block:
                    self.execute_block(node.except_block, Environment(parent=env))
                else:
                    # re-raise if no except block provided
                    raise
            return None

        # unknown node
        raise RuntimeError(f"Unknown AST node type: {node_type.__name__}")

def example_usage():
    """
    Example showing features:
    - variable assignment
    - say / ask
    - functions
    - loops
    - import (requires stdlib_path and module files)
    """
    code = """
say "Hello from BLOA!"
x = 0
repeat 3 times:
    say x
    x = x + 1

function greet(name):
    say "Hello, " + name

greet("World")
"""
    interpreter = BLOAInterpreter()
    interpreter.run(code, filename="<example>")


if __name__ == "__main__":
    # Run example when executed directly
    example_usage()
