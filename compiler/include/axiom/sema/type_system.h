// core/include/axiom/type_system.h
//
// 【役割】
//   Nexa 言語の組み込み型（プリミティブ型）を管理する TypeRegistry クラスを定義する。
//
//   コンパイラが扱う型はすべて StringID で表現されるが、
//   TypeRegistry はその「どの ID が組み込み型か」「int32 の ID は何か」といった
//   知識を一元管理する。
//
// 【設計メモ】
//   TypeRegistry は StringInterner への参照を持ち、コンストラクタで
//   組み込み型名をインターンすることで各型の StringID を確定させる。
//   以降は is_builtin() や get_int32() 等のゲッターで O(1) アクセスできる。
//
//   将来ユーザー定義型（struct, enum 等）が増えた場合は、
//   TypeRegistry にユーザー定義型の登録メソッドを追加していく想定。

#pragma once

#include <array>
#include "axiom/base/compiler.h" // StringInterner と StringID が定義されているヘッダー

namespace axiom {

/// Nexa コンパイラが扱う型情報を管理するレジストリ。
///
/// 【組み込み型一覧】
///   整数系  : int8, int16, int32, int64, uint8, uint16, uint32, uint64
///   浮動小数: float32, float64
///   その他  : bool, string
///
/// 【使い方】
///   TypeRegistry type_registry(interner);
///   type_registry.is_builtin(some_id);  // 既知の型かチェック
///   type_registry.get_bool();           // bool 型の StringID を取得
class TypeRegistry {
public:
    /// コンストラクタで、コンパイラが扱うすべての組み込み型を登録する。
    /// interner を使って各型名を StringID に変換し、メンバにキャッシュしておく。
    explicit TypeRegistry(StringInterner& interner) : interner_(interner) {
        // 整数型
        type_int8_    = interner_.Intern("int8");
        type_int16_   = interner_.Intern("int16");
        type_int32_   = interner_.Intern("int32");
        type_int64_   = interner_.Intern("int64");

        // 符号なし整数型
        type_uint8_   = interner_.Intern("uint8");
        type_uint16_  = interner_.Intern("uint16");
        type_uint32_  = interner_.Intern("uint32");
        type_uint64_  = interner_.Intern("uint64");

        // 浮動小数点型とその他
        type_float32_ = interner_.Intern("float32");
        type_float64_ = interner_.Intern("float64");
        type_bool_    = interner_.Intern("bool");
        type_string_  = interner_.Intern("string");

        type_void_ = interner_.Intern("void");

        // is_builtin() で高速に検索できるよう、IDを配列にまとめておく
        builtin_type_ids_ = {
            type_int8_, type_int16_, type_int32_, type_int64_,
            type_uint8_, type_uint16_, type_uint32_, type_uint64_,
            type_float32_, type_float64_,
            type_bool_, type_string_, type_void_,
        };
    }

    /// 指定された StringID が組み込み型かどうかを判定する。
    /// 線形探索だが組み込み型は12種類固定なので実質 O(1)。
    [[nodiscard]] bool is_builtin(StringID type_id) const noexcept {
        for (auto id : builtin_type_ids_) {
            if (id == type_id) return true;
        }
        return false;
    }

    // 型検査の時に「これはfloat32か？」「これはboolか？」と確認するためのゲッター。
    // 直接 StringID を比較するより意図が明確になる。
    [[nodiscard]] StringID get_float32() const noexcept { return type_float32_; }
    [[nodiscard]] StringID get_bool()    const noexcept { return type_bool_; }
    [[nodiscard]] StringID get_int32()   const noexcept { return type_int32_; }
    [[nodiscard]] StringID get_void() const noexcept { return type_void_; }

private:
    StringInterner& interner_;  // 参照のみ。所有権はなし（コンパイラ側が管理）

    // 各組み込み型の StringID をキャッシュしておくメンバ変数
    StringID type_int8_;
    StringID type_int16_;
    StringID type_int32_;
    StringID type_int64_;

    StringID type_uint8_;
    StringID type_uint16_;
    StringID type_uint32_;
    StringID type_uint64_;

    StringID type_float32_;
    StringID type_float64_;

    StringID type_bool_;
    StringID type_string_;

    StringID type_void_;

    /// is_builtin() 用の組み込み型IDまとめ配列（要素数は型の総数と一致させること）
    std::array<StringID, 13> builtin_type_ids_;
};

} // namespace axiom