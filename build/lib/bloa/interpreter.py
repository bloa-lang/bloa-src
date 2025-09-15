# interpreter.py
import ast
import os
import sys

class ParseError(Exception): pass
class ReturnSignal(Exception):
    def __init__(self, value):
        self.value = value

class Node:
    pass

# AST node classes
class Say(Node):
    def __init__(self, expr): self.expr = expr

class Ask(Node):
    def __init__(self, prompt, var): self.prompt = prompt; self.var = var

class Assign(Node):
    def __init__(self, name, expr): self.name = name; self.expr = expr

class If(Node):
    def __init__(self, cond, then_block, else_block): self.cond=cond; self.then_block=then_block; self.else_block=else_block

class Repeat(Node):
    def __init__(self, times_expr, block): self.times_expr=times_expr; self.block=block

class FunctionDef(Node):
    def __init__(self, name, params, block): self.name=name; self.params=params; self.block=block

class FunctionCall(Node):
    def __init__(self, name, args): self.name=name; self.args=args

class Return(Node):
    def __init__(self, expr): self.expr=expr

class Import(Node):
    def __init__(self, name): self.name=name

class ExprStmt(Node):
    def __init__(self, expr): self.expr = expr

# --- simple lexer/parser based on indentation ---
def split_lines(code):
    # preserve \n lines, normalize to LF
    return code.replace("\r\n", "\n").replace("\r", "\n").split("\n")

def indent_level(line):
    # count leading spaces; treat tab as 4 spaces
    count = 0
    for ch in line:
        if ch == ' ': count += 1
        elif ch == '\t': count += 4
        else: break
    return count

def parse_block(lines, start_idx=0, base_indent=0):
    idx = start_idx
    nodes = []
    while idx < len(lines):
        raw = lines[idx]
        if not raw.strip():
            idx += 1; continue
        indent = indent_level(raw)
        if indent < base_indent:
            break
        if indent > base_indent:
            raise ParseError(f"Unexpected indent at line {idx+1}: {raw!r}")
        line = raw.strip()
        # comments
        if line.startswith("#"):
            idx += 1; continue

        # say
        if line.startswith("say "):
            expr = line[4:].strip()
            nodes.append(Say(expr))
            idx += 1; continue

        # ask "prompt" -> var
        if line.startswith("ask "):
            rest = line[4:].strip()
            if "->" not in rest:
                raise ParseError(f"Invalid ask syntax at line {idx+1}")
            prompt, var = rest.split("->", 1)
            nodes.append(Ask(prompt.strip(), var.strip()))
            idx += 1; continue

        # import
        if line.startswith("import "):
            name = line[len("import "):].strip()
            nodes.append(Import(name))
            idx += 1; continue

        # if ...:
        if line.startswith("if ") and line.endswith(":"):
            cond = line[3:-1].strip()
            then_block, next_idx = parse_block(lines, idx+1, base_indent+4)
            # check for else directly following at same indent
            else_block = []
            if next_idx < len(lines):
                nxt = lines[next_idx]
                if indent_level(nxt) == base_indent and nxt.strip().startswith("else:"):
                    else_block, after_else = parse_block(lines, next_idx+1, base_indent+4)
                    idx = after_else
                else:
                    idx = next_idx
            else:
                idx = next_idx
            nodes.append(If(cond, then_block, else_block))
            continue

        # else: should be handled by if; if appears alone it's an error
        if line.startswith("else:"):
            # signal to caller to handle
            break

        # repeat N times:
        if line.startswith("repeat ") and line.endswith(" times:"):
            times_part = line[len("repeat "):-len(" times:")].strip()
            block, next_idx = parse_block(lines, idx+1, base_indent+4)
            nodes.append(Repeat(times_part, block))
            idx = next_idx
            continue

        # function def
        if line.startswith("function ") and line.endswith(":"):
            header = line[len("function "):-1].strip()
            if "(" not in header or not header.endswith(")"):
                raise ParseError(f"Invalid function header at line {idx+1}")
            name, params_raw = header.split("(",1)
            name = name.strip()
            params = [p.strip() for p in params_raw[:-1].split(",") if p.strip()]
            block, next_idx = parse_block(lines, idx+1, base_indent+4)
            nodes.append(FunctionDef(name, params, block))
            idx = next_idx
            continue

        # return
        if line.startswith("return "):
            nodes.append(Return(line[len("return "):].strip()))
            idx += 1; continue

        # assignment?
        if "=" in line and not line.startswith("==") and not line.startswith("if"):
            # but avoid equality comparisons disguised, simple heuristic
            parts = line.split("=", 1)
            left = parts[0].strip()
            right = parts[1].strip()
            # only treat as assign if left is a name (simple)
            if left.replace("_","").isalnum():
                nodes.append(Assign(left, right))
                idx += 1; continue

        # function call or expression
        # detect something like name(arg1, arg2)
        if "(" in line and line.endswith(")"):
            name, args_raw = line.split("(",1)
            name = name.strip()
            args = [a.strip() for a in args_raw[:-1].split(",") if a.strip()]
            nodes.append(FunctionCall(name, args))
            idx += 1; continue

        # default: treat as expression stmt
        nodes.append(ExprStmt(line))
        idx += 1

    return nodes, idx

# --- Runtime / evaluator ---
class Environment:
    def __init__(self, parent=None):
        self.vars = {}
        self.parent = parent

    def get(self, name):
        if name in self.vars: return self.vars[name]
        if self.parent: return self.parent.get(name)
        raise NameError(f"Name '{name}' is not defined")

    def set(self, name, value):
        self.vars[name] = value

    def has(self, name):
        if name in self.vars: return True
        if self.parent: return self.parent.has(name)
        return False

