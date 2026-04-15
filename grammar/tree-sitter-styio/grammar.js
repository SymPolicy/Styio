module.exports = grammar({
  name: "styio",

  extras: ($) => [/\s/, $.line_comment, $.block_comment],
  word: ($) => $.identifier,
  conflicts: ($) => [
    [$.param, $._expr],
  ],

  rules: {
    source_file: ($) => repeat($._statement),

    _statement: ($) =>
      choice(
        $.function_decl,
        $.binding_stmt,
        $.yield_stmt,
        $.expr_stmt,
        $.block
      ),

    function_decl: ($) =>
      choice(
        seq(
          "#",
          field("name", $.identifier),
          optional(seq(":", $.type_expr)),
          choice(":=", "="),
          optional($.param_list),
          optional("=>"),
          choice($.block, $._expr)
        ),
        seq(
          field("name", $.identifier),
          $.param_list,
          optional(seq(":", $.type_expr)),
          choice(":=", "="),
          optional("=>"),
          choice($.block, $._expr)
        )
      ),

    binding_stmt: ($) =>
      seq(
        field("name", $.identifier),
        optional(seq(":", $.type_expr)),
        choice(":=", "=", "<-"),
        $._expr
      ),

    yield_stmt: ($) => seq("<|", $._expr),

    expr_stmt: ($) => $._expr,

    block: ($) => seq("{", repeat($._statement), "}"),

    param_list: ($) =>
      seq(
        "(",
        optional(seq($.param, repeat(seq(",", $.param)))),
        ")"
      ),

    param: ($) =>
      seq(
        field("name", $.identifier),
        optional(seq(":", $.type_expr))
      ),

    type_expr: ($) => choice($.identifier, seq("[", $.type_expr, "]")),

    _expr: ($) =>
      choice(
        $.binary_expr,
        $.identifier,
        $.number,
        $.string,
        $.call_expr,
        $.attr_expr,
        $.list_expr,
        $.resource_expr,
        $.paren_expr
      ),

    binary_expr: ($) =>
      prec.left(1, seq(
        field("left", $._expr),
        field("operator", choice("+", "-", "*", "/", "%", "==", "!=", ">=", "<=", ">", "<", "&&", "||")),
        field("right", $._expr)
      )),

    call_expr: ($) => prec.left(3, seq(field("callee", $.identifier), $.arg_list)),
    arg_list: ($) => seq("(", optional(seq($._expr, repeat(seq(",", $._expr)))), ")"),
    attr_expr: ($) => prec.left(4, seq($._expr, ".", $.identifier)),
    list_expr: ($) => seq("[", optional(seq($._expr, repeat(seq(",", $._expr)))), "]"),
    resource_expr: ($) =>
      choice(
        prec.right(2, seq("@", $.identifier, $.block)),
        seq("@", $.identifier)
      ),
    paren_expr: ($) => seq("(", $._expr, ")"),

    identifier: () => /[_\p{XID_Start}][_\p{XID_Continue}]*/u,
    number: () => /\d+(\.\d+)?/,
    string: () => /"[^"\n\r]*"/,
    line_comment: () => token(seq("//", /.*/)),
    block_comment: () => token(seq("/*", /[^*]*\*+([^/*][^*]*\*+)*/, "/")),
  },
});
