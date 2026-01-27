#include "engine/FrameAllocator.h"

#include <algorithm>
#include <cstdint>

namespace Genesis::Engine {

FrameAllocator::FrameAllocator(size_t capacityBytes)
    : m_buffer(capacityBytes, 0u) {
}

void FrameAllocator::Reset() {
    m_offset = 0;
}

void* FrameAllocator::Allocate(size_t size, size_t alignment) {
    if (size == 0) return nullptr;
    if (alignment == 0) alignment = 1;

    const size_t capacity = m_buffer.size();
    if (capacity == 0) return nullptr;

    uintptr_t base = reinterpret_cast<uintptr_t>(m_buffer.data());
    uintptr_t current = base + m_offset;
    size_t misalignment = static_cast<size_t>(current % alignment);
    size_t padding = (misalignment == 0) ? 0 : (alignment - misalignment);

    if (m_offset + padding + size > capacity) {
        return nullptr;
    }

    size_t alignedOffset = m_offset + padding;
    m_offset = alignedOffset + size;
    return m_buffer.data() + alignedOffset;
}

bool FrameAllocator::Owns(const void* ptr) const {
    if (!ptr || m_buffer.empty()) return false;
    auto base = reinterpret_cast<uintptr_t>(m_buffer.data());
    auto end = base + m_buffer.size();
    auto address = reinterpret_cast<uintptr_t>(ptr);
    return address >= base && address < end;
}

} // namespace Genesis::Engine