class BLOAInterpreter:
    def __init__(self, stdlib_path=None):
        self.global_env = Environment()
        self.functions = {}   # name -> (params, block, env-at-definition)
        self.stdlib_path = stdlib_path

        # add builtins
        self.global_env.set("print", lambda *a: print(*a))
        self.global_env.set("len", lambda x: len(x))
        # we keep raw python functions minimal

    def run(self, code, filename="<string>"):
        lines = split_lines(code)
        ast_nodes, _ = parse_block(lines, 0, 0)
        try:
            self.execute_block(ast_nodes, self.global_env)
        except ReturnSignal as r:
            # top-level return ignored
            pass

    def execute_block(self, nodes, env):
        for node in nodes:
            self.execute(node, env)

    def eval_expr(self, expr, env):
        # expr may contain {var} formatting, try to substitute
        if isinstance(expr, (int, float, list, dict)):
            return expr
        s = expr
        # replace {var} placeholders
        # do multiple passes to allow nested replacements
        for _ in range(3):
            for name in list(env.vars.keys()):
                s = s.replace("{" + name + "}", repr(env.get(name)))
        # try literal eval (numbers, strings, lists, dicts, tuples)
        try:
            return ast.literal_eval(s)
        except:
            pass
        # otherwise, attempt to eval arithmetic/boolean expressions using env vars
        # build local dict from env chain
        local = {}
        cur = env
        while cur:
            local.update(cur.vars)
            cur = cur.parent
        # restrict builtins
        allowed_builtins = {}
        try:
            return eval(s, {"__builtins__": allowed_builtins}, local)
        except Exception as e:
            # as fallback, if it's a bare variable name, return its value
            n = s.strip()
            if n and n.replace("_","").isalnum():
                try:
                    return env.get(n)
                except:
                    pass
            raise RuntimeError(f"Failed to evaluate expression '{expr}': {e}")

    def execute(self, node, env):
        if isinstance(node, Say):
            val = self.eval_expr(node.expr, env)
            # if val is a list/dict, pretty print
            print(val)
        elif isinstance(node, Ask):
            prompt = node.prompt.strip().strip('"')
            raw = input(prompt + " ")
            # try to parse as literal, else keep string
            try:
                v = ast.literal_eval(raw)
            except:
                v = raw
            env.set(node.var, v)
        elif isinstance(node, Assign):
            value = self.eval_expr(node.expr, env)
            env.set(node.name, value)
        elif isinstance(node, If):
            cond = self.eval_expr(node.cond, env)
            if cond:
                self.execute_block(node.then_block, Environment(parent=env))
            else:
                if node.else_block:
                    self.execute_block(node.else_block, Environment(parent=env))
        elif isinstance(node, Repeat):
            times = self.eval_expr(node.times_expr, env)
            if not isinstance(times, int):
                raise RuntimeError("repeat times must be integer")
            for i in range(times):
                env.set("count", i+1)
                self.execute_block(node.block, Environment(parent=env))
        elif isinstance(node, FunctionDef):
            # store function with closure env
            self.functions[node.name] = (node.params, node.block, env)
        elif isinstance(node, FunctionCall):
            # built-in call?
            if env.has(node.name):
                # call python callable
                func = env.get(node.name)
                args = [self.eval_expr(a, env) for a in node.args]
                if callable(func):
                    return func(*args)
            # user-defined?
            if node.name in self.functions:
                params, block, def_env = self.functions[node.name]
                if len(params) != len(node.args):
                    raise RuntimeError(f"Function {node.name} expects {len(params)} args")
                call_env = Environment(parent=def_env)
                for p, aexpr in zip(params, node.args):
                    call_env.set(p, self.eval_expr(aexpr, env))
                try:
                    self.execute_block(block, call_env)
                    return None
                except ReturnSignal as r:
                    return r.value
            else:
                raise NameError(f"Function or name '{node.name}' not found")
        elif isinstance(node, Return):
            val = self.eval_expr(node.expr, env) if node.expr else None
            raise ReturnSignal(val)
        elif isinstance(node, Import):
            # search stdlib and load file
            if not self.stdlib_path:
                raise RuntimeError("No stdlib path configured")
            path = os.path.join(self.stdlib_path, node.name + ".bloa")
            if not os.path.isfile(path):
                raise RuntimeError(f"Library {node.name} not found in stdlib path")
            with open(path, "r", encoding="utf-8") as f:
                code = f.read()
            # create a new interpreter instance to parse the lib into its own module env
            module_env = Environment(parent=self.global_env)
            # execute lib code inside module_env
            lines = split_lines(code)
            ast_nodes, _ = parse_block(lines, 0, 0)
            # run lib code. library typically defines functions and assigns
            self.execute_block(ast_nodes, module_env)
            # expose module as object in current env: map names to a dict-like proxy
            module_proxy = ModuleProxy(module_env)
            env.set(node.name, module_proxy)
        elif isinstance(node, ExprStmt):
            # evaluate expr for side effect
            self.eval_expr(node.expr, env)
        else:
            raise RuntimeError("Unknown AST node: " + str(type(node)))

class ModuleProxy:
    def __init__(self, env):
        self._env = env
    def __getattr__(self, item):
        try:
            return self._env.get(item)
        except NameError as e:
            raise AttributeError(str(e))