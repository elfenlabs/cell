#include <benchmark/benchmark.h>
#include <cell/arena.h>
#include <cell/context.h>
#include <cell/pool.h>

#include <vector>

// =============================================================================
// Arena Benchmarks
// =============================================================================

static void BM_Arena_Linear_64B(benchmark::State &state) {
    Cell::Context ctx;

    for (auto _ : state) {
        Cell::Arena arena(ctx);
        for (int i = 0; i < 1000; ++i) {
            void *ptr = arena.alloc(64);
            benchmark::DoNotOptimize(ptr);
        }
        // Arena automatically freed at scope end
    }
    state.SetItemsProcessed(state.iterations() * 1000);
}
BENCHMARK(BM_Arena_Linear_64B);

static void BM_Arena_Linear_Mixed(benchmark::State &state) {
    Cell::Context ctx;

    for (auto _ : state) {
        Cell::Arena arena(ctx);
        for (int i = 0; i < 100; ++i) {
            void *p1 = arena.alloc(16);
            void *p2 = arena.alloc(64);
            void *p3 = arena.alloc(256);
            void *p4 = arena.alloc(1024);
            benchmark::DoNotOptimize(p1);
            benchmark::DoNotOptimize(p2);
            benchmark::DoNotOptimize(p3);
            benchmark::DoNotOptimize(p4);
        }
    }
    state.SetItemsProcessed(state.iterations() * 400);
}
BENCHMARK(BM_Arena_Linear_Mixed);

static void BM_Arena_Reset(benchmark::State &state) {
    Cell::Context ctx;
    Cell::Arena arena(ctx);

    for (auto _ : state) {
        // Allocate a bunch
        for (int i = 0; i < 100; ++i) {
            void *ptr = arena.alloc(64);
            benchmark::DoNotOptimize(ptr);
        }
        // Reset (bulk free)
        arena.reset();
    }
    state.SetItemsProcessed(state.iterations() * 100);
}
BENCHMARK(BM_Arena_Reset);

static void BM_Arena_Scope(benchmark::State &state) {
    Cell::Context ctx;
    Cell::Arena arena(ctx);

    for (auto _ : state) {
        {
            Cell::ArenaScope scope(arena);
            for (int i = 0; i < 50; ++i) {
                void *ptr = arena.alloc(64);
                benchmark::DoNotOptimize(ptr);
            }
        } // Scope resets here

        {
            Cell::ArenaScope scope(arena);
            for (int i = 0; i < 50; ++i) {
                void *ptr = arena.alloc(128);
                benchmark::DoNotOptimize(ptr);
            }
        }
    }
    state.SetItemsProcessed(state.iterations() * 100);
}
BENCHMARK(BM_Arena_Scope);

// =============================================================================
// Pool<T> Benchmarks
// =============================================================================

struct TestObject {
    int x, y, z;
    double value;
    char padding[32];

    TestObject() : x(0), y(0), z(0), value(0.0) {}
    TestObject(int x_, int y_, int z_, double v) : x(x_), y(y_), z(z_), value(v) {}
};

static void BM_Pool_Alloc_Free(benchmark::State &state) {
    Cell::Context ctx;
    Cell::Pool<TestObject> pool(ctx);

    for (auto _ : state) {
        TestObject *obj = pool.alloc();
        benchmark::DoNotOptimize(obj);
        pool.free(obj);
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Pool_Alloc_Free);

static void BM_Pool_Create_Destroy(benchmark::State &state) {
    Cell::Context ctx;
    Cell::Pool<TestObject> pool(ctx);

    for (auto _ : state) {
        TestObject *obj = pool.create(1, 2, 3, 4.5);
        benchmark::DoNotOptimize(obj);
        pool.destroy(obj);
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Pool_Create_Destroy);

static void BM_Pool_Batch(benchmark::State &state) {
    Cell::Context ctx;
    Cell::Pool<TestObject> pool(ctx);
    const size_t batch_size = 100;
    std::vector<TestObject *> ptrs(batch_size);

    for (auto _ : state) {
        size_t allocated = pool.alloc_batch(ptrs.data(), batch_size);
        benchmark::DoNotOptimize(ptrs.data());
        pool.free_batch(ptrs.data(), allocated);
    }
    state.SetItemsProcessed(state.iterations() * batch_size);
}
BENCHMARK(BM_Pool_Batch);

// =============================================================================
// Comparison: new/delete
// =============================================================================

static void BM_NewDelete_TestObject(benchmark::State &state) {
    for (auto _ : state) {
        TestObject *obj = new TestObject(1, 2, 3, 4.5);
        benchmark::DoNotOptimize(obj);
        delete obj;
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_NewDelete_TestObject);

// =============================================================================
// Arena vs malloc for temporary allocations
// =============================================================================

static void BM_Malloc_Temporary_Pattern(benchmark::State &state) {
    const size_t count = 100;
    std::vector<void *> ptrs(count);

    for (auto _ : state) {
        for (size_t i = 0; i < count; ++i) {
            ptrs[i] = std::malloc(64);
        }
        benchmark::DoNotOptimize(ptrs.data());
        for (size_t i = 0; i < count; ++i) {
            std::free(ptrs[i]);
        }
    }
    state.SetItemsProcessed(state.iterations() * count);
}
BENCHMARK(BM_Malloc_Temporary_Pattern);

static void BM_Arena_Temporary_Pattern(benchmark::State &state) {
    Cell::Context ctx;

    for (auto _ : state) {
        Cell::Arena arena(ctx);
        for (int i = 0; i < 100; ++i) {
            void *ptr = arena.alloc(64);
            benchmark::DoNotOptimize(ptr);
        }
        // Bulk free on arena destruction
    }
    state.SetItemsProcessed(state.iterations() * 100);
}
BENCHMARK(BM_Arena_Temporary_Pattern);
