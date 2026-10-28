/*
 * Copyright (c) 2026 Nebula-Ray42
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <string>
#include <vector>

#include "axiom/ir/ir.h"
#include "axiom/base/diagnostic.h"

namespace axiom::ir {

[[nodiscard]] std::vector<Diagnostic> verify_function(const IrFunction& func, const StringInterner* interner = nullptr);

[[nodiscard]] std::vector<Diagnostic> verify_module(const IrModule& module, const StringInterner* interner = nullptr);

} // namespace axiom::ir

