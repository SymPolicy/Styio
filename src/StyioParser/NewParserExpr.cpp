#include "NewParserExpr.hpp"

#include <memory>

#include "ParserLookahead.hpp"

namespace {

int
expr_prec_of(StyioTokenType type) {
  switch (type) {
    case StyioTokenType::LOGIC_OR:
      return 20;
    case StyioTokenType::LOGIC_AND:
      return 30;
    case StyioTokenType::BINOP_EQ:
    case StyioTokenType::BINOP_NE:
      return 35;
    case StyioTokenType::BINOP_GT:
    case StyioTokenType::BINOP_GE:
    case StyioTokenType::BINOP_LT:
    case StyioTokenType::BINOP_LE:
    case StyioTokenType::TOK_RANGBRAC:
    case StyioTokenType::TOK_LANGBRAC:
      return 40;
    case StyioTokenType::TOK_PLUS:
    case StyioTokenType::TOK_MINUS:
      return 50;
    case StyioTokenType::TOK_STAR:
    case StyioTokenType::TOK_SLASH:
    case StyioTokenType::TOK_PERCENT:
      return 60;
    case StyioTokenType::BINOP_POW:
      return 70;
    default:
      return -1;
  }
}

bool
expr_is_right_assoc(StyioTokenType type) {
  return expr_prec_of(type) >= 0;
}

StyioOpType
expr_map_binop(StyioTokenType type) {
  switch (type) {
    case StyioTokenType::TOK_PLUS:
      return StyioOpType::Binary_Add;
    case StyioTokenType::TOK_MINUS:
      return StyioOpType::Binary_Sub;
    case StyioTokenType::TOK_STAR:
      return StyioOpType::Binary_Mul;
    case StyioTokenType::TOK_SLASH:
      return StyioOpType::Binary_Div;
    case StyioTokenType::TOK_PERCENT:
      return StyioOpType::Binary_Mod;
    case StyioTokenType::BINOP_POW:
      return StyioOpType::Binary_Pow;
    default:
      return StyioOpType::Undefined;
  }
}

bool
expr_is_comp(StyioTokenType type) {
  switch (type) {
    case StyioTokenType::BINOP_GT:
    case StyioTokenType::TOK_RANGBRAC:
    case StyioTokenType::BINOP_GE:
    case StyioTokenType::BINOP_LT:
    case StyioTokenType::TOK_LANGBRAC:
    case StyioTokenType::BINOP_LE:
    case StyioTokenType::BINOP_EQ:
    case StyioTokenType::BINOP_NE:
      return true;
    default:
      return false;
  }
}

CompType
expr_map_comp(StyioTokenType type) {
  switch (type) {
    case StyioTokenType::BINOP_GT:
    case StyioTokenType::TOK_RANGBRAC:
      return CompType::GT;
    case StyioTokenType::BINOP_GE:
      return CompType::GE;
    case StyioTokenType::BINOP_LT:
    case StyioTokenType::TOK_LANGBRAC:
      return CompType::LT;
    case StyioTokenType::BINOP_LE:
      return CompType::LE;
    case StyioTokenType::BINOP_EQ:
      return CompType::EQ;
    case StyioTokenType::BINOP_NE:
      return CompType::NE;
    default:
      return CompType::EQ;
  }
}

bool
expr_is_logic(StyioTokenType type) {
  return type == StyioTokenType::LOGIC_AND || type == StyioTokenType::LOGIC_OR;
}

LogicType
expr_map_logic(StyioTokenType type) {
  switch (type) {
    case StyioTokenType::LOGIC_AND:
      return LogicType::AND;
    case StyioTokenType::LOGIC_OR:
      return LogicType::OR;
    default:
      return LogicType::RAW;
  }
}

class StyioExprSubsetParser
{
private:
  StyioContext& context_;

  std::vector<StyioAST*> parse_call_args() {
    context_.move_forward(1, "new_expr:call(");

    std::vector<StyioAST*> args;
    context_.skip_spaces_no_linebreak();
    if (context_.cur_tok_type() != StyioTokenType::TOK_RPAREN) {
      while (true) {
        args.push_back(parse_expression(0));
        context_.skip_spaces_no_linebreak();
        if (context_.cur_tok_type() == StyioTokenType::TOK_RPAREN) {
          break;
        }
        context_.try_match_panic(StyioTokenType::TOK_COMMA);
        context_.skip_spaces_no_linebreak();
      }
    }
    context_.try_match_panic(StyioTokenType::TOK_RPAREN);
    return args;
  }

