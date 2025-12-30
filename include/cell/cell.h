#pragma once

#include <cstdint>

#include "config.h"

namespace Cell {

#ifndef NDEBUG
    /** @brief Magic number for cell validation in debug builds. */
    static constexpr uint32_t kCellMagic = 0xCE11DA7A; // "CELLDATA"

    /** @brief Magic number indicating a freed cell in debug builds. */
    static constexpr uint32_t kCellFreeMagic = 0xDEADCE11; // "DEADCELL"
#endif

    /**
     * @brief Header stored at the beginning of each Cell.
     *
     * Contains metadata for profiling and management.
     * Debug builds include additional fields for corruption detection.
     */
    struct CellHeader {
        uint8_t tag; /**< Application-defined memory tag for profiling. */
#ifdef NDEBUG
        uint8_t reserved[7]; /**< Reserved for future use. */
#else
        uint8_t flags;       /**< State flags (reserved for future use). */
        uint16_t generation; /**< Incremented on free, detects stale references. */
        uint32_t magic;      /**< Magic number for validation (kCellMagic or kCellFreeMagic). */
#endif
    };

#ifndef NDEBUG
    /**
     * @brief Validates that a cell header has the correct magic number.
     * @param header Pointer to the cell header.
     * @return true if the cell is valid and in-use, false otherwise.
     */
    inline bool is_valid_cell(const CellHeader *header) {
        return header && header->magic == kCellMagic;
    }

    /**
     * @brief Checks if a cell has been freed (debug builds only).
     * @param header Pointer to the cell header.
     * @return true if the cell has been freed, false otherwise.
     */
    inline bool is_freed_cell(const CellHeader *header) {
        return header && header->magic == kCellFreeMagic;
    }
#endif

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
