module.exports = grammar({
  name: 'nexa',

  precedences: $ => [
    [
      'assignment',
      'or',
      'and',
      'compare',
      'add',
      'multiply',
      'not',
      'try',
      'member'
    ]
  ],

  rules: {
    source_file: $ => repeat($._declaration),

    _declaration: $ => choice(
      $.component_declaration,
      $.entity_declaration,
      $.error_declaration,
      $.function_declaration,
      $.variable_declaration
    ),

    // --- データ構造 ---
    component_declaration: $ => seq('component', field('name', $.type_identifier), '{', repeat($.field_definition), '}'),
    entity_declaration: $ => seq('entity', field('name', $.type_identifier), '{', repeat(choice(seq($.type_identifier, ','), $.field_definition)), '}'),
    error_declaration: $ => seq('error', field('name', $.type_identifier), '{', repeat(seq($.type_identifier, optional(','))), '}'),
    field_definition: $ => seq(field('name', $.identifier), ':', field('type', $._type), optional(',')),

    // --- 関数とロジック ---
    function_declaration: $ => seq(
      optional('pub'), 'fun', field('name', $.identifier),
        '(', field('parameters', optional($.parameter_list)), ')',
      optional(seq('->', field('return_type', $._type_expression))),
      field('body', $.block)
    ),

    parameter_list: $ => seq($.parameter, repeat(seq(',', $.parameter))),
    parameter: $ => seq(field('name', $.identifier), ':', field('type', $._type)),

    _type_expression: $ => seq($._type, optional(seq('or', $.type_identifier))),

    variable_declaration: $ => seq(
      choice('val', 'var'), field('name', $.identifier),
      optional(seq(':', field('type', $._type))),
      '=', field('value', $._expression),
      ';'
    ),

    // --- ステートメントと式 ---
    block: $ => seq('{', repeat(choice($._declaration, $._statement)), '}'),

    _statement: $ => choice(
      $.forEach_statement,
        $.if_statement,
        $.while_statement,
        $.return_statement,
        $.expression_statement
    ),

    forEach_statement: $ => seq(
      'forEach', field('target', $.type_identifier),
      optional(seq('where', field('condition', $._expression))),
      field('body', $.block)
    ),

    if_statement: $ => seq('if', field('condition', $._expression), field('consequence', $.block)),

      while_statement: $ => seq(
          'while', field('condition', $._expression),
          field('body', $.block)
      ),

    return_statement: $ => seq('return', optional($._expression), ';'),

    expression_statement: $ => seq($._expression, ';'),

    _expression: $ => choice(
      $.assignment_expression,
      $.binary_expression,
      $.unary_expression,
      $.try_expression,
      $.identifier,
      $.number,
      $.member_expression
    ),

    assignment_expression: $ => prec.right('assignment', seq(
      field('left', choice($.identifier, $.member_expression)),
      '=',
      field('right', $._expression)
    )),

    member_expression: $ => prec('member', seq(
      field('object', choice($.identifier, $.type_identifier)),
      '.',
      field('property', $.identifier)
    )),

    try_expression: $ => prec('try', seq('try', $._expression)),

    binary_expression: $ => choice(
      prec.left('or', seq($._expression, 'or', $._expression)),
      prec.left('and', seq($._expression, 'and', $._expression)),
      prec.left('compare', seq($._expression, choice('<', '<=', '>', '>=', '==', '!='), $._expression)),
      prec.left('add', seq($._expression, choice('+', '-'), $._expression)),
      prec.left('multiply', seq($._expression, choice('*', '/'), $._expression))
    ),

    unary_expression: $ => prec('not', seq('not', $._expression)),

    // --- 組み込み型と識別子の厳格化 ---
      primitive_type: $ => choice(
          'int8', 'int16', 'int32', 'int64',
          'uint8', 'uint16', 'uint32', 'uint64',
          'float32', 'float64',
          'bool',
          'string'
      ),

    _type: $ => choice($.primitive_type, $.type_identifier),

    identifier: $ => /[a-z_][a-zA-Z0-9_]*/,
    type_identifier: $ => /[A-Z][a-zA-Z0-9_]*/,
    number: $ => /\d+(\.\d+)?/
  }
});
