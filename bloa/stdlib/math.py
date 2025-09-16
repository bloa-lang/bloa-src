import math as _math

sin = _math.sin
cos = _math.cos
tan = _math.tan
sqrt = _math.sqrt
log = _math.log
exp = _math.exp
pi = _math.pi
e = _math.e

def add(a, b): return a + b
def sub(a, b): return a - b
def mul(a, b): return a * b
def div(a, b):
    if b == 0:
        raise ZeroDivisionError("Division by zero in Bloa math.div()")
    return a / b
