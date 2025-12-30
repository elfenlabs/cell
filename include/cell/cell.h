#pragma once

#include <cstdint>

#include "config.h"

namespace Cell {

#ifndef NDEBUG
    /** @brief Magic number for cell validation in debug builds. */
    static constexpr uint32_t kCellMagic = 0xCE11DA7A; // "CELLDATA"

    /** @brief Magic number indicating a freed cell in debug builds. */
    static constexpr uint32_t kCellFreeMagic = 0xDEADCE11; // "DEADCELL"

    /** @brief Poison byte pattern for freed memory in debug builds. */
    static constexpr uint8_t kPoisonByte = 0xFE;
#endif

    // -------------------------------------------------------------------------
    // Free-List Node
    // -------------------------------------------------------------------------

    /**
     * @brief A free block node for the inline free-list.
     *
     * Stored inline in freed memory blocks within a cell.
     */
    struct FreeBlock {
        FreeBlock *next; /**< Pointer to next free block in the cell. */
    };

    // -------------------------------------------------------------------------
    // Cell Header
    // -------------------------------------------------------------------------

    /**
     * @brief Header stored at the beginning of each Cell.
     *
     * Contains metadata for profiling and management.
     * Debug builds include additional fields for corruption detection.
     */
    struct CellHeader {
        uint8_t tag;         /**< Application-defined memory tag for profiling. */
        uint8_t size_class;  /**< Size class bin index (0-9), or kFullCellMarker. */
        uint16_t free_count; /**< Number of free blocks remaining in this cell. */
#ifdef NDEBUG
        uint8_t reserved[4]; /**< Reserved for future use. */
#else
        uint16_t generation; /**< Incremented on free, detects stale references. */
        uint16_t reserved;   /**< Reserved for alignment. */
        uint32_t magic;      /**< Magic number for validation (kCellMagic or kCellFreeMagic). */
#endif
    };

    // Ensure header is exactly 8 bytes in release, 12 bytes in debug
#ifdef NDEBUG
    static_assert(sizeof(CellHeader) == 8, "CellHeader must be 8 bytes in release");
#else
    static_assert(sizeof(CellHeader) == 12, "CellHeader must be 12 bytes in debug");
#endif

    // -------------------------------------------------------------------------
    // Cell Metadata (for sub-cell allocation)
    // -------------------------------------------------------------------------

    /**
     * @brief Extended metadata stored after CellHeader for sub-cell management.
     *
     * Only used when the cell is dedicated to a size class (size_class != kFullCellMarker).
     */
    struct CellMetadata {
        CellHeader *next_partial; /**< Next cell in bin's partial list (nullptr if none). */
        FreeBlock *free_list;     /**< Head of free blocks in this cell. */
    };

    /**
     * @brief Helper to align a value up to the given alignment.
     */
    inline constexpr size_t align_up_const(size_t value, size_t alignment) {
        return (value + alignment - 1) & ~(alignment - 1);
    }

    /** @brief Offset to first allocatable block after header + metadata, aligned to 16 bytes. */
    static constexpr size_t kBlockStartOffset =
        align_up_const(sizeof(CellHeader) + sizeof(CellMetadata), 16);

    // -------------------------------------------------------------------------
    // Cell Data
    // -------------------------------------------------------------------------

    /**
     * @brief A fixed-size, aligned memory unit.
     *
     * The usable payload starts after the CellHeader (and CellMetadata for sub-cell).
     */
    struct CellData {
        CellHeader header; /**< Metadata header at the start of the cell. */
        // Remaining bytes are available for allocation
    };

    // -------------------------------------------------------------------------
    // Utility Functions
    // -------------------------------------------------------------------------

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

    /**
     * @brief Gets the CellMetadata for a cell.
     *
     * @param header Pointer to the cell header.
     * @return Pointer to the CellMetadata immediately after the header.
     */
    inline CellMetadata *get_metadata(CellHeader *header) {
        return reinterpret_cast<CellMetadata *>(reinterpret_cast<char *>(header) +
                                                sizeof(CellHeader));
    }

    /**
     * @brief Gets the start of allocatable blocks in a cell.
     *
     * @param header Pointer to the cell header.
     * @return Pointer to the first allocatable block.
     */
    inline void *get_block_start(CellHeader *header) {
        return reinterpret_cast<char *>(header) + kBlockStartOffset;
    }

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

}
