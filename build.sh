#!/bin/bash
pkgver=1.0.0

if command -v dpkg-deb &> /dev/null; then
    echo "Creating debian package directory"
    mkdir -p ebus
    mkdir -p ebus/DEBIAN
    mkdir -p ebus/usr
    mkdir -p ebus/usr/lib
    cp control ebus/DEBIAN/control
    echo "Compiling memory.so"
    g++ -fPIC -shared -o ebus/usr/lib/memory.so src/memory.cpp
    echo "Compiling emitter.so"
    g++ -fPIC -shared -o ebus/usr/lib/emitter.so src/emitter.cpp
    echo "Compiling listener.so"
    g++ -fPIC -shared -o ebus/usr/lib/listener.so src/listener.cpp
    echo "Setting file permissions"
    chmod 0755 ebus/DEBIAN
    chmod 0644 ebus/DEBIAN/control
    echo "Building ebus.deb"
    dpkg-deb --build ebus
    echo "Finished building debian package"
fi

echo "Creating AUR package directory"
AUR=ebus-v$pkgver-release-x86_64
mkdir -p $AUR
echo "Building ebus-bin AUR package"
echo "Copying license"
cp LICENSE $AUR/LICENSE
echo "Compiling memory.so"
g++ -fPIC -shared -o $AUR/memory.so src/memory.cpp
echo "Compiling emitter.so"
g++ -fPIC -shared -o $AUR/emitter.so src/emitter.cpp
echo "Compiling listener.so"
g++ -fPIC -shared -o $AUR/listener.so src/listener.cpp
echo "Creating archive"
tar -czvf $AUR.tar.gz $AUR/
echo "Finished building AUR package"
