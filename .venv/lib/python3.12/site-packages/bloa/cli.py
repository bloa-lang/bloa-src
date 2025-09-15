#!/usr/bin/env python3
# bloa/cli.py
import sys
import os
from bloa.interpreter import BLOAInterpreter

USAGE = """BLOA © 2025
Usage:
  bloa run file.bloa
  bloa repl
"""

def main(argv=None):
    if argv is None:
        argv = sys.argv[1:]
    interp = BLOAInterpreter(stdlib_path=os.path.join(os.path.dirname(__file__), "stdlib"))
    if not argv:
        print(USAGE); return 0
    cmd = argv[0]
    if cmd == "run":
        if len(argv) < 2:
            print("Provide a .bloa file to run."); return 1
        path = argv[1]
        with open(path, "r", encoding="utf-8") as f:
            code = f.read()
        interp.run(code, filename=path)
        return 0
    elif cmd == "repl":
        import bloa.repl as repl
        repl.start_repl(interp)
        return 0
    else:
        print(USAGE); return 1

if __name__ == "__main__":
    raise SystemExit(main())