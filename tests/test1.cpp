#define EBUS_LISTENER
#include "ebus.hpp"
#include <chrono>
#include <thread>

int main() {
    eBusInit();
    {
        Listener listen("test");
        char* data;
        int loop = 0;
        while (loop < 100) {
            uint32_t size = listen.poll(&data);
            if (size > 0) {
                std::cout << size << ": " << data << std::endl;
                loop++;
            }
        }
    }
    eBusClose();
}