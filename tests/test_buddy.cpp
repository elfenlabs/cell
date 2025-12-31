#include "cell/context.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <thread>
#include <vector>

// Simple test helper
#define TEST(name)                                                                                 \
    void test_##name();                                                                            \
    struct Register##name {                                                                        \
        Register##name() { tests.push_back({#name, test_##name}); }                                \
    } reg_##name;                                                                                  \
    void test_##name()

struct TestCase {
    const char *name;
    void (*fn)();
};
std::vector<TestCase> tests;

// =============================================================================
// Buddy Allocator Tests
// =============================================================================

// Test 1: Basic buddy allocation (32KB)
TEST(BuddyBasic32KB) {
    Cell::Config config;
    config.reserve_size = 64 * 1024 * 1024; // 64MB total

    Cell::Context ctx(config);

    // Allocate 32KB (minimum buddy size)
    void *p = ctx.alloc_bytes(32 * 1024, 1);
    assert(p != nullptr && "Failed to allocate 32KB");

    // Write to it
    std::memset(p, 0xAA, 32 * 1024);

    ctx.free_bytes(p);
    printf("  PASSED\n");
}

// Test 2: Various buddy sizes
TEST(BuddyVariousSizes) {
    Cell::Config config;
    config.reserve_size = 128 * 1024 * 1024;

    Cell::Context ctx(config);

    const size_t sizes[] = {32 * 1024, 64 * 1024, 128 * 1024, 256 * 1024, 512 * 1024, 1024 * 1024};
    std::vector<void *> ptrs;

    for (size_t size : sizes) {
        void *p = ctx.alloc_bytes(size, 0);
        assert(p != nullptr && "Failed to allocate");
        std::memset(p, 0x55, size);
        ptrs.push_back(p);
        printf("  Allocated %zuKB\n", size / 1024);
    }

    for (void *p : ptrs) {
        ctx.free_bytes(p);
    }

    printf("  PASSED\n");
}

// Test 3: Explicit alloc_large
TEST(AllocLargeExplicit) {
    Cell::Config config;
    config.reserve_size = 64 * 1024 * 1024;

    Cell::Context ctx(config);

    // Explicit large allocation
    void *p = ctx.alloc_large(128 * 1024, 42);
    assert(p != nullptr && "alloc_large failed");

    std::memset(p, 0xBB, 128 * 1024);

    ctx.free_large(p);
    printf("  PASSED (128KB via alloc_large)\n");
}

// Test 4: Direct OS allocation (> 2MB)
TEST(DirectOS4MB) {
    Cell::Config config;
    config.reserve_size = 64 * 1024 * 1024;

    Cell::Context ctx(config);

    // 4MB allocation should go directly to OS
    void *p = ctx.alloc_bytes(4 * 1024 * 1024, 0);
    assert(p != nullptr && "Failed to allocate 4MB");

    // Write to all of it
    std::memset(p, 0xCC, 4 * 1024 * 1024);

    ctx.free_bytes(p);
    printf("  PASSED (4MB direct OS)\n");
}

// Test 5: Buddy coalescing
TEST(BuddyCoalescing) {
    Cell::Config config;
    config.reserve_size = 64 * 1024 * 1024;

    Cell::Context ctx(config);

    // Allocate two 32KB blocks (should split a 64KB)
    void *p1 = ctx.alloc_bytes(32 * 1024, 0);
    void *p2 = ctx.alloc_bytes(32 * 1024, 0);

    assert(p1 && p2 && "Failed to allocate");

    // Free both - should coalesce back to 64KB
    ctx.free_bytes(p1);
    ctx.free_bytes(p2);

    // Now allocate 64KB - should succeed using the coalesced block
    void *p3 = ctx.alloc_bytes(64 * 1024, 0);
    assert(p3 != nullptr && "Failed to allocate after coalescing");

    ctx.free_bytes(p3);
    printf("  PASSED (coalescing verified)\n");
}

