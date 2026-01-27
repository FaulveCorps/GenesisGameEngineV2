#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <vector>

namespace Genesis::Engine {

class FrameAllocator {
public:
    explicit FrameAllocator(size_t capacityBytes);
    FrameAllocator(const FrameAllocator&) = delete;
    FrameAllocator& operator=(const FrameAllocator&) = delete;
    FrameAllocator(FrameAllocator&&) noexcept = default;
    FrameAllocator& operator=(FrameAllocator&&) noexcept = default;

    void Reset();

    void* Allocate(size_t size, size_t alignment = alignof(std::max_align_t));

    template<typename T>
    T* Allocate(size_t count = 1) {
        return reinterpret_cast<T*>(Allocate(sizeof(T) * count, alignof(T)));
    }

    size_t GetCapacity() const { return m_buffer.size(); }
    size_t GetUsed() const { return m_offset; }
    size_t GetRemaining() const { return m_buffer.size() - m_offset; }

    bool Owns(const void* ptr) const;

private:
    std::vector<uint8_t> m_buffer;
    size_t m_offset = 0;
};

} // namespace Genesis::Engine
