<p align="center">
  <h1 align="center">Cell</h1>
  <p align="center">
    <strong>High-performance, cache-friendly C++ memory library</strong>
  </p>
  <p align="center">
    <a href="#features">Features</a> •
    <a href="#installation">Installation</a> •
    <a href="#quick-start">Quick Start</a> •
    <a href="#api-reference">API Reference</a> •
    <a href="#configuration">Configuration</a> •
    <a href="#contributing">Contributing</a>
  </p>
</p>

---

**Cell** is a multi-tier memory allocator designed for data-oriented applications that demand predictable performance and minimal fragmentation. Built with modern C++17, it provides a layered allocation strategy that automatically routes allocations to the optimal tier based on size.

## Features

### 🚀 Multi-Tier Allocation Strategy

Cell intelligently routes allocations through four tiers, each optimized for different size ranges:

| Tier | Size Range | Strategy | Use Case |
|------|------------|----------|----------|
| **Layer 1** | 16KB (Cell) | TLS Cache → Global Pool → OS | Fixed-size blocks, internal management |
| **Layer 2** | 16B – 8KB | Segregated size classes (10 bins) | General-purpose allocations |
| **Layer 3** | 32KB – 2MB | Binary buddy allocator | Medium-large allocations |
| **Layer 4** | > 2MB | Direct OS (huge page support) | Large buffers, textures |

### ⚡ Performance Optimizations

- **Lock-free TLS caches** for cells and hot sub-cell sizes (16B–128B)
- **Batch refill** from global pools to amortize synchronization costs
- **Memory decommit API** for releasing physical memory during idle periods
- **Aligned allocation** support for SIMD and cache-line requirements

### 🧰 High-Level Abstractions

| Abstraction | Description |
|-------------|-------------|
| `Arena` | Linear/bump allocator with O(1) allocations and bulk reset |
| `Pool<T>` | Typed object pool with optional construction/destruction |
| `ArenaScope` | RAII guard for automatic arena marker restoration |
| `StlAllocator<T>` | STL-compatible allocator for standard containers |

### 🔧 Debug & Diagnostic Features

Enable at compile-time for zero overhead when disabled:

| Feature | Flag | Description |
|---------|------|-------------|
| **Guard Bytes** | `CELL_DEBUG_GUARDS` | Detects buffer overflows/underflows |
| **Stack Traces** | `CELL_DEBUG_STACKTRACE` | Captures allocation call stacks |
| **Leak Detection** | `CELL_DEBUG_LEAKS` | Reports unfreed allocations at shutdown |
| **Memory Statistics** | `CELL_ENABLE_STATS` | Tracks allocation counts, sizes, and peaks |
| **Budget Limits** | `CELL_ENABLE_BUDGET` | Enforces per-context memory caps |
| **Instrumentation** | `CELL_ENABLE_INSTRUMENTATION` | Allocation/deallocation callbacks |

---

## Requirements

- **C++17** or later
- **CMake 3.16+**
- Supported platforms: Linux, Windows, macOS

---

## Installation

### Building from Source

```bash
git clone https://github.com/your-username/cell.git
cd cell
mkdir build && cd build
cmake ..
cmake --build .
```

### CMake Integration

Add Cell as a subdirectory in your project:

```cmake
add_subdirectory(external/cell)
target_link_libraries(your_target PRIVATE cell)
```

Or use FetchContent:

```cmake
include(FetchContent)
FetchContent_Declare(
    cell
    GIT_REPOSITORY https://github.com/your-username/cell.git
    GIT_TAG main
)
FetchContent_MakeAvailable(cell)
target_link_libraries(your_target PRIVATE cell)
```

---

## Quick Start

### Basic Allocation

```cpp
#include <cell/context.h>

int main() {
    Cell::Context ctx;

    // Typed allocation (automatically routed by size)
    int* value = ctx.alloc<int>();
    *value = 42;
    ctx.free_bytes(value);

    // Array allocation
    float* buffer = ctx.alloc_array<float>(1024);
    ctx.free_bytes(buffer);

    // Raw bytes with custom alignment
    void* aligned = ctx.alloc_aligned(4096, 64);  // 64-byte aligned
    ctx.free_bytes(aligned);
}
```

### Arena Allocator (Bulk Deallocation)

```cpp
#include <cell/arena.h>

void process_frame(Cell::Context& ctx) {
    Cell::Arena arena(ctx);

    // Fast O(1) allocations
    auto* temp1 = arena.alloc<Matrix4x4>();
    auto* temp2 = arena.alloc_array<Vertex>(1000);

    // ... use temporary data ...

    arena.reset();  // Bulk free - all pointers invalidated
}
```

### Scoped Arena (RAII)

