#pragma once

#include <sstream>
#include "axiom/ir/ir.h"
#include "axiom/base/compiler.h"

namespace axiom {

class C99CodeGen {
public:
    explicit C99CodeGen(const StringInterner& interner);

    [[nodiscard]] std::string generate(const ir::IrModule& module);

private:
    const StringInterner& interner_;
    // 型名変換: "int32" ➔ "int32_t"
    [[nodiscard]] std::string map_type(StringID type_id) const;
    // オペランド変換: %0 ➔ _t0, 10 ➔ 10, res ➔ res
    [[nodiscard]] std::string format_operand(const ir::IrOperand& op) const;
    // 1つの関数、1つの命令を出力するヘルパー
    void generate_function(const ir::IrFunction& func, std::string& out);
    void generate_instruction(const ir::IrInstruction& inst, std::string& out);

};

}  // namespace axiom