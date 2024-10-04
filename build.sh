echo "Creating package directory"
mkdir ebus
mkdir ebus/DEBIAN
mkdir ebus/usr
mkdir ebus/usr/lib
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
echo "Finished building"