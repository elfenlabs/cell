#pragma once

#include "context.h"

#include <cstddef>
#include <type_traits>

namespace Cell {

    /**
     * @brief STL-compatible allocator backed by a Cell Context.
     *
     * Allows using Cell memory with standard containers:
     * @code
     * Cell::Context ctx;
     * std::vector<int, Cell::StlAllocator<int>> vec(Cell::StlAllocator<int>(ctx));
     * vec.push_back(42);  // Uses Cell memory!
     * @endcode
     *
     * @tparam T Type to allocate.
     */
    template <typename T> class StlAllocator {
    public:
        // Standard allocator type definitions
        using value_type = T;
        using size_type = size_t;
        using difference_type = ptrdiff_t;
        using propagate_on_container_move_assignment = std::true_type;
        using is_always_equal = std::false_type;

        /**
         * @brief Constructs an allocator using the given Context.
         * @param ctx Reference to the Cell context (must outlive the allocator).
         * @param tag Optional tag for profiling (default: 0).
         */
        explicit StlAllocator(Context &ctx, uint8_t tag = 0) noexcept : m_ctx(&ctx), m_tag(tag) {}

        /**
         * @brief Copy constructor for rebinding.
         */
        template <typename U>
        StlAllocator(const StlAllocator<U> &other) noexcept
            : m_ctx(other.context()), m_tag(other.tag()) {}

        /**
         * @brief Allocates memory for n objects of type T.
         * @param n Number of objects.
         * @return Pointer to allocated memory.
         * @throws std::bad_alloc if allocation fails.
         */
        [[nodiscard]] T *allocate(size_type n) {
            if (n == 0)
                return nullptr;

            void *ptr = m_ctx->alloc_bytes(n * sizeof(T), m_tag, alignof(T));
            if (!ptr) {
                throw std::bad_alloc();
            }
            return static_cast<T *>(ptr);
        }

        /**
         * @brief Deallocates memory.
         * @param p Pointer to memory.
         * @param n Number of objects (unused, but required by interface).
         */
        void deallocate(T *p, size_type /*n*/) noexcept { m_ctx->free_bytes(p); }

        /**
         * @brief Returns the underlying context.
         */
        [[nodiscard]] Context *context() const noexcept { return m_ctx; }

        /**
         * @brief Returns the allocation tag.
         */
        [[nodiscard]] uint8_t tag() const noexcept { return m_tag; }

    private:
        Context *m_ctx;
        uint8_t m_tag;
    };

    // Equality comparison (required for C++17 allocator requirements)
    template <typename T, typename U>
    bool operator==(const StlAllocator<T> &a, const StlAllocator<U> &b) noexcept {
        return a.context() == b.context();
    }

    template <typename T, typename U>
    bool operator!=(const StlAllocator<T> &a, const StlAllocator<U> &b) noexcept {
        return !(a == b);
    }

}
