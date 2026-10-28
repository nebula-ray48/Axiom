#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <span>

namespace axiom::jit {

class JitCodeArena {
public:
    explicit JitCodeArena(size_t capacity_bytes = 65536);
    ~JitCodeArena();

    JitCodeArena(const JitCodeArena&)            = delete;
    JitCodeArena& operator=(const JitCodeArena&) = delete;

    size_t emit_function(std::span<const uint32_t> instructions);

    template <typename FnSig>
    FnSig* get_function(size_t offset = 0) const noexcept {
        assert(base_ptr_ != nullptr && offset < used_bytes_);
        return reinterpret_cast<FnSig*>(base_ptr_ + offset);
    }

    void   reset() noexcept { used_bytes_ = 0; }
    size_t used_bytes() const noexcept { return used_bytes_; }
    size_t capacity() const noexcept { return capacity_; }

private:
    uint8_t* base_ptr_   = nullptr;
    size_t   capacity_   = 0;
    size_t   used_bytes_ = 0;
};

}  // namespace axiom::jit
