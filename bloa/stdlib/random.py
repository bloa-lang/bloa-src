import random as _rand

def randint(a: int, b: int) -> int:
    return _rand.randint(a, b)

def randfloat(a: float, b: float) -> float:
    return _rand.uniform(a, b)

def choice(seq):
    return _rand.choice(seq)

def shuffle(seq):
    _rand.shuffle(seq)
    return seq

def sample(seq, k: int):
    return _rand.sample(seq, k)
