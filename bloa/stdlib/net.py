import urllib.request

def http_get(url: str) -> str:
    with urllib.request.urlopen(url) as response:
        return response.read().decode("utf-8")

def http_download(url: str, path: str):
    with urllib.request.urlopen(url) as response:
        data = response.read()
        with open(path, "wb") as f:
            f.write(data)
