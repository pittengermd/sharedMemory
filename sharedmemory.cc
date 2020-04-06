#include "sharedmemory.hh"
#include <iostream>

SharedMemory::SharedMemory(const std::string& path, size_t size, sharedMemMode mode, sharedMemRole role) :
    _sharedMemName(path), _size(size + sizeof(*_sync)), _mode(mode), _role(role)
{ }

SharedMemory::~SharedMemory()
{
    #ifdef POSIX_ONLY
    // make sure we unmap shared memory
    if (!UnMap()) {
        ReportSystemSharedMemError("SharedMemory::~SharedMemory::UnMap()");
    }
    if (!Close()) {
        ReportSystemSharedMemError("SharedMemory::~SharedMemory::Close()");
    }
    #endif
    shared_memory_object::remove(_sharedMemName.c_str());
}

bool
SharedMemory::Create()
{
    #ifdef POSIX_ONLY
    switch (_mode) {
        case sharedMemMode::ReadOnly:
            _fileDescriptor = shm_open(_sharedMemName.c_str(), O_RDONLY | O_CREAT | O_EXCL, 0);
            break;
        case sharedMemMode::ReadWrite:
            _fileDescriptor = shm_open(_sharedMemName.c_str(), O_RDWR | O_CREAT | O_EXCL, 0660);
            break;
        default:
            break;
    }

    if (_fileDescriptor == -1) {
        ReportSystemSharedMemError("Error on shm_open::");
        return false;
    }

    if (_mode == sharedMemMode::ReadWrite) {
        if (ftruncate(_fileDescriptor, _size) == -1) {
            ReportSystemSharedMemError("Error on ftruncate::");
            return false;
        }
    }
    #elif MANAGED

    _memSegment =
      managed_shared_memory { create_only, _sharedMemName.c_str(), _size + sizeof(managed_shared_memory::handle) };
    _freeMem       = managed_shared_memory::size_type { _memSegment.get_free_memory() };
    _memMapPointer = static_cast<std::byte *>(_memSegment.allocate(_size + sizeof(managed_shared_memory::handle)));
    if (_memMapPointer == nullptr) {
        std::cout << "ERROR!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!n\n" << std::endl;
    }

    // error checks
    if (_freeMem <= _memSegment.get_free_memory() ) {
        return false;
    }

    _handle = _memSegment.get_handle_from_address(_memMapPointer);
    std::memcpy(_memMapPointer, &_handle, sizeof(_handle);
    #elif UNMANAGED
    switch (_mode) {
        case sharedMemMode::ReadWrite:
            _memSegment = shared_memory_object { open_or_create, _sharedMemName.c_str(), read_write };
            _memSegment.truncate(_size);
            _mapping = mapped_region { _memSegment, read_write };
            std::memset(_mapping.get_address(), 0, _mapping.get_size());
            if (_mapping.get_address()) {
                // _sync = new Synchronization;
                _sync = new (_mapping.get_address()) Synchronization;
                _sync->TestingVar = 128;
                // std::memcpy(_mapping.get_address(), _sync, sizeof(*_sync));
                // _basePointer = reinterpret_cast<std::byte *>(_mapping.get_address()) + sizeof(_sync);
                _basePointer = reinterpret_cast<std::byte *>(_mapping.get_address());
                if (_basePointer) {
                    std::cout << "Address is: " << _basePointer << std::endl;
                    _basePointer += sizeof(*_sync);
                    std::cout << "New Address is: " << _basePointer << std::endl;
                    std::cout << "Sizeof _sync is: " << sizeof(*_sync) << std::endl;
                    // Can I move _basePointer back to base address and still print updated TestingVar
                    _basePointer -= sizeof(*_sync);
                    auto temp { reinterpret_cast<Synchronization *>(_basePointer)->TestingVar };
                    std::cout << "TestingVar from Create is: "
                              << temp << std::endl;
                    _basePointer += sizeof(*_sync);
                }
            }
            break;
        default:
            break;
    }
    #endif // ifdef POSIX_ONLY
    return true;
} // SharedMemory::Create

bool
SharedMemory::Open()
{
    std::cout << "SharedMemory::Open" << std::endl;

    #ifdef MANAGED
    std::cout << "MANAGED IS: " << MANAGED << std::endl;
    #endif
    #ifdef POSIX_ONLY
    switch (_mode) {
        case sharedMemMode::ReadOnly:
            _fileDescriptor = shm_open(_sharedMemName.c_str(), O_RDONLY, 0);
            break;
        case sharedMemMode::ReadWrite:
            _fileDescriptor = shm_open(_sharedMemName.c_str(), O_RDWR, 0660);
            break;
        default:
            break;
    }

    if (_fileDescriptor == -1) {
        ReportSystemSharedMemError("Error on shm_open::");
        return false;
    }

    if (_mode == sharedMemMode::ReadWrite) {
        if (ftruncate(_fileDescriptor, _size) == -1) {
            ReportSystemSharedMemError("Error on ftruncate::");
            return false;
        }
    }
    #elif MANAGED
    std::cout << "Open Managed" << std::endl;
    switch (_mode) {
        case sharedMemMode::ReadOnly:
            _memSegment    = managed_shared_memory { open_read_only, _sharedMemName.c_str() };
            _memMapPointer = static_cast<std::byte *>(_memSegment.allocate(
                  _size + sizeof(_handle)));
            // std::memcpy(&_handle, _memMapPointer, sizeof(_handle));
            // _memMapPointer  = static_cast<std::byte *>(_memSegment.get_address_from_handle(_handle));
            // _memMapPointer += sizeof(_handle);
            break;
        case sharedMemMode::ReadWrite:
            _memSegment =
              managed_shared_memory { open_or_create, _sharedMemName.c_str(), _size + sizeof(_handle) };
            _memMapPointer = static_cast<std::byte *>(_memSegment.allocate(
                  _size + sizeof(_handle)));
            // std::memcpy(&_handle, _memMapPointer, sizeof(_handle));
            // _memMapPointer  = reinterpret_cast<std::byte *>(_memSegment.get_handle_from_address(_handle));
            // _memMapPointer  = static_cast<std::byte *>(_memSegment.get_address_from_handle(_handle));
            // _memMapPointer += sizeof(_handle);
            break;
        default:
            break;
    }
    _size = _memSegment.get_size();
    std::cout << "_size is: " << _size << std::endl;
    std::cout << "sizeof(_handle) is: " << sizeof(_handle) << std::endl;
    #elif UNMANAGED
    switch (_mode) {
        case sharedMemMode::ReadOnly:
            _memSegment = shared_memory_object(open_only, _sharedMemName.c_str(), read_only);
            _mapping = mapped_region { _memSegment, read_only };
            if (_mapping.get_address()) {
                // Read the _sync struct and populate this instance with the master's sync utilites
                // std::memcpy(_sync, _mapping.get_address(), sizeof(*_sync));
                _sync = static_cast<Synchronization *>(_mapping.get_address());
                std::cout << "TestingVar is: " << _sync->TestingVar << std::endl;
                _basePointer = reinterpret_cast<std::byte *>(_mapping.get_address()) + sizeof(*_sync);
            }
            break;
        case sharedMemMode::ReadWrite:
            _memSegment = shared_memory_object(open_or_create, _sharedMemName.c_str(), read_write);
            offset_t SharedMemSize;
            if (_memSegment.get_size(SharedMemSize) < _size) {
                _memSegment.truncate(_size);
            }
            _mapping = mapped_region { _memSegment, read_write };
            if (_mapping.get_address()) {
                // Store Sync struct in the first bytes of shared mem
                _sync = static_cast<Synchronization *>(_mapping.get_address());
                std::cout << "TestingVar is: " << _sync->TestingVar << std::endl;
                _basePointer = reinterpret_cast<std::byte *>(_mapping.get_address()) + sizeof(*_sync);
            }
            break;
        default:
            break;
    }

    #endif // ifdef POSIX_ONLY

    return true;
} // SharedMemory::Open

bool
SharedMemory::Map()
{
    #ifdef POSIX_ONLY
    // Ensure shared memory is open before mapping to it
    if (_fileDescriptor > 0) {
        switch (_mode) {
            case sharedMemMode::ReadOnly:
                _memMapPointer =
                  (static_cast<std::byte *>(mmap(NULL, _size, PROT_READ, MAP_SHARED, _fileDescriptor, 0)));
                break;
            case sharedMemMode::ReadWrite:
                _memMapPointer =
                  (static_cast<std::byte *>(mmap(NULL, _size, PROT_READ | PROT_WRITE, MAP_SHARED, _fileDescriptor, 0)));
                break;
            default:
                break;
        }
    } else {
        std::cout << "SHARED MEM NOT OPEN!!\n" << std::endl;
        return false;
    }
    # elseif UNMANAGED
    switch (_mode) {
        case sharedMemMode::ReadOnly:
            _mapping = mapped_region { _memSegment, read_only };
            break;
        case sharedMemMode::ReadWrite:
            _mapping = mapped_region { _memSegment, read_write };
            break;
        default:
            break;
    }
    std::memset(_mapping.get_addres(), 0, _mapping.get_size());
    #endif // ifdef POSIX_ONLY
    return true;
} // SharedMemory::Map

bool
SharedMemory::SetSize(size_t specifiedSize)
{
    // if (ftruncate (_fileDescriptor,  specifiedSize) != 0)
    // {
    //   ReportSystemSharedMemError();
    //   return false;
    // }
    return true;
}

bool
SharedMemory::UnMap()
{
    // if (munmap(static_cast<void*>(_memMapPointer), _size) != 0)
    // {
    //     ReportSystemSharedMemError("Error on munmap::");
    //     return false;
    // }
    return true;
}

bool
SharedMemory::Close()
{
    // _fileDescriptor =  shm_unlink(_sharedMemName.c_str());
    // if (_fileDescriptor == -1 )
    // {
    //     ReportSystemSharedMemError("Error on shm_unlink::");
    //     return false;
    // }
    return true;
}

void
SharedMemory::ReportSystemSharedMemError(const std::string& errorString)
{
    switch (errno) {
        case EACCES:
            perror(errorString.c_str());
            break;
        case EEXIST:
            perror(errorString.c_str());
            break;
        case EINVAL:
            perror(errorString.c_str());
            break;
        case EMFILE:
            perror(errorString.c_str());
            break;
        case ENOENT:
            perror(errorString.c_str());
            break;
        case EFAULT:
            perror(errorString.c_str());
        case EFBIG:
            perror(errorString.c_str());
        case EINTR:
            perror(errorString.c_str());
        case EIO:
            perror(errorString.c_str());
        case EISDIR:
            perror(errorString.c_str());
        case ELOOP:
            perror(errorString.c_str());
            break;
        case ENAMETOOLONG:
            perror(errorString.c_str());
            break;
        case EPERM:
            perror(errorString.c_str());
            break;
        case EROFS:
            perror(errorString.c_str());
            break;
        case ETXTBSY:
            perror(errorString.c_str());
            break;
        case EBADF:
            perror(errorString.c_str());
            break;
        default:
            perror("Unknown error: ");
            break;
    }
} // SharedMemory::ReportSystemSharedMemError
