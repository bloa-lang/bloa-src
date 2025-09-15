# repl.py
import readline
import sys

PROMPT = "bloa> "

def start_repl(interpreter):
    print("BLOA REPL. Type 'exit' or Ctrl-D to quit.")
    buffer = []
    while True:
        try:
            line = input(PROMPT)
        except (EOFError, KeyboardInterrupt):
            print(); break
        if line.strip() in ("exit", "quit"):
            break
        if line.strip() == "":
            # execute buffer if present
            if buffer:
                code = "\n".join(buffer)
                try:
                    interpreter.run(code, "<repl>")
                except Exception as e:
                    print("Error:", e)
                buffer = []
            continue
        buffer.append(line)