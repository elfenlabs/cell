#include <benchmark/benchmark.h>

#include <cstdlib>
#include <random>
#include <vector>

// =============================================================================
// Baseline: System malloc/free
// These benchmarks mirror bench_alloc_patterns.cpp to enable direct comparison.
// =============================================================================

// =============================================================================
// Small Allocations
// =============================================================================

static void BM_Malloc_Small_16B(benchmark::State &state) {
    for (auto _ : state) {
        void *ptr = std::malloc(16);
        benchmark::DoNotOptimize(ptr);
        std::free(ptr);
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Malloc_Small_16B);

static void BM_Malloc_Small_64B(benchmark::State &state) {
    for (auto _ : state) {
        void *ptr = std::malloc(64);
        benchmark::DoNotOptimize(ptr);
        std::free(ptr);
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Malloc_Small_64B);

static void BM_Malloc_Small_128B(benchmark::State &state) {
    for (auto _ : state) {
        void *ptr = std::malloc(128);
        benchmark::DoNotOptimize(ptr);
        std::free(ptr);
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Malloc_Small_128B);

// =============================================================================
// Medium Allocations
// =============================================================================

static void BM_Malloc_Medium_512B(benchmark::State &state) {
    for (auto _ : state) {
        void *ptr = std::malloc(512);
        benchmark::DoNotOptimize(ptr);
        std::free(ptr);
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Malloc_Medium_512B);

static void BM_Malloc_Medium_1KB(benchmark::State &state) {
    for (auto _ : state) {
        void *ptr = std::malloc(1024);
        benchmark::DoNotOptimize(ptr);
        std::free(ptr);
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Malloc_Medium_1KB);

static void BM_Malloc_Medium_4KB(benchmark::State &state) {
    for (auto _ : state) {
        void *ptr = std::malloc(4096);
        benchmark::DoNotOptimize(ptr);
        std::free(ptr);
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Malloc_Medium_4KB);

static void BM_Malloc_Medium_16KB(benchmark::State &state) {
    for (auto _ : state) {
        void *ptr = std::malloc(16 * 1024);
        benchmark::DoNotOptimize(ptr);
        std::free(ptr);
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Malloc_Medium_16KB);

// =============================================================================
// Large Allocations (Buddy-equivalent range)
// =============================================================================

static void BM_Malloc_Buddy_64KB(benchmark::State &state) {
    for (auto _ : state) {
        void *ptr = std::malloc(64 * 1024);
        benchmark::DoNotOptimize(ptr);
        std::free(ptr);
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Malloc_Buddy_64KB);

static void BM_Malloc_Buddy_256KB(benchmark::State &state) {
    for (auto _ : state) {
        void *ptr = std::malloc(256 * 1024);
        benchmark::DoNotOptimize(ptr);
        std::free(ptr);
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Malloc_Buddy_256KB);

static void BM_Malloc_Buddy_1MB(benchmark::State &state) {
    for (auto _ : state) {
        void *ptr = std::malloc(1024 * 1024);
        benchmark::DoNotOptimize(ptr);
        std::free(ptr);
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Malloc_Buddy_1MB);

// =============================================================================
// Very Large Allocations
// =============================================================================

static void BM_Malloc_Large_4MB(benchmark::State &state) {
    for (auto _ : state) {
        void *ptr = std::malloc(4 * 1024 * 1024);
        benchmark::DoNotOptimize(ptr);
        std::free(ptr);
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Malloc_Large_4MB);

// =============================================================================
// Batch Allocation Patterns
// =============================================================================

static void BM_Malloc_BatchAlloc_64B(benchmark::State &state) {
    const size_t batch_size = 1000;
    std::vector<void *> ptrs(batch_size);

    for (auto _ : state) {
        for (size_t i = 0; i < batch_size; ++i) {
            ptrs[i] = std::malloc(64);
        }
        benchmark::DoNotOptimize(ptrs.data());

        for (size_t i = 0; i < batch_size; ++i) {
            std::free(ptrs[i]);
        }
    }
    state.SetItemsProcessed(state.iterations() * batch_size);
}
BENCHMARK(BM_Malloc_BatchAlloc_64B);

static void BM_Malloc_BatchAlloc_1KB(benchmark::State &state) {
    const size_t batch_size = 1000;
    std::vector<void *> ptrs(batch_size);

    for (auto _ : state) {
        for (size_t i = 0; i < batch_size; ++i) {
            ptrs[i] = std::malloc(1024);
        }
        benchmark::DoNotOptimize(ptrs.data());

        for (size_t i = 0; i < batch_size; ++i) {
            std::free(ptrs[i]);
        }
    }
    state.SetItemsProcessed(state.iterations() * batch_size);
}
BENCHMARK(BM_Malloc_BatchAlloc_1KB);

// =============================================================================
// Mixed Size Patterns
// =============================================================================

static void BM_Malloc_MixedSizes(benchmark::State &state) {
    std::mt19937 rng(42);
    std::uniform_int_distribution<size_t> size_dist(16, 4096);

    const size_t batch_size = 100;
    std::vector<void *> ptrs(batch_size);
    std::vector<size_t> sizes(batch_size);

    for (auto _ : state) {
        state.PauseTiming();
        for (size_t i = 0; i < batch_size; ++i) {
            sizes[i] = size_dist(rng);
        }
        state.ResumeTiming();

        for (size_t i = 0; i < batch_size; ++i) {
            ptrs[i] = std::malloc(sizes[i]);
        }
        benchmark::DoNotOptimize(ptrs.data());

        for (size_t i = 0; i < batch_size; ++i) {
            std::free(ptrs[i]);
        }
    }
    state.SetItemsProcessed(state.iterations() * batch_size);
}
BENCHMARK(BM_Malloc_MixedSizes);

// =============================================================================
// Realloc Pattern
// =============================================================================

static void BM_Malloc_Realloc_Growth(benchmark::State &state) {
    for (auto _ : state) {
        void *ptr = std::malloc(16);
        for (size_t size = 32; size <= 4096; size *= 2) {
            ptr = std::realloc(ptr, size);
            benchmark::DoNotOptimize(ptr);
        }
        std::free(ptr);
    }
    state.SetItemsProcessed(state.iterations() * 8);
}
BENCHMARK(BM_Malloc_Realloc_Growth);