```cpp
#include <cell/pool.h>

void nested_processing(Cell::Arena& arena) {
    Cell::ArenaScope scope(arena);  // Save position

    auto* local = arena.alloc<TempData>();
    // ... use local ...

}  // Automatically restored to saved position
```

### Object Pool

```cpp
#include <cell/pool.h>

void game_loop(Cell::Context& ctx) {
    Cell::Pool<Entity> entity_pool(ctx);

    // Allocate without construction
    Entity* raw = entity_pool.alloc();
    new (raw) Entity(args...);

    // Or allocate with construction
    Entity* entity = entity_pool.create(x, y, z);
    
    // Cleanup
    entity_pool.destroy(entity);
    entity_pool.free(raw);
}
```

### STL Container Integration

```cpp
#include <cell/stl_allocator.h>
#include <vector>
#include <map>

void use_stl_containers(Cell::Context& ctx) {
    // Vector with Cell memory
    std::vector<int, Cell::StlAllocator<int>> vec(Cell::StlAllocator<int>(ctx));
    vec.push_back(1);
    vec.push_back(2);

    // Map with Cell memory
    using Alloc = Cell::StlAllocator<std::pair<const std::string, int>>;
    std::map<std::string, int, std::less<>, Alloc> map(Alloc(ctx));
    map["key"] = 42;
}
```

### Memory Management for Long-Running Applications

```cpp
#include <cell/context.h>

void on_loading_screen(Cell::Context& ctx) {
    // Release physical memory while keeping virtual address space
    size_t released = ctx.decommit_unused();
    printf("Released %zu bytes to OS\n", released);
}

void on_thread_exit(Cell::Context& ctx) {
    // Flush thread-local caches before thread terminates
    ctx.flush_tls_bin_caches();
}
```

---

## API Reference

### Context

The central memory environment. All allocations flow through a `Context`.

```cpp
class Context {
public:
    explicit Context(const Config& config = Config{});
    ~Context();

    // Primary allocation API (auto-routed by size)
    void* alloc_bytes(size_t size, uint8_t tag = 0, size_t alignment = 8);
    void  free_bytes(void* ptr);
    void* realloc_bytes(void* ptr, size_t new_size, uint8_t tag = 0);

    // Typed allocation
    template<typename T> T* alloc(uint8_t tag = 0);
    template<typename T> T* alloc_array(size_t count, uint8_t tag = 0);

    // Large allocation API (explicit tier selection)
    void* alloc_large(size_t size, uint8_t tag = 0, bool try_huge_pages = true);
    void  free_large(void* ptr);
    void* alloc_aligned(size_t size, size_t alignment, uint8_t tag = 0);

    // Cell-level API (16KB blocks)
    CellData* alloc_cell(uint8_t tag = 0);
    void      free_cell(CellData* cell);

    // Memory management
    size_t decommit_unused();
    size_t committed_bytes() const;
    void   flush_tls_bin_caches();
};
```

### Arena

Linear allocator for temporary allocations with bulk deallocation.

```cpp
class Arena {
public:
    explicit Arena(Context& ctx, uint8_t tag = 0);
    ~Arena();

    void* alloc(size_t size, size_t alignment = 8);
    template<typename T> T* alloc();
    template<typename T> T* alloc_array(size_t count);

    void reset();    // Free all, keep cells
    void release();  // Free all, return cells to context

    Marker save() const;
    void reset_to_marker(Marker marker);

    size_t bytes_allocated() const;
    size_t bytes_remaining() const;
    size_t cell_count() const;
};
```

### Pool\<T\>

Typed object pool with optional construction support.

```cpp
template<typename T>
class Pool {
public:
    explicit Pool(Context& ctx, uint8_t tag = 0);

    T* alloc();                              // Allocate without construction
    T* alloc_array(size_t count);
    void free(T* ptr);

    template<typename... Args>
    T* create(Args&&... args);               // Allocate + construct
    void destroy(T* ptr);                    // Destruct + free

    size_t alloc_batch(T** out, size_t count);
    void free_batch(T** ptrs, size_t count);
};
```

### StlAllocator\<T\>

STL-compatible allocator for use with standard containers.

```cpp
template<typename T>
class StlAllocator {
public:
    explicit StlAllocator(Context& ctx, uint8_t tag = 0) noexcept;

    T* allocate(size_type n);
    void deallocate(T* p, size_type n) noexcept;
};
```

---

## Configuration

### CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `CELL_BUILD_TESTS` | `ON` | Build unit tests |
| `CELL_ENABLE_STATS` | `OFF` | Enable memory statistics |
| `CELL_DEBUG_GUARDS` | `OFF` | Enable guard bytes for bounds checking |
| `CELL_DEBUG_STACKTRACE` | `OFF` | Enable stack trace capture |
| `CELL_DEBUG_LEAKS` | `OFF` | Enable leak detection |
| `CELL_ENABLE_BUDGET` | `OFF` | Enable memory budget limits |
| `CELL_ENABLE_INSTRUMENTATION` | `OFF` | Enable allocation callbacks |

### Example: Debug Build

```bash
cmake -DCELL_DEBUG_GUARDS=ON \
      -DCELL_DEBUG_LEAKS=ON \
      -DCELL_ENABLE_STATS=ON \
      ..
```

### Using Debug Features

```cpp
// With CELL_ENABLE_STATS
ctx.dump_stats();           // Print statistics to stdout
ctx.reset_stats();          // Reset counters
const auto& stats = ctx.get_stats();

// With CELL_DEBUG_LEAKS
ctx.report_leaks();         // Print unfreed allocations
size_t leaks = ctx.live_allocation_count();

// With CELL_DEBUG_GUARDS
bool ok = ctx.check_guards(ptr);  // Validate bounds

// With CELL_ENABLE_BUDGET
ctx.set_budget(1024 * 1024 * 100);  // 100 MB limit
ctx.set_budget_callback([](size_t req, size_t budget, size_t current) {
    fprintf(stderr, "Budget exceeded!\n");
});

// With CELL_ENABLE_INSTRUMENTATION
ctx.set_alloc_callback([](void* ptr, size_t size, uint8_t tag, bool is_alloc) {
    printf("%s %zu bytes at %p\n", is_alloc ? "ALLOC" : "FREE", size, ptr);
});
```

---

## Running Tests

```bash
cd build
ctest --output-on-failure
```

Or run individual test executables:

```bash
./test_allocator
./test_sub_cell
./test_arena
./test_pool
./test_buddy
./test_stats
./test_debug
./test_large
./test_budget
./test_instrumentation
```

---

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                         Context                             │
│  ┌─────────────────────────────────────────────────────┐   │
│  │                  alloc_bytes(size)                   │   │
│  └─────────────────────────────────────────────────────┘   │
│                            │                                │
│              ┌─────────────┼─────────────┐                 │
│              ▼             ▼             ▼                 │
│   ┌──────────────┐ ┌──────────────┐ ┌──────────────┐      │
│   │  Sub-Cell    │ │    Buddy     │ │    Large     │      │
│   │  16B - 8KB   │ │  32KB - 2MB  │ │    > 2MB     │      │
│   │  (10 bins)   │ │  (power-of-2)│ │  (OS direct) │      │
│   └──────────────┘ └──────────────┘ └──────────────┘      │
│          │                                                  │
│          ▼                                                  │
│   ┌──────────────┐                                         │
│   │ TLS Cache    │  ← Lock-free, bins 0-3 (16B-128B)       │
│   └──────────────┘                                         │
│          │                                                  │
│          ▼                                                  │
│   ┌──────────────┐                                         │
│   │ Global Pool  │  ← Per-bin mutex                        │
│   └──────────────┘                                         │
│          │                                                  │
│          ▼                                                  │
│   ┌──────────────┐                                         │
│   │  Cell Pool   │  ← 16KB cells, lock-free stack          │
│   └──────────────┘                                         │
│          │                                                  │
│          ▼                                                  │
│   ┌──────────────┐                                         │
│   │   OS VMM     │  ← Superblock allocation                │
│   └──────────────┘                                         │
└─────────────────────────────────────────────────────────────┘
```

---

## Thread Safety

- **Context**: Thread-safe for all allocation APIs (uses per-bin locking)
- **Arena**: **NOT** thread-safe (use one per thread)
- **Pool\<T\>**: Thread-safe (same as Context)
- **StlAllocator**: Thread-safe (delegates to Context)

**Important**: Call `flush_tls_bin_caches()` before thread exit to prevent memory leaks in thread-local caches.

---

## Performance Tips

1. **Use Arenas for temporary allocations** — Avoid per-allocation overhead
2. **Prefer `Pool<T>` for fixed-type objects** — Enables batch operations
3. **Call `decommit_unused()` during idle** — Release physical RAM to OS
4. **Use memory tags** — Enable per-category statistics and debugging
5. **Pre-warm with a few allocations** — TLS caches and global pools will be prepared

---

## License

This project is licensed under the MIT License — see the [LICENSE](LICENSE) file for details.

---

## Contributing

Contributions are welcome! Please read the [STYLE_GUIDE.md](STYLE_GUIDE.md) before submitting pull requests.

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

---

## Roadmap

See [ROADMAP.md](ROADMAP.md) for planned features and version milestones.
