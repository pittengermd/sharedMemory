#pragma once
#include <string>
#include <cstddef>
#include <cstring>
#include <vector>
#include <iostream>
#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <memory>
#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include <boost/interprocess/sync/interprocess_condition.hpp>
#include <boost/interprocess/sync/sharable_lock.hpp>
#include <boost/interprocess/sync/upgradable_lock.hpp>
#include <boost/interprocess/sync/interprocess_upgradable_mutex.hpp>
#include <boost/interprocess/sync/interprocess_condition_any.hpp>

#define UNMANAGED 1

using namespace boost::interprocess;

enum class sharedMemMode {
    ReadOnly,
    ReadWrite,
    ERROR
};

enum class sharedMemRole {
    Master,
    Slave
};

struct Synchronization {
    interprocess_upgradable_mutex sharedMemMutex;
    // interprocess_condition        sharedMemCondition;
    interprocess_condition_any    sharedMemCondition;
    uint32_t                      TestingVar { 255 };
};

class SharedMemory
{
public:
    SharedMemory() = delete;
    SharedMemory(const std::string& path, size_t size, sharedMemMode mode, sharedMemRole role);
    ~SharedMemory();

    bool
    IsValid(){ return _fileDescriptor > 0; }

    bool
    SetSize(size_t specifiedSize);
    bool
    Create();
    bool
    Open();
    bool
    Map();
    // Currently this will not work with a heap allocated container
    // TODO Implement template checks to find std::container and use std::iterator to copy
    template <typename ObjectToWrite>
    void
    Write(const ObjectToWrite& dataToWrite)
    {
        std::cout << "SharedMemory::Write" << std::endl;
        std::cout << "sizeof(dataToWrite) is: " << sizeof(dataToWrite) << std::endl;

        #if UNMANAGED
        upgradable_lock<interprocess_upgradable_mutex> lock(_sync->sharedMemMutex);
        scoped_lock<interprocess_upgradable_mutex> exclusiveLock(std::move(lock));


        if (_mapping.get_address() != nullptr) {
            std::memcpy(_basePointer, &dataToWrite, sizeof(dataToWrite));
            _sync->sharedMemCondition.notify_all();
        } else {
            std::cout << "ERROR _mapping was nullptr\n" << std::endl;
        }
        #else // if UNMANAGED
        #endif // if UNMANAGED
        // std::memcpy(_memMapPointer, &dataToWrite, sizeof(dataToWrite));
    }

    // This function reads an object T from Shared Memory,
    // If object is not at base of user data in shared memory
    // Then user must specify the offset to the object in shared memory
    template <typename T>
    T
    Read(size_t offset = 0)
    {
        T data;

        try
        {
            sharable_lock<interprocess_upgradable_mutex> lock(_sync->sharedMemMutex);
            // _sync->sharedMemMutex.lock_sharable();

            _sync->sharedMemCondition.wait(lock);

            #ifdef POSIX_ONLY
            std::memcpy(&data, _memMapPointer + offset, sizeof(data));
            #elif MANAGED
            // data = *(_memSegment.find<T>(objectName));
            std::memcpy(&data, _memMapPointer + offset, sizeof(data));
            #elif UNMANAGED
            std::memcpy(&data, _basePointer + offset, sizeof(data));
            #endif
        }
        catch (interprocess_exception &ex)
        {
            std::cout << ex.what() << std::endl;
        }
        return data;
    }

    std::vector<std::byte>
    Read(size_t offset = 0)
    {
        std::vector<std::byte> readBytes;

        readBytes.reserve(_size);
        {
            scoped_lock<interprocess_upgradable_mutex> lock(_sync->sharedMemMutex);
            std::memcpy(readBytes.data(), _memMapPointer, _size);
        }
        return readBytes;
    }

    bool
    UnMap();
    bool
    Close();
    size_t
    GetSize(){ return _size; }

private:
    void
    ReportSystemSharedMemError(const std::string& errorString = "");
    int _fileDescriptor { -1 };
    std::string _sharedMemName { "" };
    sharedMemRole _role { sharedMemRole::Slave };
    size_t _size { 0 };
    sharedMemMode _mode { sharedMemMode::ERROR };
    std::byte * _memMapPointer { nullptr };
    #ifdef MANAGED
    managed_shared_memory _memSegment;
    managed_shared_memory::size_type _freeMem;
    managed_shared_memory::handle_t _handle;
    #elif UNMANAGED
    shared_memory_object _memSegment;
    mapped_region _mapping;
    std::byte * _basePointer { nullptr };
    #endif

    Synchronization * _sync;
};
