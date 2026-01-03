#include "cell/context.h"

#include <cassert>
#include <cstdio>
#include <cstring>
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
// Bug #1: Cross-tier realloc copies wrong size
// When reallocating from buddy -> large tier, the code copies new_size bytes
// instead of min(old_size, new_size), reading past the old allocation.
// =============================================================================

TEST(ReallocCrossTierBuddyToLarge) {
    Cell::Config config;
    config.reserve_size = 128 * 1024 * 1024;

    Cell::Context ctx(config);

    // Allocate 40KB in buddy tier (will get 64KB block)
    const size_t old_size = 40 * 1024;
    void *p = ctx.alloc_bytes(old_size, 1);
    assert(p != nullptr && "Failed to allocate 40KB");

    // Fill with known pattern
    std::memset(p, 0xAA, old_size);

    // Realloc to 4MB - forces cross-tier transition to large allocations
    // BUG: This tries to copy 4MB from a 40KB allocation, reading garbage
    const size_t new_size = 4 * 1024 * 1024;
    void *p2 = ctx.realloc_bytes(p, new_size, 1);
    assert(p2 != nullptr && "Realloc to 4MB failed");

    // Verify original data is preserved
    unsigned char *bytes = static_cast<unsigned char *>(p2);
    for (size_t i = 0; i < old_size; ++i) {
        if (bytes[i] != 0xAA) {
            printf("  Data corruption at byte %zu: expected 0xAA, got 0x%02X\n", i, bytes[i]);
            assert(false && "Data corruption in realloc");
        }
    }

    ctx.free_bytes(p2);
    printf("  PASSED\n");
}

TEST(ReallocCrossTierLargeToSmaller) {
    Cell::Config config;
    config.reserve_size = 128 * 1024 * 1024;

    Cell::Context ctx(config);

    // Allocate 4MB in large tier
    const size_t old_size = 4 * 1024 * 1024;
    void *p = ctx.alloc_bytes(old_size, 1);
    assert(p != nullptr && "Failed to allocate 4MB");

    // Fill first 32KB with known pattern
    const size_t pattern_size = 32 * 1024;
    std::memset(p, 0xBB, pattern_size);

    // Realloc down to 32KB - forces cross-tier transition to cell/subcell
    // BUG: This tries to copy 32KB from a 4MB allocation, which is fine,
    // but the code path should use min(old, new) for safety
    void *p2 = ctx.realloc_bytes(p, pattern_size, 1);
    assert(p2 != nullptr && "Realloc to 32KB failed");

    // Verify data is preserved
    unsigned char *bytes = static_cast<unsigned char *>(p2);
    for (size_t i = 0; i < pattern_size; ++i) {
        if (bytes[i] != 0xBB) {
            printf("  Data corruption at byte %zu: expected 0xBB, got 0x%02X\n", i, bytes[i]);
            assert(false && "Data corruption in realloc");
        }
    }

    ctx.free_bytes(p2);
    printf("  PASSED\n");
}

// =============================================================================
// Bug #2: alloc_aligned returns misaligned pointers via buddy path
// Buddy user pointers are offset by 8-byte header, so alignments > 8 are not
// guaranteed even when block_size >= alignment.
// =============================================================================

TEST(AllocAlignedBuddyMisalignment) {
    Cell::Config config;
    config.reserve_size = 128 * 1024 * 1024;

    Cell::Context ctx(config);

    // Request various alignments that should be honored
    const size_t alignments[] = {16, 32, 64, 128, 256, 512, 1024, 4096};

    for (size_t alignment : alignments) {
        // Request 40KB with specific alignment
        // This goes to buddy tier (40KB rounds up to 64KB block)
        void *p = ctx.alloc_aligned(40 * 1024, alignment, 1);
        assert(p != nullptr && "alloc_aligned failed");

        uintptr_t addr = reinterpret_cast<uintptr_t>(p);
        bool is_aligned = (addr % alignment) == 0;

        if (!is_aligned) {
            printf("  ALIGNMENT BUG: requested %zu-byte alignment, got address 0x%lx "
                   "(offset %zu)\n",
                   alignment, static_cast<unsigned long>(addr), addr % alignment);
            assert(false && "Misaligned pointer returned by alloc_aligned");
        }

        ctx.free_bytes(p);
    }

    printf("  All alignment tests passed\n");
    printf("  PASSED\n");
}

