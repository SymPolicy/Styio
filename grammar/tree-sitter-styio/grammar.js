module.exports = grammar({
  name: "styio",

  extras: ($) => [/\s/, $.line_comment, $.block_comment],
  word: ($) => $.identifier,
  conflicts: ($) => [
    [$.param, $._expr],
    [$._expr, $._range_bound_expr],
  ],

  rules: {
    source_file: ($) => repeat($._statement),

    _statement: ($) =>
      choice(
        $.function_decl,
        $.binding_stmt,
        $.resource_decl_stmt,
        $.await_stmt,
        $.iterator_stmt,
        $.sink_stmt,
        $.resource_order_stmt,
        $.print_stmt,
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

    // These edit-time forms mirror the nightly parser's resource/task slice.
    resource_decl_stmt: ($) => seq(
      "@", field("name", $.identifier), ":", $.type_expr,
      optional(seq("|", optional(".."), $.integer, "|"))
    ),
    await_stmt: ($) => seq(
      "?|", field("source", $._expr), "->",
      field("name", $.identifier), ":", $.type_expr,
      optional(seq("|", field("fallback", $._expr)))
    ),
    iterator_stmt: ($) => seq(
      field("source", $._expr), ">>", "#", $.param_list, "=>", $.block
    ),
    sink_stmt: ($) => seq(field("value", $._expr), "->", $.resource_expr),
    resource_order_stmt: ($) => seq($.identifier, "=>", $._expr),
    print_stmt: ($) => seq(">_", $.arg_list),

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
        $.range_expr,
        $.resource_expr,
        $.task_expr,
        $.paren_expr
      ),

    binary_expr: ($) =>
      prec.left(2, seq(
        field("left", $._expr),
        field("operator", choice("+", "-", "*", "/", "%", "==", "!=", ">=", "<=", ">", "<", "&&", "||")),
        field("right", $._expr)
      )),

    call_expr: ($) => prec.left(3, seq(field("callee", $.identifier), $.arg_list)),
    arg_list: ($) => seq("(", optional(seq($._expr, repeat(seq(",", $._expr)))), ")"),
    attr_expr: ($) => prec.left(4, seq($._expr, ".", $.identifier)),
    list_expr: ($) =>
      choice(
        $.materialized_range,
        $.list_literal
      ),
    list_literal: ($) => prec(1, seq("[", optional(seq($._expr, repeat(seq(",", $._expr)))), "]")),
    materialized_range: ($) => prec(2, seq("[", field("range", $.range_expr), "]")),
    range_expr: ($) => prec.left(1, seq(
      field("start", $._range_bound_expr),
      "..",
      field("end", $._range_bound_expr)
    )),
    _range_bound_expr: ($) =>
      choice(
        $.binary_expr,
        $.identifier,
        $.number,
        $.call_expr,
        $.attr_expr,
        $.paren_expr
      ),
    resource_expr: ($) =>
      choice(
        prec.right(2, seq("@", $.identifier, $.block)),
        prec.right(4, seq("@", $.identifier, optional($.history_selector)))
      ),
    history_selector: ($) => seq("[", "-", $.integer, "]"),
    task_expr: ($) => seq("||>", $.block),
    paren_expr: ($) => seq("(", $._expr, ")"),

    identifier: () => /[_\p{XID_Start}][_\p{XID_Continue}]*/u,
    number: () => /\d+(\.\d+)?/,
    integer: () => /\d+/,
    string: () => /"[^"\n\r]*"/,
    line_comment: () => token(seq("//", /.*/)),
    block_comment: () => token(seq("/*", /[^*]*\*+([^/*][^*]*\*+)*/, "/")),
  },
});
