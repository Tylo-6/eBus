#define EBUS_EMITTER_VERSION 1

#include <iostream>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <semaphore.h>
#include <cerrno>
#include <cstdint>

struct Emitter {
public:
    Emitter(const char* name) {
        sz = (sizeof(void*) * 256) + 1283;

        path += name;
    }
    ~Emitter() {
        if (init) {
            munmap(ptr, sz);
            close(fd);
            sem_destroy(sem);
        }
    }
    bool emit(void*(*post)(void*, void*, size_t), void(*clear)(void*, void*), void* memPtr, void* src, size_t size) {
        if (!init) {
            fd = shm_open(path.c_str(), O_RDWR, 0666);
            if (fd == -1)
                return false;

            if (ftruncate(fd, sz) == -1)
                return false;

            ptr = mmap(NULL, sz, PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0);
            if (ptr == MAP_FAILED)
                return false;

            sem = sem_open(path.c_str(), O_CREAT, 0666, 1);
            init = true;
        }
        sem_wait(sem);
        uint8_t queuePtr = 0;
        memcpy(&queuePtr, (void*)((uintptr_t)ptr + 2), 1);
        uint8_t oldQueuePtr = queuePtr;
        queuePtr++;
        bool kicked = false;
        uint8_t listeners = 0;
        for (int i = 0; i < 256; i++) {
            if (*(int*)((uintptr_t)ptr + (i * 5) + 4) != 0) {
                if (*(uint8_t*)((uintptr_t)ptr + (i * 5) + 3) == queuePtr) {
                    kicked = true;
                    *(int*)((uintptr_t)ptr + (i * 5) + 4) = 0;
                } else {
                    listeners++;
                }
            }
        }
        if (kicked) {
            bool first = true;
            uint8_t newTail = *(uint8_t*)((uintptr_t)ptr + 1);
            uint8_t atTail = 0;
            while (newTail != oldQueuePtr || first) {
                atTail = 0;
                for (int i = 0; i < 256; i++) {
                    if (*(int*)((uintptr_t)ptr + (i * 5) + 4) != 0) {
                        if (*(uint8_t*)((uintptr_t)ptr + (i * 5) + 3) == newTail) {
                            atTail++;
                        }
                    }
                }
                if (atTail == 0) {
                    void* polledPos = nullptr;
                    memcpy(&polledPos, (void*)((uintptr_t)ptr + ((uintptr_t)newTail * sizeof(void*)) + 1283), sizeof(void*));
                    clear(memPtr, polledPos);
                } else {
                    break;
                }
                newTail++;
                first = false;
            }
            *(uint8_t*)(ptr) = atTail;
            *(uint8_t*)((uintptr_t)ptr + 1) = newTail;
        }
        if (listeners > 0) {
            void* data = post(memPtr, src, size);
            if (data == nullptr) {
                sem_post(sem);
                return false;
            } else {
                memcpy((void*)((uintptr_t)ptr + ((uintptr_t)oldQueuePtr * sizeof(void*)) + 1283), &data, sizeof(void*));
                memcpy((void*)((uintptr_t)ptr + 2), &queuePtr, 1);
            }
        }
        sem_post(sem);
        return true;
    }
private:
    bool init = false;
    std::string path = "/ebus::";
    size_t sz;
    sem_t* sem;
    int fd;
    void* ptr;
};
extern "C" void* createEmitter(const char* name) {
    return new Emitter(name);
}
extern "C" void destroyEmitter(void* ptr) {
    delete (Emitter*)ptr;
}
extern "C" bool emit(void*(*post)(void*, void*, size_t), void(*clear)(void*, void*), void* memPtr, void* ptr, void* src, size_t size) {
    return ((Emitter*)ptr)->emit(post, clear, memPtr, src, size);
}
extern "C" uint32_t getVersion() {
    return EBUS_EMITTER_VERSION;
}