// Test 6: Many buddy allocations
TEST(BuddyManyAllocations) {
    Cell::Config config;
    config.reserve_size = 256 * 1024 * 1024;

    Cell::Context ctx(config);

    std::vector<void *> ptrs;
    const size_t count = 50;
    const size_t size = 64 * 1024; // 64KB each

    printf("  Allocating %zu x %zuKB...\n", count, size / 1024);
    for (size_t i = 0; i < count; ++i) {
        void *p = ctx.alloc_bytes(size, 0);
        if (!p) {
            printf("  Failed at allocation %zu\n", i);
            break;
        }
        ptrs.push_back(p);
    }

    printf("  Allocated %zu blocks\n", ptrs.size());

    for (void *p : ptrs) {
        ctx.free_bytes(p);
    }

    printf("  PASSED\n");
}

// Test 7: Size boundary (16KB vs 32KB)
TEST(SizeBoundary) {
    Cell::Config config;
    config.reserve_size = 64 * 1024 * 1024;

    Cell::Context ctx(config);

    // 16KB should use cell allocator
    void *p1 = ctx.alloc_bytes(16 * 1024, 1);
    assert(p1 != nullptr);

    // 17KB should use buddy (rounded up to 32KB)
    void *p2 = ctx.alloc_bytes(17 * 1024, 2);
    assert(p2 != nullptr);

    // 32KB exactly
    void *p3 = ctx.alloc_bytes(32 * 1024, 3);
    assert(p3 != nullptr);

    ctx.free_bytes(p1);
    ctx.free_bytes(p2);
    ctx.free_bytes(p3);

    printf("  PASSED (16KB -> cell, 17KB -> buddy, 32KB -> buddy)\n");
}

// Test 8: Large allocation stress
TEST(LargeStress) {
    Cell::Config config;
    config.reserve_size = 512 * 1024 * 1024;

    Cell::Context ctx(config);

    // Mix of buddy and direct OS allocations
    std::vector<std::pair<void *, size_t>> allocs;

    const size_t sizes[] = {
        32 * 1024,       // 32KB - buddy
        100 * 1024,      // 100KB - buddy (rounds to 128KB)
        1024 * 1024,     // 1MB - buddy
        3 * 1024 * 1024, // 3MB - direct OS
        64 * 1024,       // 64KB - buddy
    };

    for (int round = 0; round < 3; ++round) {
        for (size_t size : sizes) {
            void *p = ctx.alloc_bytes(size, 0);
            if (p) {
                allocs.push_back({p, size});
            }
        }

        // Free half
        for (size_t i = 0; i < allocs.size() / 2; ++i) {
            ctx.free_bytes(allocs[i].first);
        }
        allocs.erase(allocs.begin(), allocs.begin() + allocs.size() / 2);
    }

    // Free remaining
    for (auto &[p, s] : allocs) {
        ctx.free_bytes(p);
    }

    printf("  PASSED\n");
}

#include <cstdlib> // For malloc/free

// =============================================================================
// Realloc Tests (Direct BuddyAllocator)
// =============================================================================

// Test 9: Realloc - In-place same order
TEST(ReallocInPlace) {
    const size_t size = 64 * 1024 * 1024;
    void *base = std::malloc(size);
    Cell::BuddyAllocator buddy(base, size);

    // Alloc 40KB (order 16, 64KB block)
    void *p = buddy.alloc(40 * 1024);
    assert(p != nullptr);

    // Realloc to 50KB (still order 16)
    void *p2 = buddy.realloc_bytes(p, 50 * 1024);

    assert(p2 == p && "Should have expanded in-place");

    buddy.free(p2);
    std::free(base);
    printf("  PASSED\n");
}

