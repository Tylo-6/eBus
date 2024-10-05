#define EBUS_LISTENER_VERSION 1

#include <iostream>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <semaphore.h>
#include <cerrno>
#include <cstdint>

struct Listener {
public:
    Listener(const char* name) {
        sz = (sizeof(void*) * 256) + 1283;

        std::string path = "/ebus::";
        path += name;
        
        fd = shm_open(path.c_str(), O_CREAT | O_RDWR | O_EXCL, 0666);
        if (fd == -1) {
            if (errno == EEXIST) {
                fd = shm_open(path.c_str(), O_RDWR, 0666);
                if (fd == -1)
                    return;
                queuePtr = 0;
            }
            else {
                return;
            }
        }

        if (ftruncate(fd, sz) == -1)
            return;

        ptr = mmap(NULL, sz, PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0);
        if (ptr == MAP_FAILED)
            return;

        sem = sem_open(path.c_str(), O_CREAT, 0666, 1);
        sem_wait(sem);
        if (queuePtr == 0) {
            queuePtr = *(uint8_t*)((uintptr_t)ptr + 2);
            tailPtr = *(uint8_t*)((uintptr_t)ptr + 1);
            if (tailPtr == queuePtr) {
                *(uint8_t*)(ptr) += 1;
            }
        }
        else {
            queuePtr = 0;
            *(uint8_t*)((uintptr_t)ptr + 2) = 0;
            *(uint8_t*)((uintptr_t)ptr + 1) = 0;
            *(uint8_t*)(ptr) = 1;
        }
        int i = 0;
        while (i < 256) {
            int thisPid = *(int*)((uintptr_t)ptr + 4 + (i * 5));
            if (thisPid == 0)
                break;
            i++;
        }
        if (i == 256) {
            sem_post(sem);
            munmap(ptr, sz);
            close(fd);
            sem_destroy(sem);
            return;
        }
        else {
            pid = getpid();
            *(int*)((uintptr_t)ptr + 4 + (i * 5)) = pid;
            listen = (uint8_t*)((uintptr_t)ptr + 3 + (i * 5));
            *listen = queuePtr;
        }
        sem_post(sem);
        init = true;
    }
    ~Listener() {
        if (init) {
            sem_wait(sem);
            *(int*)((uintptr_t)listen + 1) = 0;
            sem_post(sem);
            munmap(ptr, sz);
            close(fd);
            sem_destroy(sem);
        }
    }
    uint32_t poll(uint32_t(*getMem)(void*, void*, void*, bool), void* memPtr, void* dst) {
        if (init && *(int*)((uintptr_t)listen + 1) == pid) {
            sem_wait(sem);
            uint32_t ret = 0;
            uint8_t topQueuePtr = 0;
            memcpy(&topQueuePtr, (void*)((uintptr_t)ptr + 2), 1);
            if (queuePtr != topQueuePtr) {
                void* polledPos = nullptr;
                memcpy(&polledPos, (void*)((uintptr_t)ptr + (queuePtr * sizeof(void*)) + 1283), sizeof(void*));
                uint8_t oldQueue = queuePtr;
                queuePtr++;
                if (*(int*)((uintptr_t)listen + 1) == pid) {
                    *listen = queuePtr;
                }
                tailPtr = *(uint8_t*)((uintptr_t)ptr + 1);
                bool clear = false;
                if (oldQueue == tailPtr) {
                    *(uint8_t*)(ptr) -= 1;
                    if (*(uint8_t*)(ptr) == 0) {
                        tailPtr++;
                        clear = true;
                        uint8_t leastenc = 0;
                        for (int i = 0; i < 256; i++) {
                            if (*(int*)((uintptr_t)ptr + (i * 5) + 4) != 0) {
                                if (*(uint8_t*)((uintptr_t)ptr + (i * 5) + 3) == tailPtr) {
                                    leastenc++;
                                }
                            }
                        }
                        *(uint8_t*)(ptr) = leastenc;
                        *(uint8_t*)((uintptr_t)ptr + 1) = tailPtr;
                    }
                }
                ret = getMem(memPtr, polledPos, dst, clear);
            }
            sem_post(sem);
            return ret;
        } else {
            return 0;
        }
    }
private:
    size_t sz;
    sem_t* sem;
    int fd;
    void* ptr;
    uint8_t queuePtr = 1;
    uint8_t tailPtr = 0;
    uint8_t* listen;
    int pid = 0;
    bool init = false;
};
extern "C" void* createListener(const char* name) {
    return new Listener(name);
}
extern "C" void destroyListener(void* ptr) {
    delete (Listener*)ptr;
}
extern "C" uint32_t poll(uint32_t(*getMem)(void*, void*, void*, bool), void* memPtr, void* ptr, void* dst) {
    return ((Listener*)ptr)->poll(getMem, memPtr, dst);
}
extern "C" uint32_t getVersion() {
    return EBUS_LISTENER_VERSION;
}