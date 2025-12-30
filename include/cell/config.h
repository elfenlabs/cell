#pragma once

#include <cstddef>
#include <cstdint>

namespace Cell {

    /**
     * @brief Log2 of the Cell size. Default is 14 (16KB).
     *
     * Must be a power of 2. Minimum is 12 (4KB, standard page size).
     */
    static constexpr size_t kCellSizeLog2 = 14;

    /** @brief Cell size in bytes (2^kCellSizeLog2). */
    static constexpr size_t kCellSize = 1ULL << kCellSizeLog2;

    /** @brief Bitmask for aligning pointers to Cell boundaries. */
    static constexpr uintptr_t kCellMask = ~(kCellSize - 1);

    static_assert(kCellSize >= 4096, "Cell size must be at least 4KB (standard page size)");
    static_assert((kCellSize & (kCellSize - 1)) == 0, "Cell size must be a power of 2");

    /**
     * @brief Configuration for creating a Context.
     */
    struct Config {
        /**
         * @brief Total address space to reserve in bytes.
         *
         * Default: 16GB. Physical RAM is committed lazily.
         */
        size_t reserve_size = 16ULL * 1024 * 1024 * 1024;
    };

}
