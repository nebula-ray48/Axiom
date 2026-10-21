#pragma once

#include <string>
#include <vector>

#include "axiom/ir/ir.h"
#include "axiom/base/diagnostic.h"

namespace axiom::ir {

[[nodiscard]] std::vector<Diagnostic> verify_function(const IrFunction& func, const StringInterner* interner = nullptr);

[[nodiscard]] std::vector<Diagnostic> verify_module(const IrModule& module, const StringInterner* interner = nullptr);

} // namespace axiom::ir

