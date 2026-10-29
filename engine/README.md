# Vanta Engine Core

Vanta is a high-performance, Data-Oriented game engine built to seamlessly integrate with the Axiom programming language.

## Architecture & Boundaries
* **Data-Oriented Design (DOD)**: Contiguous flat buffers, Struct-of-Arrays (SoA) layout, zero runtime heap allocations in hot paths.
* **Modern Graphics Pipeline**: Vulkan 1.3 + Slang compute-driven architecture.
* **Platform & Core Isolation**: Strict separation of hardware-specific backends and core engine runtime for IP protection and NDA compliance.

## Directory Structure
* `memory/`: Frame arenas, linear allocators, and SoA storage.
* `render/`: Vulkan device contexts, render passes, and Slang shader pipelines.
* `platform/`: OS windowing, input dispatch, and platform-specific backends.