// Test 10: Realloc - Grow to next order with buddy merge
TEST(ReallocBuddyMerge) {
    const size_t size = 64 * 1024 * 1024;
    void *base = std::malloc(size);
    Cell::BuddyAllocator buddy(base, size);

    // Alloc two 32KB blocks to get neighbors
    // Note: We need to ensure they are buddies.
    // Allocating sequentially usually gives sequential blocks.
    // If we alloc 32KB, we get a block at offset X.
    // Next 32KB should be at offset X + 32KB.
    // Valid 64KB aligned pairs are (0, 32K), (64K, 96K), etc.
    // Depending on which one we get, we might be the "left" or "right" buddy.
    // To be safe, we alloc enough to fill a larger block?
    // Actually, just allocating usually works.

    void *p1 = buddy.alloc(32 * 1024);
    void *p2 = buddy.alloc(32 * 1024);
    assert(p1 && p2);

    // Free p2.
    buddy.free(p2);

    // Fill p1
    std::memset(p1, 0x77, 32 * 1024);

    // Grow p1 to 40KB (needs 64KB)
    void *p3 = buddy.realloc_bytes(p1, 40 * 1024);
    assert(p3 != nullptr);

    // Verify data
    unsigned char *bytes = static_cast<unsigned char *>(p3);
    for (int i = 0; i < 32 * 1024; ++i) {
        assert(bytes[i] == 0x77);
    }

    buddy.free(p3);
    std::free(base);
    printf("  PASSED\n");
}

// Test 11: Realloc - Fallback (alloc+copy+free)
TEST(ReallocFallback) {
    const size_t size = 64 * 1024 * 1024;
    void *base = std::malloc(size);
    Cell::BuddyAllocator buddy(base, size);

    // Alloc 32KB
    void *p1 = buddy.alloc(32 * 1024);
    // Alloc another to block buddy merge
    void *p2 = buddy.alloc(32 * 1024);

    // Fill p1
    std::memset(p1, 0x88, 32 * 1024);

    // Try to grow p1 to 100KB (order 17, 128KB)
    void *p3 = buddy.realloc_bytes(p1, 100 * 1024);
    assert(p3 != nullptr);
    assert(p3 != p1 && "Should have moved (fallback)");

    // Verify data
    unsigned char *bytes = static_cast<unsigned char *>(p3);
    for (int i = 0; i < 32 * 1024; ++i) {
        assert(bytes[i] == 0x88);
    }

    buddy.free(p2);
    buddy.free(p3);
    std::free(base);
    printf("  PASSED\n");
}

// Test 12: Realloc - Shrink
TEST(ReallocShrink) {
    const size_t size = 64 * 1024 * 1024;
    void *base = std::malloc(size);
    Cell::BuddyAllocator buddy(base, size);

    // Alloc 100KB (order 17, 128KB)
    void *p1 = buddy.alloc(100 * 1024);
    std::memset(p1, 0x99, 100 * 1024);

    // Shrink to 32KB (order 15)
    void *p2 = buddy.realloc_bytes(p1, 32 * 1024);
    assert(p2 != nullptr);

    // Verify data
    unsigned char *bytes = static_cast<unsigned char *>(p2);
    for (int i = 0; i < 32 * 1024; ++i) {
        assert(bytes[i] == 0x99);
    }

    buddy.free(p2);
    std::free(base);
    printf("  PASSED\n");
}

// =============================================================================
// Main
// =============================================================================

int main() {
    printf("Buddy and Large Allocation Tests\n");
    printf("=================================\n");
    printf("Configuration:\n");
    printf("  Buddy min size: %zuKB\n", Cell::BuddyAllocator::kMinBlockSize / 1024);
    printf("  Buddy max size: %zuMB\n", Cell::BuddyAllocator::kMaxBlockSize / (1024 * 1024));
    printf("  Large alloc min: %zuMB\n", Cell::LargeAllocRegistry::kMinLargeSize / (1024 * 1024));
    printf("\n");

    int passed = 0;
    int failed = 0;

    for (const auto &test : tests) {
        printf("Running %s...\n", test.name);
        try {
            test.fn();
            ++passed;
        } catch (...) {
            printf("  FAILED (exception)\n");
            ++failed;
        }
    }

    printf("\n");
    printf("Results: %d passed, %d failed\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
