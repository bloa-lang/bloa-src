# bloa-src

A minimalist interpreter for the **bloa** scripting language.

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
```

## Releases

A GitHub Actions workflow (`.github/workflows/release.yml`) builds both architectures and packages `.deb` files when a tag is pushed.
