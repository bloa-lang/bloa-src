import time as _time

def now() -> float:
    return _time.time()

def sleep(seconds: float):
    _time.sleep(seconds)

def format_time(fmt="%Y-%m-%d %H:%M:%S"):
    return _time.strftime(fmt, _time.localtime())
