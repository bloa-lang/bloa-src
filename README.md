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

## Releases

A GitHub Actions workflow (`.github/workflows/release.yml`) builds both architectures and packages `.deb` files when a tag is pushed.
