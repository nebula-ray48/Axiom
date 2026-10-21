# Axiom Programming Language

---

## English
## [Japanese Version](README.JP.md)

### What is Axiom?

Axiom is a modern systems programming language designed from the ground up for **Data-Oriented Design (DOD)** and high-performance computing (e.g., game engines, graphics rendering). It challenges the traditional object-oriented paradigms by prioritizing data layouts, memory caches, and flat Data Structures (SoA - Structure of Arrays) automatically.

Currently in the bootstrapping phase (written in C++23 with Tree-sitter), Axiom aims for complete C/C++ compatibility and eventual self-hosting.

### Key Features

* **Data-Oriented by Default**: Write clear and intuitive code while the compiler automatically transforms it into cache-friendly SoA (Structure of Arrays) layouts under the hood.
* **Zero-Friction C/C++ Interoperability**: Designed to integrate seamlessly with existing C and C++ codebases.
* **Flat Memory Model**: Internally relies on ID-based string interning and continuous memory blocks instead of pointers, drastically reducing cache misses.

### Quick Look

In Axiom, a component is defined simply, but compiled into highly optimized parallel arrays.

```axiom
// Components are automatically laid out as Structure of Arrays (SoA)
component Position {
    x: f32,
    y: f32
}

component Velocity {
    x: f32,
    y: f32
}

val hp: i32 = 100;
```

### Getting Started

Nexa is currently built using C++23 and CMake.

**Prerequisites:**

* CMake (3.24 or newer)
* A C++23 compatible compiler (Clang / GCC / MSVC)

**Build from source:**

```bash
git clone https://github.com/yourusername/nexa.git
cd nexa

# Configure and build using CMake Presets
cmake --preset default
cmake --build --preset default
```

### Testing

Axiom uses Google Test and CTest for its Test-Driven Development (TDD) environment.

```bash
# Quick build & test runner script (auto-configures if needed)
./scripts/shell/test.sh

# Run a specific test suite or filter
./scripts/shell/test.sh IrBuilder
./scripts/shell/test.sh C99CodeGen

# Or run via CTest / test binary directly
ctest --preset default
./build/tests/axiom-test
```

--- 

This project is licensed under the [BSD-2-Clause-Patent](LICENSE).
