#include "cell/context.h"

#include <cassert>
#include <cstdio>
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
// Budget Tests (only run when CELL_ENABLE_BUDGET is defined)
// =============================================================================

#ifdef CELL_ENABLE_BUDGET

// Test 1: Budget enforcement
TEST(BudgetEnforcement) {
    Cell::Config config;
    config.reserve_size = 64 * 1024 * 1024;
    config.memory_budget = 1024; // 1KB budget

    Cell::Context ctx(config);

    // First allocation should succeed (512 bytes)
    void *p1 = ctx.alloc_bytes(512);
    assert(p1 != nullptr && "First allocation should succeed");

    // Second allocation should succeed (400 bytes, total 912)
    void *p2 = ctx.alloc_bytes(400);
    assert(p2 != nullptr && "Second allocation should succeed");

    // Third allocation should FAIL (exceeds budget)
    void *p3 = ctx.alloc_bytes(200);
    assert(p3 == nullptr && "Third allocation should fail (budget exceeded)");

    // Free one and try again
    ctx.free_bytes(p1);

    // Now should succeed
    void *p4 = ctx.alloc_bytes(200);
    assert(p4 != nullptr && "Allocation should succeed after free");

    ctx.free_bytes(p2);
    ctx.free_bytes(p4);

    printf("  PASSED\n");
}

// Test 2: Budget callback
static bool g_callback_invoked = false;
static size_t g_callback_requested = 0;
static size_t g_callback_budget = 0;
static size_t g_callback_current = 0;

void budget_callback(size_t requested, size_t budget, size_t current) {
    g_callback_invoked = true;
    g_callback_requested = requested;
    g_callback_budget = budget;
    g_callback_current = current;
}

TEST(BudgetCallback) {
    Cell::Config config;
    config.reserve_size = 64 * 1024 * 1024;
    config.memory_budget = 512;

    Cell::Context ctx(config);
    ctx.set_budget_callback(budget_callback);

    g_callback_invoked = false;

    // Fill up budget
    void *p1 = ctx.alloc_bytes(400);
    assert(p1 != nullptr);
    assert(!g_callback_invoked && "Callback should not be invoked yet");

    // This should trigger callback
    void *p2 = ctx.alloc_bytes(200);
    assert(p2 == nullptr && "Allocation should fail");
    assert(g_callback_invoked && "Callback should be invoked");
    assert(g_callback_requested == 200 && "Callback should receive requested size");
    assert(g_callback_budget == 512 && "Callback should receive budget");

    printf("  Callback: requested=%zu, budget=%zu, current=%zu\n", g_callback_requested,
           g_callback_budget, g_callback_current);

    ctx.free_bytes(p1);

    printf("  PASSED\n");
}

// Test 3: Unlimited budget (zero)
TEST(BudgetUnlimited) {
    Cell::Config config;
    config.reserve_size = 64 * 1024 * 1024;
    config.memory_budget = 0; // Unlimited

    Cell::Context ctx(config);

    // Should be able to allocate freely
    std::vector<void *> ptrs;
    for (int i = 0; i < 100; ++i) {
        void *p = ctx.alloc_bytes(1024);
        assert(p != nullptr && "Unlimited budget should allow allocations");
        ptrs.push_back(p);
    }

    for (void *p : ptrs) {
        ctx.free_bytes(p);
    }

    printf("  PASSED\n");
}

// Test 4: Budget with large allocations
// Note: Budget check uses requested size, but tracking uses rounded size.
// This means you can slightly exceed budget if the rounded size is larger.
TEST(BudgetLargeAllocs) {
    Cell::Config config;
    config.reserve_size = 128 * 1024 * 1024;
    // Buddy allocations round up: 256KB request -> 512KB block
    // Use a budget that clearly tests the limits
    config.memory_budget = 2 * 1024 * 1024; // 2MB budget

    Cell::Context ctx(config);

    // Allocate 512KB (buddy allocation -> uses 1MB block due to +8 header)
    void *p1 = ctx.alloc_bytes(512 * 1024);
    assert(p1 != nullptr && "First buddy allocation should succeed");
    printf("  After 512KB alloc: usage = %zuKB\n", ctx.get_budget_current() / 1024);

    // Allocate another 512KB (-> another 1MB block)
    void *p2 = ctx.alloc_bytes(512 * 1024);
    assert(p2 != nullptr && "Second buddy allocation should succeed");
    printf("  After 512KB alloc: usage = %zuKB\n", ctx.get_budget_current() / 1024);

    // This should fail - budget is 2MB and we've used 2MB
    void *p3 = ctx.alloc_bytes(512 * 1024);
    assert(p3 == nullptr && "Third allocation should fail (budget exceeded)");

    ctx.free_bytes(p1);
    ctx.free_bytes(p2);

    printf("  PASSED\n");
}

// Test 5: Runtime budget change
TEST(BudgetRuntimeChange) {
    Cell::Config config;
    config.reserve_size = 64 * 1024 * 1024;
    config.memory_budget = 512;

    Cell::Context ctx(config);

    // Fill up initial budget
    void *p1 = ctx.alloc_bytes(400);
    assert(p1 != nullptr);

    void *p2 = ctx.alloc_bytes(200);
    assert(p2 == nullptr && "Should fail with initial budget");

    // Increase budget at runtime
    ctx.set_budget(2048);
    assert(ctx.get_budget() == 2048);

    // Now should succeed
    void *p3 = ctx.alloc_bytes(200);
    assert(p3 != nullptr && "Should succeed with increased budget");

    ctx.free_bytes(p1);
    ctx.free_bytes(p3);

    printf("  PASSED\n");
}

#else

// When budget is disabled, just report that
TEST(BudgetDisabled) {
    printf("  CELL_ENABLE_BUDGET not defined, budget tests skipped\n");
    printf("  PASSED\n");
}

#endif

// =============================================================================
// Main
// =============================================================================

int main() {
    printf("Memory Budget Tests\n");
    printf("===================\n");
#ifdef CELL_ENABLE_BUDGET
    printf("CELL_ENABLE_BUDGET: ENABLED\n");
#else
    printf("CELL_ENABLE_BUDGET: DISABLED\n");
#endif
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
