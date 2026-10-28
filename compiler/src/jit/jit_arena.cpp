/*
 * Copyright (c) 2026 Nebula-Ray42
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * https://mozilla.org/MPL/2.0/.
 */

#include "axiom/jit/jit_arena.h"

#include <cassert>
#include <cstddef>
#include <cstring>
#include <span>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(__APPLE__)
#include <pthread.h>

#include <libkern/OSCacheControl.h>  // sys_icache_invalidate
#include <sys/mman.h>
#else  // Linux
#include <sys/mman.h>
#endif

namespace axiom::jit {

namespace {

void* os_allocate_jit_memory(size_t size) {
#if defined(_WIN32)
    return VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#elif defined(__APPLE__)
    void* ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS | MAP_JIT, -1, 0);
    return (ptr == MAP_FAILED) ? nullptr : ptr;
#else
    void* ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return (ptr == MAP_FAILED) ? nullptr : ptr;
#endif
}
void os_free_jit_memory(void* ptr, size_t size) {
#if defined(_WIN32)
    (void)size;
    VirtualFree(ptr, 0, MEM_RELEASE);
#else
    munmap(ptr, size);
#endif
}
void os_enable_write(void* base_ptr, size_t capacity) {
#if defined(__APPLE__)
    (void)base_ptr;
    (void)capacity;
    pthread_jit_write_protect_np(0);  // 書込み許可
#elif defined(_WIN32)
    DWORD old_protect;
    VirtualProtect(base_ptr, capacity, PAGE_READWRITE, &old_protect);
#else
    mprotect(base_ptr, capacity, PROT_READ | PROT_WRITE);
#endif
}
void os_enable_execute(void* base_ptr, size_t capacity, void* dest, size_t byte_size) {
#if defined(__APPLE__)
    (void)base_ptr;
    (void)capacity;
    pthread_jit_write_protect_np(1);         // 実行許可
    sys_icache_invalidate(dest, byte_size);  // 命令キャッシュフラッシュ
#elif defined(_WIN32)
    DWORD old_protect;
    VirtualProtect(base_ptr, capacity, PAGE_EXECUTE_READ, &old_protect);
    FlushInstructionCache(GetCurrentProcess(), dest, byte_size);
#else
    mprotect(base_ptr, capacity, PROT_READ | PROT_EXEC);
    __builtin___clear_cache(static_cast<char*>(dest), static_cast<char*>(dest) + byte_size);
#endif
}
}  // namespace

JitCodeArena::JitCodeArena(size_t capacity_bytes) : capacity_(capacity_bytes), used_bytes_(0) {
    base_ptr_ = static_cast<uint8_t*>(os_allocate_jit_memory(capacity_));
    assert(base_ptr_ != nullptr);
}

JitCodeArena::~JitCodeArena() {
    if (base_ptr_) {
        os_free_jit_memory(base_ptr_, capacity_);
    }
}

size_t JitCodeArena::emit_function(std::span<const uint32_t> instructions) {
    size_t aligned_offset = (used_bytes_ + 15) & ~static_cast<size_t>(15);
    size_t byte_size      = instructions.size_bytes();
    assert(byte_size > 0);
    assert(aligned_offset + byte_size <= capacity_);

    uint8_t* dest = base_ptr_ + aligned_offset;

    os_enable_write(base_ptr_, capacity_);
    std::memcpy(dest, instructions.data(), byte_size);
    os_enable_execute(base_ptr_, capacity_, dest, byte_size);

    used_bytes_ = aligned_offset + byte_size;
    return aligned_offset;
}

}  // namespace axiom::jit