  StyioAST* parse_call_after_name(const std::string& callee_name) {
    return FuncCallAST::Create(NameAST::Create(callee_name), parse_call_args());
  }

  StyioAST* parse_dot_call_after_callee(StyioAST* callee) {
    context_.move_forward(1, "new_expr:dot");
    context_.skip_spaces_no_linebreak();
    if (context_.cur_tok_type() != StyioTokenType::NAME) {
      throw StyioSyntaxError("expected method name after '.' in new parser subset");
    }
    const std::string method_name = context_.cur_tok()->original;
    context_.move_forward(1, "new_expr:dot_name");
    context_.skip_spaces_no_linebreak();
    if (context_.cur_tok_type() != StyioTokenType::TOK_LPAREN) {
      throw StyioSyntaxError("expected '(' after method name in new parser subset");
    }
    return FuncCallAST::Create(callee, NameAST::Create(method_name), parse_call_args());
  }

  StyioAST* parse_name_family(const std::string& name) {
    if (name == "true" || name == "false") {
      return BoolAST::Create(name == "true");
    }

    context_.skip_spaces_no_linebreak();
    if (context_.cur_tok_type() == StyioTokenType::TOK_LPAREN) {
      return parse_call_after_name(name);
    }

    if (context_.cur_tok_type() == StyioTokenType::TOK_DOT) {
      return parse_dot_call_after_callee(NameAST::Create(name));
    }

    return NameAST::Create(name);
  }

  StyioAST* parse_primary() {
    context_.skip();

    switch (context_.cur_tok_type()) {
      case StyioTokenType::INTEGER: {
        const std::string lit = context_.cur_tok()->original;
        context_.move_forward(1, "new_expr:int");
        return IntAST::Create(lit);
      }
      case StyioTokenType::DECIMAL: {
        const std::string lit = context_.cur_tok()->original;
        context_.move_forward(1, "new_expr:float");
        return FloatAST::Create(lit);
      }
      case StyioTokenType::STRING: {
        const std::string lit = context_.cur_tok()->original;
        context_.move_forward(1, "new_expr:string");
        return StringAST::Create(lit);
      }
      case StyioTokenType::NAME: {
        const std::string name = context_.cur_tok()->original;
        context_.move_forward(1, "new_expr:name");
        return parse_name_family(name);
      }
      case StyioTokenType::TOK_LPAREN: {
        context_.move_forward(1, "new_expr:(");
        StyioAST* inner = parse_expression(0);
        context_.skip();
        if (!context_.match(StyioTokenType::TOK_RPAREN)) {
          throw StyioSyntaxError("expected ')' in new parser expression subset");
        }
        return inner;
      }
      default:
        throw StyioSyntaxError("unexpected token in new parser expression subset");
    }
  }

  StyioAST* parse_unary() {
    context_.skip();

    if (context_.cur_tok_type() == StyioTokenType::TOK_PLUS) {
      context_.move_forward(1, "new_expr:unary+");
      return parse_unary();
    }
    if (context_.cur_tok_type() == StyioTokenType::TOK_MINUS) {
      context_.move_forward(1, "new_expr:unary-");
      // Align with legacy parser: unary '-' captures the remaining expression.
      return BinOpAST::Create(StyioOpType::Binary_Sub, IntAST::Create("0"), parse_expression(0));
    }

    return parse_primary();
  }

  StyioAST* parse_expression(int min_prec) {
    StyioAST* lhs = parse_unary();

    while (true) {
      context_.skip();
      const StyioTokenType tok = context_.cur_tok_type();
      const int prec = expr_prec_of(tok);
      if (prec < min_prec) {
        break;
      }

      const bool is_comp = expr_is_comp(tok);
      const bool is_logic = expr_is_logic(tok);
      const StyioOpType op = expr_map_binop(tok);
      if (!is_comp && !is_logic && op == StyioOpType::Undefined) {
        break;
      }

      context_.move_forward(1, "new_expr:binop");
      const int next_min = expr_is_right_assoc(tok) ? prec : (prec + 1);
      StyioAST* rhs = parse_expression(next_min);
      if (is_comp) {
        lhs = new BinCompAST(expr_map_comp(tok), lhs, rhs);
      }
      else if (is_logic) {
        lhs = CondAST::Create(expr_map_logic(tok), lhs, rhs);
      }
      else {
        lhs = BinOpAST::Create(op, lhs, rhs);
      }
    }

    return lhs;
  }

public:
  explicit StyioExprSubsetParser(StyioContext& context) :
      context_(context) {
  }

  StyioAST* parse() {
    return parse_expression(0);
  }
};

} // namespace

