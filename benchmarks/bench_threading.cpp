#include <benchmark/benchmark.h>
#include <cell/context.h>

#include <atomic>
#include <thread>
#include <vector>

// =============================================================================
// Multi-Threaded Benchmarks
// Tests concurrent allocation performance and scalability.
// =============================================================================

// Shared context for contention tests
static Cell::Context *g_shared_ctx = nullptr;

static void BM_Cell_Parallel_Small_64B(benchmark::State &state) {
    // First thread creates the shared context
    if (state.thread_index() == 0) {
        g_shared_ctx = new Cell::Context();
    }

    for (auto _ : state) {
        void *ptr = g_shared_ctx->alloc_bytes(64);
        benchmark::DoNotOptimize(ptr);
        g_shared_ctx->free_bytes(ptr);
    }

    // Last thread cleans up
    if (state.thread_index() == 0) {
        delete g_shared_ctx;
        g_shared_ctx = nullptr;
    }

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Cell_Parallel_Small_64B)->Threads(1)->Threads(2)->Threads(4)->Threads(8);

static void BM_Cell_Parallel_Medium_1KB(benchmark::State &state) {
    if (state.thread_index() == 0) {
        g_shared_ctx = new Cell::Context();
    }

    for (auto _ : state) {
        void *ptr = g_shared_ctx->alloc_bytes(1024);
        benchmark::DoNotOptimize(ptr);
        g_shared_ctx->free_bytes(ptr);
    }

    if (state.thread_index() == 0) {
        delete g_shared_ctx;
        g_shared_ctx = nullptr;
    }

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Cell_Parallel_Medium_1KB)->Threads(1)->Threads(2)->Threads(4)->Threads(8);

// =============================================================================
// Thread-Local Context (No Contention)
// Each thread has its own context - measures pure allocation speed.
// =============================================================================

static void BM_Cell_ThreadLocal_64B(benchmark::State &state) {
    Cell::Context ctx; // Each thread gets its own

    for (auto _ : state) {
        void *ptr = ctx.alloc_bytes(64);
        benchmark::DoNotOptimize(ptr);
        ctx.free_bytes(ptr);
    }

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Cell_ThreadLocal_64B)->Threads(1)->Threads(2)->Threads(4)->Threads(8);

// =============================================================================
// High Contention: Batch allocations with shared context
// =============================================================================

static void BM_Cell_Parallel_Batch(benchmark::State &state) {
    if (state.thread_index() == 0) {
        g_shared_ctx = new Cell::Context();
    }

    const size_t batch_size = 100;
    std::vector<void *> ptrs(batch_size);

    for (auto _ : state) {
        for (size_t i = 0; i < batch_size; ++i) {
            ptrs[i] = g_shared_ctx->alloc_bytes(64);
        }
        benchmark::DoNotOptimize(ptrs.data());

        for (size_t i = 0; i < batch_size; ++i) {
            g_shared_ctx->free_bytes(ptrs[i]);
        }
    }

    if (state.thread_index() == 0) {
        delete g_shared_ctx;
        g_shared_ctx = nullptr;
    }

    state.SetItemsProcessed(state.iterations() * batch_size);
}
BENCHMARK(BM_Cell_Parallel_Batch)->Threads(1)->Threads(2)->Threads(4)->Threads(8);

// =============================================================================
// Mixed Size Parallel
// =============================================================================

static void BM_Cell_Parallel_MixedSizes(benchmark::State &state) {
    if (state.thread_index() == 0) {
        g_shared_ctx = new Cell::Context();
    }

    // Different size per thread to reduce bin contention
    const size_t sizes[] = {16, 32, 64, 128, 256, 512, 1024, 2048};
    const size_t size = sizes[state.thread_index() % 8];

    for (auto _ : state) {
        void *ptr = g_shared_ctx->alloc_bytes(size);
        benchmark::DoNotOptimize(ptr);
        g_shared_ctx->free_bytes(ptr);
    }

    if (state.thread_index() == 0) {
        delete g_shared_ctx;
        g_shared_ctx = nullptr;
    }

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Cell_Parallel_MixedSizes)->Threads(1)->Threads(2)->Threads(4)->Threads(8);

// =============================================================================
// Baseline: malloc parallel comparison
// =============================================================================

static void BM_Malloc_Parallel_64B(benchmark::State &state) {
    for (auto _ : state) {
        void *ptr = std::malloc(64);
        benchmark::DoNotOptimize(ptr);
        std::free(ptr);
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Malloc_Parallel_64B)->Threads(1)->Threads(2)->Threads(4)->Threads(8);

static void BM_Malloc_Parallel_1KB(benchmark::State &state) {
    for (auto _ : state) {
        void *ptr = std::malloc(1024);
        benchmark::DoNotOptimize(ptr);
        std::free(ptr);
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Malloc_Parallel_1KB)->Threads(1)->Threads(2)->Threads(4)->Threads(8);
