#include "pc/shm_region.hpp"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace pc {
namespace {

[[noreturn]] void throw_errno(const char* what) {
    throw std::system_error(errno, std::generic_category(), what);
}

class FdGuard {
public:
    explicit FdGuard(int fd) noexcept : fd_(fd) {}
    ~FdGuard() {
        if (fd_ >= 0) {
            close(fd_);
        }
    }
    FdGuard(const FdGuard&) = delete;
    FdGuard& operator=(const FdGuard&) = delete;

private:
    int fd_;
};

void validate_name(const std::string& name) {
    if (name.size() < 2 || name[0] != '/' || name.find('/', 1) != std::string::npos) {
        throw std::invalid_argument("shared memory name must look like '/name' (single leading slash)");
    }
}

}  // namespace

std::size_t MirroredShmRegion::page_size() {
    const long size = sysconf(_SC_PAGESIZE);
    if (size <= 0) {
        throw_errno("sysconf(_SC_PAGESIZE)");
    }
    return static_cast<std::size_t>(size);
}

MirroredShmRegion MirroredShmRegion::create(const std::string& name, std::size_t data_capacity) {
    validate_name(name);
    const std::size_t page = page_size();
    if (data_capacity == 0 || data_capacity % page != 0) {
        throw std::invalid_argument("shared memory data capacity must be a non-zero multiple of the page size");
    }

    shm_unlink(name.c_str());  // remove a stale object left by a crashed producer, if any
    const int fd = shm_open(name.c_str(), O_CREAT | O_EXCL | O_RDWR, 0600);
    if (fd < 0) {
        throw_errno("shm_open(O_CREAT)");
    }
    FdGuard guard(fd);

    const std::size_t total = page + data_capacity;
    if (ftruncate(fd, static_cast<off_t>(total)) != 0) {
        const int saved = errno;
        shm_unlink(name.c_str());
        errno = saved;
        throw_errno("ftruncate");
    }
    try {
        return map(fd, name, page, data_capacity, /*owner=*/true);
    } catch (...) {
        shm_unlink(name.c_str());
        throw;
    }
}

std::optional<MirroredShmRegion> MirroredShmRegion::try_open(const std::string& name) {
    validate_name(name);
    const int fd = shm_open(name.c_str(), O_RDWR, 0);
    if (fd < 0) {
        if (errno == ENOENT) {
            return std::nullopt;
        }
        throw_errno("shm_open");
    }
    FdGuard guard(fd);

    struct stat st {};
    if (fstat(fd, &st) != 0) {
        throw_errno("fstat");
    }
    const std::size_t page = page_size();
    const auto size = static_cast<std::size_t>(st.st_size);
    if (size == 0) {
        return std::nullopt;  // created but not sized yet
    }
    if (size < 2 * page || (size - page) % page != 0) {
        throw std::runtime_error("shared memory object '" + name + "' has an unexpected size");
    }
    return map(fd, name, page, size - page, /*owner=*/false);
}

MirroredShmRegion MirroredShmRegion::map(int fd, std::string name, std::size_t header_bytes,
                                         std::size_t data_bytes, bool owner) {
    MirroredShmRegion region;
    region.name_ = std::move(name);
    region.owner_ = owner;
    region.header_bytes_ = header_bytes;
    region.data_bytes_ = data_bytes;

    void* header = mmap(nullptr, header_bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (header == MAP_FAILED) {
        throw_errno("mmap(header)");
    }
    region.header_ = header;

    // Reserve a contiguous range twice the data size, then map the same file
    // pages into both halves. MAP_FIXED replaces the reservation in place.
    void* base = mmap(nullptr, 2 * data_bytes, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (base == MAP_FAILED) {
        throw_errno("mmap(reserve)");
    }
    region.data_ = static_cast<std::uint8_t*>(base);

    const auto data_offset = static_cast<off_t>(header_bytes);
    for (int half = 0; half < 2; ++half) {
        void* target = region.data_ + static_cast<std::size_t>(half) * data_bytes;
        void* mapped = mmap(target, data_bytes, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, data_offset);
        if (mapped == MAP_FAILED) {
            throw_errno("mmap(data)");
        }
        if (mapped != target) {
            throw std::runtime_error("mmap(MAP_FIXED) returned an unexpected address");
        }
    }
    return region;
}

MirroredShmRegion::MirroredShmRegion(MirroredShmRegion&& other) noexcept
    : name_(std::move(other.name_)),
      header_(std::exchange(other.header_, nullptr)),
      data_(std::exchange(other.data_, nullptr)),
      header_bytes_(std::exchange(other.header_bytes_, 0)),
      data_bytes_(std::exchange(other.data_bytes_, 0)),
      owner_(std::exchange(other.owner_, false)) {}

MirroredShmRegion& MirroredShmRegion::operator=(MirroredShmRegion&& other) noexcept {
    if (this != &other) {
        release();
        name_ = std::move(other.name_);
        header_ = std::exchange(other.header_, nullptr);
        data_ = std::exchange(other.data_, nullptr);
        header_bytes_ = std::exchange(other.header_bytes_, 0);
        data_bytes_ = std::exchange(other.data_bytes_, 0);
        owner_ = std::exchange(other.owner_, false);
    }
    return *this;
}

MirroredShmRegion::~MirroredShmRegion() { release(); }

void MirroredShmRegion::unlink() noexcept {
    if (owner_) {
        shm_unlink(name_.c_str());
        owner_ = false;
    }
}

void MirroredShmRegion::release() noexcept {
    unlink();
    if (data_ != nullptr) {
        munmap(data_, 2 * data_bytes_);
        data_ = nullptr;
    }
    if (header_ != nullptr) {
        munmap(header_, header_bytes_);
        header_ = nullptr;
    }
}

}  // namespace pc