bool
styio_new_parser_is_expr_subset_token(StyioTokenType type) {
  if (styio_is_trivia_token(type)) {
    return true;
  }

  switch (type) {
    case StyioTokenType::TOK_EOF:
    case StyioTokenType::INTEGER:
    case StyioTokenType::DECIMAL:
    case StyioTokenType::STRING:
    case StyioTokenType::NAME:
    case StyioTokenType::TOK_LPAREN:
    case StyioTokenType::TOK_RPAREN:
    case StyioTokenType::TOK_DOT:
    case StyioTokenType::TOK_PLUS:
    case StyioTokenType::TOK_MINUS:
    case StyioTokenType::TOK_STAR:
    case StyioTokenType::TOK_SLASH:
    case StyioTokenType::TOK_PERCENT:
    case StyioTokenType::BINOP_POW:
    case StyioTokenType::BINOP_GT:
    case StyioTokenType::BINOP_GE:
    case StyioTokenType::BINOP_LT:
    case StyioTokenType::BINOP_LE:
    case StyioTokenType::TOK_RANGBRAC:
    case StyioTokenType::TOK_LANGBRAC:
    case StyioTokenType::BINOP_EQ:
    case StyioTokenType::BINOP_NE:
    case StyioTokenType::LOGIC_AND:
    case StyioTokenType::LOGIC_OR:
      return true;
    default:
      return false;
  }
}

bool
styio_new_parser_is_expr_subset_start(StyioTokenType type) {
  switch (type) {
    case StyioTokenType::INTEGER:
    case StyioTokenType::DECIMAL:
    case StyioTokenType::STRING:
    case StyioTokenType::NAME:
    case StyioTokenType::TOK_LPAREN:
    case StyioTokenType::TOK_PLUS:
    case StyioTokenType::TOK_MINUS:
      return true;
    default:
      return false;
  }
}

StyioAST*
parse_expr_new_subset(StyioContext& context) {
  StyioExprSubsetParser parser(context);
  return parser.parse();
}

bool
styio_new_parser_is_stmt_subset_token(StyioTokenType type) {
  if (styio_new_parser_is_expr_subset_token(type)) {
    return true;
  }
  switch (type) {
    case StyioTokenType::TOK_HASH:
    case StyioTokenType::PRINT:
    case StyioTokenType::TOK_COMMA:
    case StyioTokenType::TOK_EQUAL:
    case StyioTokenType::TOK_COLON:
    case StyioTokenType::WALRUS:
    case StyioTokenType::MATCH:
    case StyioTokenType::ITERATOR:
    case StyioTokenType::ARROW_DOUBLE_RIGHT:
    case StyioTokenType::TOK_LCURBRAC:
    case StyioTokenType::TOK_RCURBRAC:
    case StyioTokenType::TOK_UNDLINE:
    case StyioTokenType::EXTRACTOR:
    case StyioTokenType::BOUNDED_BUFFER_OPEN:
    case StyioTokenType::BOUNDED_BUFFER_CLOSE:
    case StyioTokenType::COMPOUND_ADD:
    case StyioTokenType::COMPOUND_SUB:
    case StyioTokenType::COMPOUND_MUL:
    case StyioTokenType::COMPOUND_DIV:
    case StyioTokenType::COMPOUND_MOD:
      return true;
    default:
      return false;
  }
}

bool
styio_new_parser_is_stmt_subset_start(StyioTokenType type) {
  return type == StyioTokenType::TOK_HASH
         || type == StyioTokenType::PRINT
         || styio_new_parser_is_expr_subset_start(type);
}

