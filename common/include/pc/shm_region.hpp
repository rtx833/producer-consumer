#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace pc {

// A POSIX shared memory object mapped as:
//
//   [ header page ][ data ... ][ data ... ]   <- virtual address space
//                  ^ same physical pages mapped twice, back to back
//
// The "mirrored" data mapping makes the ring buffer built on top of it
// contiguous across its wrap-around point: a record starting near the end of
// the data area simply continues at the beginning without any split copies.
class MirroredShmRegion {
public:
    static std::size_t page_size();

    // Creates (replacing any stale object with the same name) a region whose
    // data area is `data_capacity` bytes. Capacity must be a multiple of the
    // page size. The returned region owns the name and unlinks it on request
    // or on destruction.
    static MirroredShmRegion create(const std::string& name, std::size_t data_capacity);

    // Maps an existing region. Returns nullopt if no object with that name
    // exists yet or if it has not been sized by its creator yet.
    static std::optional<MirroredShmRegion> try_open(const std::string& name);

    MirroredShmRegion(MirroredShmRegion&& other) noexcept;
    MirroredShmRegion& operator=(MirroredShmRegion&& other) noexcept;
    MirroredShmRegion(const MirroredShmRegion&) = delete;
    MirroredShmRegion& operator=(const MirroredShmRegion&) = delete;
    ~MirroredShmRegion();

    [[nodiscard]] void* header() noexcept { return header_; }
    [[nodiscard]] const void* header() const noexcept { return header_; }
    [[nodiscard]] std::size_t header_capacity() const noexcept { return header_bytes_; }
    [[nodiscard]] std::uint8_t* data() noexcept { return data_; }
    [[nodiscard]] std::size_t data_capacity() const noexcept { return data_bytes_; }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
    [[nodiscard]] bool owner() const noexcept { return owner_; }

    // Removes the name from the system; existing mappings stay valid.
    void unlink() noexcept;

private:
    MirroredShmRegion() = default;
    static MirroredShmRegion map(int fd, std::string name, std::size_t header_bytes, std::size_t data_bytes,
                                 bool owner);
    void release() noexcept;

    std::string name_;
    void* header_ = nullptr;
    std::uint8_t* data_ = nullptr;
    std::size_t header_bytes_ = 0;
    std::size_t data_bytes_ = 0;
    bool owner_ = false;
};

}  // namespace pc
