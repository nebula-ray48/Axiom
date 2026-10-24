#pragma once

#include <cstdint>
#include <span>

#include "axiom/ir/ir.h"

namespace axiom::ir {

class IrVm {
public:
    enum class TrapCode : uint8_t {
        None,
        IntegerDivideByZero,
        IntegerOverflow,
        InvalidInstruction,
    };

    struct VmResult {
        int64_t  value = 0;
        TrapCode trap  = TrapCode::None;

        [[nodiscard]] bool ok() const noexcept { return trap == TrapCode::None; }
    };

    VmResult execute(const IrFunction& func, std::span<const int64_t> args = {});

private:
};

}  // namespace axiom::ir
