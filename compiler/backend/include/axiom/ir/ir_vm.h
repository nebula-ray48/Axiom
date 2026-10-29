/*
 * Copyright (c) 2026 Nebula-Ray42
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * https://mozilla.org/MPL/2.0/.
 */

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
