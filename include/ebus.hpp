#ifndef EBUS_HPP
#define EBUS_HPP

#define EBUS_HEADER_VERSION 1

#include <iostream>
#include <dlfcn.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <semaphore.h>
#include <cerrno>
#include <cstdint>

void* memory = nullptr;

#ifdef EBUS_EMITTER
void* emitterDL = nullptr;
void*(*createEmitter)(const char*) = nullptr;
void(*destroyEmitter)(void*) = nullptr;
bool(*callEmit)(void*(*)(void*, size_t), void(*)(void*, void*), void*, void*, void*, size_t) = nullptr;
void*(*post)(void*, size_t) = nullptr;
void(*clear)(void*, void*) = nullptr;
uint32_t(*emitterVersion)() = nullptr;
struct Emitter {
public:
    Emitter(const char* name) {
        if (createEmitter)
            ptr = createEmitter(name);
    }
    ~Emitter() {
        if (destroyEmitter && ptr)
            destroyEmitter(ptr);
    }
    bool emit(void* src, size_t size) {
        if (callEmit && ptr)
            return callEmit(post, clear, memory, ptr, src, size);
        return false;
    }
private:
    void* ptr = nullptr;
};
#endif

#ifdef EBUS_LISTENER
void* listenerDL = nullptr;
void*(*createListener)(const char*) = nullptr;
void(*destroyListener)(void*) = nullptr;
uint32_t(*callPoll)(uint32_t(*)(void*, void*, bool), void*, void*, void*) = nullptr;
uint32_t(*getMem)(void*, void*, bool) = nullptr;
uint32_t(*listenerVersion)() = nullptr;
struct Listener {
public:
    Listener(const char* name) {
        if (createListener)
            ptr = createListener(name);
    }
    ~Listener() {
        if (destroyListener && ptr)
            destroyListener(ptr);
    }
    uint32_t poll(void* src) {
        if (callPoll && ptr)
            return callPoll(getMem, memory, ptr, src);
        return 0;
    }
private:
    void* ptr = nullptr;
};
#endif

void* memoryDL = nullptr;
void*(*createMemory)(const char*) = nullptr;
void(*destroyMemory)(void*) = nullptr;
uint32_t(*memVersion)() = nullptr;

int eBusInit() {
    int exitCode = 0;
    memoryDL = dlopen("/usr/lib/memory.so", RTLD_NOW);
    if (!memoryDL) {
        return exitCode;
    }
    memVersion = (uint32_t(*)())dlsym(memoryDL, "getVersion");
    if (memVersion() != EBUS_HEADER_VERSION) {
        dlclose(memoryDL);
        memoryDL = nullptr;
        return exitCode;
    }
    exitCode += 1;
    createMemory = (void*(*)(const char*))dlsym(memoryDL, "createMemory");
    destroyMemory = (void(*)(void*))dlsym(memoryDL, "destroyMemory");
    memory = createMemory("memory_pool");

    #ifdef EBUS_EMITTER

    emitterDL = dlopen("/usr/lib/emitter.so", RTLD_NOW);
    if (emitterDL) {
        emitterVersion = (uint32_t(*)())dlsym(emitterDL, "getVersion");
        if (emitterVersion() != EBUS_HEADER_VERSION) {
            dlclose(emitterDL);
            emitterDL = nullptr;
        } else {
            exitCode += 2;
            createEmitter = (void*(*)(const char*))dlsym(emitterDL, "createEmitter");
            destroyEmitter = (void(*)(void*))dlsym(emitterDL, "destroyEmitter");
            callEmit = (bool(*)(void*(*)(void*, size_t), void(*)(void*, void*), void*, void*, void*, size_t))dlsym(emitterDL, "emit");
            post = (void*(*)(void*, size_t))dlsym(memoryDL, "post");
            clear = (void(*)(void*, void*))dlsym(memoryDL, "clear");
        }
    }
    
    
    #endif

    #ifdef EBUS_LISTENER

    listenerDL = dlopen("/usr/lib/listener.so", RTLD_NOW);
    if (listenerDL) {
        listenerVersion = (uint32_t(*)())dlsym(listenerDL, "getVersion");
        if (listenerVersion() != EBUS_HEADER_VERSION) {
            dlclose(listenerDL);
            listenerDL = nullptr;
        } else {
            exitCode += 4;
            createListener = (void*(*)(const char*))dlsym(listenerDL, "createListener");
            destroyListener = (void(*)(void*))dlsym(listenerDL, "destroyListener");
            callPoll = (uint32_t(*)(uint32_t(*)(void*, void*, bool), void*, void*, void*))dlsym(listenerDL, "poll");
            getMem = (uint32_t(*)(void*, void*, bool))dlsym(memoryDL, "getMem");
        }
    }

    #endif

    return exitCode;
}
void eBusClose() {
    if (destroyMemory)
        destroyMemory(memory);

    if (memoryDL) {
        dlclose(memoryDL);
    }

    #ifdef EBUS_EMITTER
    if (emitterDL) {
        dlclose(emitterDL);
    }
    #endif

    #ifdef EBUS_LISTENER
    if (listenerDL) {
        dlclose(listenerDL);
    }
    #endif
}

#endif