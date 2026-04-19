# bloa-src

A minimalist interpreter for the **bloa** scripting language.

## Features

- Variables and expressions (arithmetic, logic, comparison)
- Control flow: if/else, while, foreach/for-in, repeat, break, continue, try/except
- Functions with parameters
- Classes with methods and inheritance (`extends`)
- Modules: `use` for importing, `require` for including files
- Built-in functions: print, range, len, str, int, float, append, ref, deref, set_ref, is_ref
- Lists, strings, numbers, booleans
- I/O: say (print), ask (input)
- `echo`, `isset`, `unset`
- `new` object creation for Java-style instantiation
- BAAR archive support for `.baar` packages and built-in `baar_*` helpers
- cURL support: `curl_get`, `curl_post`, `curl_request`
- SQLite utilities: `sqlite_query`, `sqlite_exec`
- Exception handling: try/except
- Standard library in `stdlib/` (math, io, string)

## Syntax Examples
### Comments
```
// This is a single-line comment
say "Hello"
/* This is a
   block comment */
say "World"
```

### Pointer-like references
```
x = 42
p = ref("x")
say deref(p)
set_ref(p, 100)
say x
say is_ref(p)
```

### Prefix reference operators
```
y = 10
p = &y
say *p
set_ref(p, 20)
say y
```
### Variables and Expressions
```
x = 42
y = x + 10
say "Result: " + str(y)
```

### Control Flow
```
if (x > 10) {
  say "big"
} else {
  say "small"
}

foreach (range(0, 5) as value) {
  if (value == 3) {
    continue;
  }
  if (value == 4) {
    break;
  }
  say value
}

try {
  read_file("missing.txt")
} except {
  say "Failed to read file"
}
```

### Functions
```
function greet(name) {
  say "Hello, " + name
}

greet("World")
```

### Classes and Inheritance
```
class Animal {
  function speak(self) {
    say "Animal sound"
  }
}

class Dog extends Animal {
  function speak(self) {
    say "Woof!"
  }
}

d = Dog()
d.speak()
```

### Java-style object construction
```
class Counter {
  function __init__(self, start) {
    self.value = start
  }
  function inc(self) {
    self.value = self.value + 1
  }
}

c = new Counter(10)
c.inc()
say c.value
```

### BAAR packages and archives
```
files = [
  ["main.bloa", "say \"Hello from archive\"\n"],
  ["data.txt", "archive payload"]
]
baar_create("app.baar", files)
list = baar_list("app.baar")
say list
content = baar_read("app.baar", "data.txt")
say content
```

A `.baar` file can also be executed directly with `bloa app.baar`. The interpreter will choose `main.bloa`, `index.bloa`, the first `.bloa` entry, or the first file in the archive.

### HTTP/cURL helpers
```
response = curl_get("https://example.com/data")
json = curl_post("https://example.com/api", "payload")
response = curl_request("https://example.com/api", "PUT", "body")
```

### SQLite helpers
```
rows = sqlite_query("data.db", "SELECT id, name FROM users")
for (rows as row) {
  say row[0] + ": " + row[1]
}
sqlite_exec("data.db", "CREATE TABLE IF NOT EXISTS users(id INTEGER, name TEXT)")
```

### Modules
```
use math;
say "Square root of 4: " + str(sqrt(4))
```

## Standard Library

The standard library is built-in and automatically available, providing functions without requiring external files:

### Math Functions
- `sqrt(x)`: Square root
- `pow(base, exp)`: Power
- `sin(x)`, `cos(x)`, `tan(x)`: Trigonometric functions
- `log(x)`, `exp(x)`: Logarithm and exponential
- `abs(x)`: Absolute value
- `floor(x)`, `ceil(x)`, `round(x)`: Rounding functions
- `pi()`, `e()`: Mathematical constants

### I/O Functions
- `read_file(path)`: Read file content as string
- `write_file(path, content)`: Write string to file
- `exists(path)`: Check if file/directory exists
- `list_dir(path)`: List directory contents as list of strings
- `mkdir(path)`: Create directory
- `rmdir(path)`: Remove directory
- `remove(path)`: Remove file
- `copy_file(from, to)`: Copy file
- `move(from, to)`: Move/rename file
- `file_size(path)`: Get file size in bytes
- `is_dir(path)`: Check if path is a directory

### String Functions
- `len(s)`: String length (built-in)
- `split(s, delim)`: Split string by delimiter
- `join(list, sep)`: Join list of strings with separator
- `substr(s, start, len?)`: Substring
- `find(s, sub)`: Find substring position
- `replace(s, old, new)`: Replace all occurrences
- `to_upper(s)`, `to_lower(s)`: Case conversion
- `trim(s)`: Remove leading/trailing whitespace
- `starts_with(s, prefix)`: Check if string starts with prefix
- `ends_with(s, suffix)`: Check if string ends with suffix
- `contains(s, sub)`: Check if string contains substring
- `reverse(s)`: Reverse string
- `repeat(s, n)`: Repeat string n times

### Utility Functions
- `random_int(max)` or `random_int(min, max)`: Random integer
- `random_float(max)` or `random_float(min, max)`: Random float
- `now()`: Current timestamp in milliseconds

All functions are available globally.

```bloa
say "Square root of 16: " + str(sqrt(16))
say "Pi value: " + str(pi())
say "Uppercase: " + to_upper("hello")
```

## Building

```sh
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
```

To create Debian packages for amd64 and i386 (requires multilib tools):

```sh
# amd64
mkdir -p package_amd64/DEBIAN package_amd64/usr/local/bin
cp build/bloa package_amd64/usr/local/bin/
# edit control fields then
chmod 0755 package_amd64/DEBIAN
sudo dpkg-deb --build package_amd64 bloa_0.2.0-alpha_amd64.deb

# i386 (after installing g++-multilib libc6-dev-i386)
rm -rf build32 && mkdir build32 && cd build32
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_FLAGS=-m32 -DCMAKE_CXX_FLAGS=-m32 ..
cmake --build .
mkdir -p package_i386/DEBIAN package_i386/usr/local/bin
cp bloa ../package_i386/usr/local/bin/
# edit control & build
chmod 0755 package_i386/DEBIAN
sudo dpkg-deb --build package_i386 bloa_0.2.0-alpha_i386.deb
# all architecture (use when you just want a generic package, e.g. containing
# scripts or the prebuilt binary for the current host; the package is marked
# "Architecture: all" so dpkg will install it regardless of machine type)
mkdir -p package_all/DEBIAN package_all/usr/local/bin
# copy whatever payload makes sense; we'll just include amd64 binary here as an
# example, but you can replace it with a shell wrapper or source archive
cp build/bloa package_all/usr/local/bin/
cat > package_all/DEBIAN/control <<'EOF'
Package: bloa
Version: 0.2.0-alpha
Section: utils
Priority: optional
Architecture: all
Maintainer: bloa <noreply@local>
Description: bloa language runtime (architecture independent "all" package)
EOF
chmod 0755 package_all/DEBIAN
sudo dpkg-deb --build package_all bloa_0.2.0-alpha_all.deb```

```

## Releases

This is the `1.0.0-RC1` release candidate of BLOA.

A GitHub Actions workflow (`.github/workflows/release.yml`) builds both architectures and packages `.deb` files when a tag is pushed.

## Older changelogs

See `older changelogs/1.0.0.md` for release notes on version 1.0.0-RC1.