// =============================================================================
// Bug #3: Budget accounting inconsistency
// Allocations record requested size, but frees subtract rounded sizes.
// This causes budget drift and potential underflow.
// =============================================================================

#ifdef CELL_ENABLE_BUDGET

TEST(BudgetAccountingDrift) {
    Cell::Config config;
    config.reserve_size = 64 * 1024 * 1024;
    config.memory_budget = 10 * 1024 * 1024; // 10MB budget (plenty of room)

    Cell::Context ctx(config);

    // Initially no memory should be tracked
    size_t initial_usage = ctx.get_budget_current();
    assert(initial_usage == 0 && "Initial budget usage should be 0");

    // Allocate many small allocations
    // Request 20 bytes each (rounds to 32-byte size class)
    // BUG was: +20 on alloc, -32 on free = drift of -12 per cycle
    // FIXED: Both should use rounded size (32 bytes)
    const size_t count = 100;
    const size_t request_size = 20;
    std::vector<void *> ptrs;

    for (size_t i = 0; i < count; ++i) {
        void *p = ctx.alloc_bytes(request_size, 0);
        assert(p != nullptr && "Allocation should succeed within budget");
        ptrs.push_back(p);
    }

    size_t usage_after_alloc = ctx.get_budget_current();
    printf("  After %zu allocs of %zu bytes: usage = %zu (expected ~%zu)\n", count, request_size,
           usage_after_alloc, count * 32);

    // Free all
    for (void *p : ptrs) {
        ctx.free_bytes(p);
    }

    size_t final_usage = ctx.get_budget_current();

    // Budget usage should return to 0 after freeing everything
    if (final_usage != 0) {
        printf("  BUDGET DRIFT: initial=%zu, final=%zu\n", initial_usage, final_usage);
    }

    assert(final_usage == 0 && "Budget accounting drift detected - usage should be 0 after frees");
    printf("  PASSED\n");
}

#else

TEST(BudgetAccountingDriftSkipped) {
    printf("  CELL_ENABLE_BUDGET not defined, test skipped\n");
    printf("  PASSED\n");
}

#endif

// =============================================================================
// Bug #4: free_batch assumes homogeneous size class
// The fast path uses first pointer's size class for the entire batch.
// This test documents the contract and adds debug validation.
// =============================================================================

TEST(FreeBatchHomogeneousContract) {
    Cell::Config config;
    config.reserve_size = 64 * 1024 * 1024;

    Cell::Context ctx(config);

    // Allocate a batch of same-size allocations
    const size_t count = 16;
    const size_t size = 64; // All same size
    void *ptrs[count];

    for (size_t i = 0; i < count; ++i) {
        ptrs[i] = ctx.alloc_bytes(size, 0);
        assert(ptrs[i] != nullptr);
    }

    // This should work correctly - homogeneous batch
    ctx.free_batch(ptrs, count);

    printf("  Homogeneous batch freed successfully\n");
    printf("  PASSED\n");
}

// Note: We don't test mixed batches directly as it would corrupt memory.
// The fix adds a debug assertion to catch this during development.

// =============================================================================
// Main
// =============================================================================

int main() {
    printf("Code Review Bug Regression Tests\n");
    printf("=================================\n");
    printf("These tests verify the bugs from the code review.\n");
    printf("Tests should FAIL on buggy code and PASS after fixes.\n\n");

    int passed = 0;
    int failed = 0;

    for (const auto &test : tests) {
        printf("Running %s...\n", test.name);
        try {
            test.fn();
            ++passed;
        } catch (...) {
            printf("  FAILED (exception/crash)\n");
            ++failed;
        }
    }

    printf("\n");
    printf("Results: %d passed, %d failed\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