namespace {
StyioAST*
parse_stmt_new_subset(StyioContext& context);

PrintAST*
parse_print_new_subset(StyioContext& context) {
  std::vector<StyioAST*> exprs;
  context.match_panic(StyioTokenType::PRINT);
  context.try_match_panic(StyioTokenType::TOK_LPAREN);

  while (true) {
    context.skip();
    if (context.match(StyioTokenType::TOK_RPAREN)) {
      break;
    }
    exprs.push_back(parse_expr_new_subset(context));
    context.skip();
    if (context.match(StyioTokenType::TOK_RPAREN)) {
      break;
    }
    context.try_match_panic(StyioTokenType::TOK_COMMA);
  }

  return PrintAST::Create(exprs);
}

TypeAST*
parse_hash_simple_type_new_subset(StyioContext& context) {
  context.skip();
  if (context.cur_tok_type() == StyioTokenType::BOUNDED_BUFFER_OPEN) {
    context.move_forward(1, "new_stmt:hash_type_bounded_open");
    context.skip();
    if (context.cur_tok_type() != StyioTokenType::INTEGER) {
      throw StyioSyntaxError("expected integer capacity in [|n|] return type");
    }
    const std::string cap = context.cur_tok()->original;
    context.move_forward(1, "new_stmt:hash_type_bounded_cap");
    context.skip();
    context.try_match_panic(StyioTokenType::BOUNDED_BUFFER_CLOSE);
    return TypeAST::CreateBoundedRingBuffer(cap);
  }
  if (context.cur_tok_type() != StyioTokenType::NAME) {
    throw StyioSyntaxError("expected simple return type name in new parser subset");
  }
  const std::string type_name = context.cur_tok()->original;
  context.move_forward(1, "new_stmt:hash_type_name");
  return TypeAST::Create(type_name);
}

std::variant<TypeAST*, TypeTupleAST*>
parse_hash_ret_type_new_subset(StyioContext& context) {
  context.skip();
  if (context.cur_tok_type() == StyioTokenType::TOK_LPAREN) {
    context.move_forward(1, "new_stmt:hash_ret_tuple_open");
    std::vector<TypeAST*> types;
    context.skip();
    if (context.cur_tok_type() != StyioTokenType::TOK_RPAREN) {
      while (true) {
        types.push_back(parse_hash_simple_type_new_subset(context));
        context.skip();
        if (context.cur_tok_type() == StyioTokenType::TOK_RPAREN) {
          break;
        }
        context.try_match_panic(StyioTokenType::TOK_COMMA);
        context.skip();
      }
    }
    context.try_match_panic(StyioTokenType::TOK_RPAREN);
    return TypeTupleAST::Create(types);
  }
  return parse_hash_simple_type_new_subset(context);
}

StyioAST*
parse_hash_stmt_new_subset(StyioContext& context) {
  context.match_panic(StyioTokenType::TOK_HASH);
  context.skip();
  if (context.cur_tok_type() != StyioTokenType::NAME) {
    throw StyioSyntaxError("expected function name after # in new parser subset");
  }
  NameAST* tag_name = parse_name_unsafe(context);

  std::vector<ParamAST*> params;
  context.skip();
  if (context.cur_tok_type() == StyioTokenType::TOK_LPAREN) {
    params = parse_params(context);
  }

  std::variant<TypeAST*, TypeTupleAST*> ret_type = static_cast<TypeAST*>(nullptr);
  context.skip();
  if (context.cur_tok_type() == StyioTokenType::TOK_COLON) {
    context.move_forward(1, "new_stmt:hash_ret_colon");
    ret_type = parse_hash_ret_type_new_subset(context);
  }

  context.skip();
  if (context.cur_tok_type() == StyioTokenType::MATCH) {
    context.move_forward(1, "new_stmt:hash_match");
    if (context.cur_tok_type() == StyioTokenType::TOK_LCURBRAC) {
      return FunctionAST::Create(tag_name, true, params, ret_type, parse_cases_only(context));
    }

    std::vector<StyioAST*> rvals;
    do {
      rvals.push_back(parse_expr_new_subset(context));
    } while (context.try_match(StyioTokenType::TOK_COMMA));
    return FunctionAST::Create(tag_name, true, params, ret_type, CheckEqualAST::Create(rvals));
  }

  context.skip();
  if (context.cur_tok_type() == StyioTokenType::ITERATOR) {
    if (params.size() != 1) {
      throw StyioSyntaxError(
        context.mark_cur_tok("Confusing: The iterator (>>) can not be applied to multiple objects."));
    }
    return FunctionAST::Create(tag_name, true, params, ret_type, parse_iterator_with_forward(context, params[0]));
  }

  context.skip();
  bool is_unique = false;
  bool saw_assign = false;
  if (context.cur_tok_type() == StyioTokenType::WALRUS) {
    is_unique = true;
    saw_assign = true;
    context.move_forward(1, "new_stmt:hash_walrus");
  }
  else if (context.cur_tok_type() == StyioTokenType::TOK_EQUAL) {
    saw_assign = true;
    context.move_forward(1, "new_stmt:hash_equal");
  }
  else if (context.cur_tok_type() != StyioTokenType::ARROW_DOUBLE_RIGHT) {
    throw StyioSyntaxError("expected :=, =, or => in new parser subset hash function");
  }

  context.skip();
  if (saw_assign && context.cur_tok_type() == StyioTokenType::TOK_LPAREN) {
    params = parse_params(context);
  }

  context.skip();
  if (context.cur_tok_type() == StyioTokenType::ARROW_DOUBLE_RIGHT) {
    context.move_forward(1, "new_stmt:hash_arrow");

    context.skip();
    if (context.cur_tok_type() == StyioTokenType::TOK_LCURBRAC) {
      return FunctionAST::Create(tag_name, is_unique, params, ret_type, parse_block_with_forward(context));
    }
    StyioAST* ret_expr = parse_stmt_new_subset(context);
    return SimpleFuncAST::Create(tag_name, is_unique, params, ret_type, ret_expr);
  }
  if (saw_assign) {
    context.skip();
    StyioAST* ret_expr = parse_expr_new_subset(context);
    return SimpleFuncAST::Create(tag_name, is_unique, params, ret_type, ret_expr);
  }
  throw StyioSyntaxError("expected => or expression body in new parser subset hash function");
}

StyioAST*
parse_stmt_new_subset(StyioContext& context) {
  context.skip();

  if (context.cur_tok_type() == StyioTokenType::NAME) {
    const auto saved = context.save_cursor();
    const std::string id = context.cur_tok()->original;
    context.move_forward(1, "new_stmt:name_probe");
    context.skip();
    if (context.cur_tok_type() == StyioTokenType::TOK_COLON) {
      context.move_forward(1, "new_stmt:final_bind_colon");
      context.skip();
      if (context.cur_tok_type() != StyioTokenType::NAME) {
        throw StyioSyntaxError("expected simple type name after ':' in new parser subset");
      }
      TypeAST* ty = TypeAST::Create(context.cur_tok()->original);
      context.move_forward(1, "new_stmt:final_bind_type");
      context.skip();
      if (context.cur_tok_type() != StyioTokenType::WALRUS) {
        throw StyioSyntaxError("expected ':=' after type in new parser subset");
      }
      context.move_forward(1, "new_stmt:final_bind_walrus");
      context.skip();
      return FinalBindAST::Create(
        VarAST::Create(NameAST::Create(id), ty),
        parse_expr_new_subset(context));
    }
    if (context.cur_tok_type() == StyioTokenType::TOK_EQUAL) {
      context.move_forward(1, "new_stmt:flex_bind");
      context.skip();
      return FlexBindAST::Create(
        VarAST::Create(NameAST::Create(id)),
        parse_expr_new_subset(context));
    }
    StyioOpType cop = StyioOpType::Undefined;
    switch (context.cur_tok_type()) {
      case StyioTokenType::COMPOUND_ADD:
        cop = StyioOpType::Self_Add_Assign;
        break;
      case StyioTokenType::COMPOUND_SUB:
        cop = StyioOpType::Self_Sub_Assign;
        break;
      case StyioTokenType::COMPOUND_MUL:
        cop = StyioOpType::Self_Mul_Assign;
        break;
      case StyioTokenType::COMPOUND_DIV:
        cop = StyioOpType::Self_Div_Assign;
        break;
      case StyioTokenType::COMPOUND_MOD:
        cop = StyioOpType::Self_Mod_Assign;
        break;
      default:
        break;
    }
    if (cop != StyioOpType::Undefined) {
      context.move_forward(1, "new_stmt:compound_assign");
      context.skip();
      return BinOpAST::Create(cop, NameAST::Create(id), parse_expr_new_subset(context));
    }
    context.restore_cursor(saved);
  }

  if (context.cur_tok_type() == StyioTokenType::PRINT) {
    return parse_print_new_subset(context);
  }
  if (context.cur_tok_type() == StyioTokenType::TOK_HASH) {
    const auto saved = context.save_cursor();
    try {
      return parse_hash_stmt_new_subset(context);
    } catch (const std::exception&) {
      context.restore_cursor(saved);
      return parse_hash_tag(context);
    }
  }
  if (styio_new_parser_is_expr_subset_start(context.cur_tok_type())) {
    return parse_expr_new_subset(context);
  }
  throw StyioSyntaxError("unexpected statement token in new parser subset");
}

} // namespace

MainBlockAST*
parse_main_block_new_subset(StyioContext& context) {
  std::vector<std::unique_ptr<StyioAST>> statements_owned;
  while (true) {
    context.skip();
    if (context.cur_tok_type() == StyioTokenType::TOK_EOF) {
      break;
    }
    statements_owned.emplace_back(parse_stmt_new_subset(context));
  }
  std::vector<StyioAST*> statements;
  statements.reserve(statements_owned.size());
  for (auto& owned : statements_owned) {
    statements.push_back(owned.release());
  }
  return MainBlockAST::Create(statements);
}
