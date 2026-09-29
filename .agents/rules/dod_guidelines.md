---
trigger: always_on
---

# Data-Oriented Design (DOD) & Performance Guidelines

This engine (Vanta) strictly adheres to **Data-Oriented Design (DOD)** principles to maximize hardware cache locality and runtime throughput.

## Core Rules

1. **Memory Layout & Cache Efficiency**:
   - Prefer flat arrays, contiguous memory buffers, and Struct-of-Arrays (SoA) / Array-of-Structures-of-Arrays (AoSoA) over deeply nested OOP hierarchies.
   - Avoid deep pointer indirection and polymorphic virtual method tables in performance-critical hot paths.
   - Separate frequently accessed hot data from infrequently accessed cold data.

2. **Zero Runtime Heap Allocations in Hot Paths**:
   - Do NOT perform dynamic heap allocations (`new`, `malloc`, unbounded `std::vector::push_back` without reserve) inside per-frame update or render loops.
   - Utilize linear allocators, frame arenas, or object pools for transient/short-lived per-frame data.

3. **Modern C++20 Standards**:
   - Write clean, modern C++20 code (`std::span`, concepts, designated initializers where appropriate).
   - Prefer value semantics and explicit handles (`uint32_t` IDs, generational handles) over raw/smart pointer tracking where applicable.