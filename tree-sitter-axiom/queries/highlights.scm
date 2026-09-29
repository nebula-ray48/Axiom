; =======================================
; Nexa Syntax Highlighting Queries
; =======================================

; キーワード
[
  "pub"
  "fun"
  "component"
  "entity"
  "error"
  "forEach"
  "where"
  "if"
  "try"
  "return"
  "val"
  "var"
  "and"
  "or"
  "not"
] @keyword

; 型と組み込み型
(primitive_type) @type.builtin
(type_identifier) @type

; 識別子（デフォルトは変数扱い）
(identifier) @variable

; 数値
(number) @number

; 関数名（関数の定義部分の識別子を上書きして関数色にする）
(function_declaration name: (identifier) @function)

; プロパティアクセス（Position.x の x などをプロパティ色にする）
(member_expression property: (identifier) @property)

; コンポーネントやエンティティのフィールド定義
(field_definition name: (identifier) @property)
