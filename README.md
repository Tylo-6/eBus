# eBus
### An event bus that allows for inter-process communication by emitting and polling events on channels.
## Building
1. Download the repository
2. Build the repository using:
```
bash build.sh
```
3. Install the debian package with:
```
sudo dpkg -i ebus.deb
```
## Using eBus
Include the provided C++ header file located at `dev/ebus.hpp` in your project as follows:
```
#define EBUS_EMITTER // Include if program is an emitter
#define EBUS_LISTENER // Include if program is a listener
#include "ebus.hpp"
```
### Program structure
You must first start by initializing eBus before using any function calls. This can be done as follows:
```
int main() {
    int code = eBusInit();
}
```
The return value of `eBusInit()` is a combination of the following added:
- `1` - Memory was successfully initialized (Required for both emitters and listeners)
- `2` - Emitter was successfully initialized
- `4` - Listener was successfully initialized
---
Once finished with using eBus, you must also close ebus like so:
```
int main() {
    int code = eBusInit();
    
    ...

    eBusClose();
}
```
/!\ Notice: eBusClose() must be called AFTER all eBus objects have been destroyed.
### Emitters
Emitters follow the following syntax:
```
Emitter emitter(const char* channel);
emitter.emit(void* source, size_t size);
```
/!\ Notice: Other processes do not have access to the same memory your process has, so emitting a pointer most likely will result in a segmentation fault.
### Listeners
Listeners follow the following syntax:
```
Listener listener(const char* channel);
int size = listener.poll(void** destination);
```
`Listener::poll()` will return a size of 0 whenever there is no new events. It will also set the pointer pointed to by destination to a new object with the size of the emitted event, which must be deleted using `delete` once finished using to prevent memory leaks.