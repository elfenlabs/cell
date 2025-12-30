#include "cell/context.h"

#include <cassert>
#include <cstring>

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace Cell {

    Context::Context(const Config &config) : m_reserved_size(config.reserve_size) {
#if defined(_WIN32)
        m_base = VirtualAlloc(nullptr, m_reserved_size, MEM_RESERVE, PAGE_NOACCESS);
#else
        m_base = mmap(nullptr, m_reserved_size, PROT_NONE,
                      MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
        if (m_base == MAP_FAILED) {
            m_base = nullptr;
        }
#endif
        if (m_base) {
            m_allocator = std::make_unique<Allocator>(m_base, m_reserved_size);
        }

        // Initialize bins (already zero-initialized, but be explicit)
        for (size_t i = 0; i < kNumSizeBins; ++i) {
            m_bins[i].partial_head = nullptr;
            m_bins[i].warm_cell_count = 0;
            m_bins[i].total_allocated = 0;
            m_bins[i].current_allocated = 0;
        }
    }

    Context::~Context() {
        if (m_base) {
#if defined(_WIN32)
            VirtualFree(m_base, 0, MEM_RELEASE);
#else
            munmap(m_base, m_reserved_size);
#endif
        }
    }

    // =========================================================================
    // Sub-Cell Allocation API
    // =========================================================================

    void *Context::alloc_bytes(size_t size, uint8_t tag, size_t alignment) {
        if (!m_allocator || size == 0) {
            return nullptr;
        }

        // Determine size class
        uint8_t bin_index = get_size_class(size, alignment);

        if (bin_index == kFullCellMarker) {
            // Too large for sub-cell, use full cell allocation
            CellData *cell = alloc_cell(tag);
            if (cell) {
                cell->header.size_class = kFullCellMarker;
            }
            return cell;
        }

        // Allocate from size class bin
        return alloc_from_bin(bin_index, tag);
    }

    void Context::free_bytes(void *ptr) {
        if (!ptr) {
            return;
        }

        // Get header to determine allocation type
        CellHeader *header = get_header(ptr);

        if (header->size_class == kFullCellMarker) {
            // Full-cell allocation
            free_cell(reinterpret_cast<CellData *>(header));
        } else {
            // Sub-cell allocation
            free_to_bin(ptr, header);
        }
    }

    // =========================================================================
    // Cell-Level API
    // =========================================================================

    CellData *Context::alloc_cell(uint8_t tag) {
        if (!m_allocator) {
            return nullptr;
        }

        void *ptr = m_allocator->alloc();
        if (!ptr) {
            return nullptr;
        }

        auto *cell = static_cast<CellData *>(ptr);
        cell->header.tag = tag;
        cell->header.size_class = kFullCellMarker;
        cell->header.free_count = 0;
        return cell;
    }

    void Context::free_cell(CellData *cell) {
        if (m_allocator && cell) {
            m_allocator->free(cell);
        }
    }

    // =========================================================================
    // Sub-Cell Implementation
    // =========================================================================

    void *Context::alloc_from_bin(size_t bin_index, uint8_t tag) {
        assert(bin_index < kNumSizeBins);

        std::lock_guard<std::mutex> lock(m_bin_locks[bin_index]);
        SizeBin &bin = m_bins[bin_index];

        // Try to allocate from a partial cell
        if (bin.partial_head) {
            CellHeader *cell_header = bin.partial_head;
            CellMetadata *metadata = get_metadata(cell_header);

            // Pop a block from the free list
            FreeBlock *block = metadata->free_list;
            assert(block && "Partial cell should have free blocks");
            metadata->free_list = block->next;
            cell_header->free_count--;

            // If cell is now full, remove from partial list
            if (cell_header->free_count == 0) {
                bin.partial_head = reinterpret_cast<CellHeader *>(metadata->next_partial);
                metadata->next_partial = nullptr;
            }

            // Update stats
            bin.total_allocated++;
            bin.current_allocated++;

            return block;
        }

        // No partial cells available, get a fresh cell
        void *raw_cell = m_allocator->alloc();
        if (!raw_cell) {
            return nullptr;
        }

        // Initialize the cell for this bin
        init_cell_for_bin(raw_cell, bin_index, tag);

        CellHeader *cell_header = static_cast<CellHeader *>(raw_cell);
        CellMetadata *metadata = get_metadata(cell_header);

        // Pop the first block
        FreeBlock *block = metadata->free_list;
        metadata->free_list = block->next;
        cell_header->free_count--;

        // Add to partial list (if there are still free blocks)
        if (cell_header->free_count > 0) {
            metadata->next_partial = reinterpret_cast<CellHeader *>(bin.partial_head);
            bin.partial_head = cell_header;
        }

        // Update stats
        bin.total_allocated++;
        bin.current_allocated++;

        return block;
    }

    void Context::free_to_bin(void *ptr, CellHeader *header) {
        size_t bin_index = header->size_class;
        assert(bin_index < kNumSizeBins);

        size_t block_size = kSizeClasses[bin_index];

#ifndef NDEBUG
        // Poison the freed memory
        std::memset(ptr, kPoisonByte, block_size);
#endif

        std::lock_guard<std::mutex> lock(m_bin_locks[bin_index]);
        SizeBin &bin = m_bins[bin_index];
        CellMetadata *metadata = get_metadata(header);

        // Check if cell was full (not in partial list)
        bool was_full = (header->free_count == 0);

        // Add block back to cell's free list
        auto *block = static_cast<FreeBlock *>(ptr);
        block->next = metadata->free_list;
        metadata->free_list = block;
        header->free_count++;

        // Update stats
        bin.current_allocated--;

        // Calculate max blocks for this bin
        size_t max_blocks = blocks_per_cell(bin_index);

        // If cell is now completely empty
        if (header->free_count == max_blocks) {
            // Warm reserve policy: keep a few empty cells per bin
            if (bin.warm_cell_count < kWarmCellsPerBin) {
                // Keep as warm reserve, stays in partial list
                bin.warm_cell_count++;
                if (was_full) {
                    // Add to partial list
                    metadata->next_partial = reinterpret_cast<CellHeader *>(bin.partial_head);
                    bin.partial_head = header;
                }
            } else {
                // Return cell to allocator
                // First, remove from partial list
                CellHeader **pp = &bin.partial_head;
                while (*pp && *pp != header) {
                    pp = reinterpret_cast<CellHeader **>(&get_metadata(*pp)->next_partial);
                }
                if (*pp == header) {
                    *pp = reinterpret_cast<CellHeader *>(metadata->next_partial);
                }
                metadata->next_partial = nullptr;

                // Return to allocator
                m_allocator->free(header);
            }
        } else if (was_full) {
            // Cell was full, now has space - add to partial list
            metadata->next_partial = reinterpret_cast<CellHeader *>(bin.partial_head);
            bin.partial_head = header;
        }
        // Otherwise cell is already in partial list, nothing to do
    }

    void Context::init_cell_for_bin(void *cell, size_t bin_index, uint8_t tag) {
        auto *header = static_cast<CellHeader *>(cell);
        CellMetadata *metadata = get_metadata(header);

        // Set up header
        header->tag = tag;
        header->size_class = static_cast<uint8_t>(bin_index);

#ifndef NDEBUG
        header->magic = kCellMagic;
        header->generation = 0;
#endif

        // Calculate block layout
        size_t block_size = kSizeClasses[bin_index];
        size_t num_blocks = blocks_per_cell(bin_index);
        header->free_count = static_cast<uint16_t>(num_blocks);

        // Initialize metadata
        metadata->next_partial = nullptr;
        metadata->free_list = nullptr;

        // Build free list (all blocks are free initially)
        char *block_start = static_cast<char *>(get_block_start(header));
        FreeBlock *prev = nullptr;

        for (size_t i = num_blocks; i > 0; --i) {
            auto *block = reinterpret_cast<FreeBlock *>(block_start + (i - 1) * block_size);
            block->next = prev;
            prev = block;
        }

        metadata->free_list = prev;
    }

}
