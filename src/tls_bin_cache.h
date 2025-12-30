#pragma once

#include "cell/cell.h"
#include "cell/config.h"

namespace Cell {

    /**
     * @brief Per-thread cache for sub-cell blocks (bins 0-3 only).
     *
     * Fixed-size array, no locking required.
     * Stores FreeBlock pointers for fast alloc/free on hot sizes.
     */
    struct TlsBinCache {
        FreeBlock *blocks[kTlsBinCacheCapacity] = {};
        size_t count = 0;

        [[nodiscard]] bool is_empty() const { return count == 0; }
        [[nodiscard]] bool is_full() const { return count >= kTlsBinCacheCapacity; }

        void push(FreeBlock *b) { blocks[count++] = b; }
        [[nodiscard]] FreeBlock *pop() { return blocks[--count]; }
    };

    /**
     * @brief Thread-local bin caches for sizes 16B, 32B, 64B, 128B.
     *
     * Index 0 = bin 0 (16B), index 1 = bin 1 (32B), etc.
     */
    inline thread_local TlsBinCache t_bin_cache[kTlsBinCacheCount];

}
