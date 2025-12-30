#pragma once

#include <cstdint>

#include "config.h"

namespace Cell {

    /**
     * @brief Tags for memory profiling and subsystem identification.
     */
    enum class MemoryTag : uint8_t {
        Unknown = 0, /**< Untagged or unknown memory. */
        General,     /**< General purpose allocation. */
        // Add application-specific tags here
    };

    /**
     * @brief Header stored at the beginning of each Cell.
     *
     * Contains metadata for profiling and management.
     */
    struct CellHeader {
        MemoryTag tag;       /**< Memory subsystem tag. */
        uint8_t reserved[7]; /**< Reserved for future use (alignment, generation counters, etc.) */
    };

    /**
     * @brief A fixed-size, aligned memory unit.
     *
     * The usable payload starts after the CellHeader.
     */
    struct CellData {
        CellHeader header; /**< Metadata header at the start of the cell. */
        // Remaining bytes are available for allocation
    };

    /**
     * @brief Locates the CellHeader for any pointer within a Cell.
     *
     * Performs a constant-time alignment mask.
     *
     * @param ptr Any pointer within a Cell's memory range.
     * @return Pointer to the CellHeader at the start of the Cell.
     */
    inline CellHeader *get_header(void *ptr) {
        auto addr = reinterpret_cast<uintptr_t>(ptr);
        return reinterpret_cast<CellHeader *>(addr & kCellMask);
    }

}
