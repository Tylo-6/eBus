#define EBUS_MEMORY_VERSION 1

#include <iostream>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <semaphore.h>
#include <cerrno>

struct Memory {
public:
    void updateSize() {
        if (*(size_t*)ptr != sz) {
            size_t newsz = *(size_t*)ptr;
            munmap(ptr, (sz * 256) + sizeof(size_t));
            sz = newsz;
            if (ftruncate(fd, (sz * 256) + sizeof(size_t)) == -1)
                return;
            ptr = mmap(NULL, (sz * 256) + sizeof(size_t), PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0);
            if (ptr == MAP_FAILED)
                return;
        }
    }
    void setNewSize(size_t size) {
        *(size_t*)ptr = size;
        munmap(ptr, (sz * 256) + sizeof(size_t));
        sz = size;
        if (ftruncate(fd, (sz * 256) + sizeof(size_t)) == -1)
            return;
        ptr = mmap(NULL, (sz * 256) + sizeof(size_t), PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0);
        if (ptr == MAP_FAILED)
            return;
    }
    Memory(const char* name) {
        sz = 1024;

        std::string path = "/ebus_mem::";
        path += name;

        bool setSize = false;
        fd = shm_open(path.c_str(), O_CREAT | O_RDWR | O_EXCL, 0666);
        if (fd == -1) {
            if (errno == EEXIST) {
                fd = shm_open(path.c_str(), O_RDWR, 0666);
                if (fd == -1)
                    return;
            }
            else {
                return;
            }
        } else {
            if (ftruncate(fd, (sz * 256) + sizeof(size_t)) == -1)
                return;
            setSize = true;
        }

        ptr = mmap(NULL, (sz * 256) + sizeof(size_t), PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0);
        if (ptr == MAP_FAILED)
            return;

        sem = sem_open(path.c_str(), O_CREAT, 0666, 1);

        uint32_t num = 0;
        sem_wait(sem);
        if (setSize) {
            *(size_t*)ptr = sz;
        } else {
            updateSize();
        }
        for (uint32_t i = 0; i < sz; i++) {
            *(uint32_t*)((uintptr_t)ptr + (i * 256) + sizeof(size_t)) = num;
        }
        sem_post(sem);
    }
    ~Memory() {
        munmap(ptr, (sz * 256) + sizeof(size_t));
        close(fd);
        sem_destroy(sem);
    }
    void* post(void* src, size_t size) {
        uint32_t times = 0;
        bool outputSet = false;
        void* output = nullptr;
        uint32_t lasti = 0;
        int first = 4;
        sem_wait(sem);
        updateSize();
        for (uint32_t i = 0; i < 4194304; i++) {
            uint32_t cont = *(uint32_t*)((uintptr_t)ptr + (i * 256) + sizeof(size_t));
            if (cont == 0) {
                if (!outputSet) {
                    outputSet = true;
                    output = (void*)((uintptr_t)i * 256);
                    *(uint32_t*)((uintptr_t)ptr + (i * 256) + 4 + sizeof(size_t)) = size;
                }
                else {
                    *(uint32_t*)((uintptr_t)ptr + (lasti * 256) + sizeof(size_t)) = i - lasti + 1;
                }
                if (size > 252 - first) {
                    memcpy((void*)((uintptr_t)ptr + (i * 256) + 4 + first + sizeof(size_t)), src, 252 - first);
                    src = (void*)((uintptr_t)src + 252 - first);
                    lasti = i;
                    size -= (252 - first);
                }
                else {
                    memcpy((void*)((uintptr_t)ptr + (i * 256) + 4 + first + sizeof(size_t)), src, size);
                    *(uint32_t*)((uintptr_t)ptr + (i * 256) + sizeof(size_t)) = 1;
                    size = 0;
                    sem_post(sem);
                    return output;
                }
                first = 0;
            }
            if (i == sz - 1) {
                setNewSize(sz + 256);
            }
        }
        sem_post(sem);
        return nullptr;
    }
    uint32_t getMem(void* src, void** output, bool clear) {
        char* cArray = nullptr;
        void* dst = nullptr;
        uint32_t size = 1;
        uint32_t fullSize = 0;
        int first = 4;
        sem_wait(sem);
        updateSize();
        while (size > 0) {
            uint32_t next = *(uint32_t*)((uintptr_t)ptr + (uintptr_t)src + sizeof(size_t)) - 1;
            if (first != 0) {
                size = *(uint32_t*)((uintptr_t)ptr + (uintptr_t)src + 4 + sizeof(size_t));
                cArray = new char[size];
                dst = cArray;
                fullSize = size;
            }
            if (clear)
                *(uint32_t*)((uintptr_t)ptr + (uintptr_t)src + sizeof(size_t)) = 0;
            if (size > 252 - first) {
                memcpy(dst, (void*)((uintptr_t)ptr + (uintptr_t)src + 4 + first + sizeof(size_t)), 252 - first);
                size -= (252 - first);
                src = (void*)((uintptr_t)src + ((uintptr_t)next * 256));
                dst = (void*)((uintptr_t)dst + 252 - first);
            }
            else {
                memcpy(dst, (void*)((uintptr_t)ptr + (uintptr_t)src + 4 + first + sizeof(size_t)), size);
                size = 0;
            }
            first = 0;
        }
        sem_post(sem);
        *output = cArray;
        return fullSize;
    }
    void clear(void* src) {
        sem_wait(sem);
        updateSize();
        uint32_t next = 1;
        while (next > 0) {
            next = *(uint32_t*)((uintptr_t)ptr + (uintptr_t)src + sizeof(size_t)) - 1;
            *(uint32_t*)((uintptr_t)ptr + (uintptr_t)src + sizeof(size_t)) = 0;
            src = (void*)((uintptr_t)src + ((uintptr_t)next * 256));
        }
        sem_post(sem);
    }
private:
    void* ptr;
    size_t sz;
    sem_t* sem;
    int fd;
};
extern "C" void* createMemory(const char* name) {
    return new Memory(name);
}
extern "C" void destroyMemory(void* ptr) {
    delete (Memory*)ptr;
}
extern "C" void post(void* ptr, void* src, size_t size) {
    ((Memory*)ptr)->post(src, size);
}
extern "C" uint32_t getMem(void* ptr, void* src, void** dst, bool clear) {
    return ((Memory*)ptr)->getMem(src, dst, clear);
}
extern "C" void clear(void* ptr, void* src) {
    return ((Memory*)ptr)->clear(src);
}
extern "C" uint32_t getVersion() {
    return EBUS_MEMORY_VERSION;
}