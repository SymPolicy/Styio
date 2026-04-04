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
expr_map_op(StyioTokenType type) {
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
    case StyioTokenType::BINOP_GT:
      return StyioOpType::Greater_Than;
    case StyioTokenType::BINOP_GE:
      return StyioOpType::Greater_Than_Equal;
    case StyioTokenType::BINOP_LT:
      return StyioOpType::Less_Than;
    case StyioTokenType::BINOP_LE:
      return StyioOpType::Less_Than_Equal;
    case StyioTokenType::BINOP_EQ:
      return StyioOpType::Equal;
    case StyioTokenType::BINOP_NE:
      return StyioOpType::Not_Equal;
    case StyioTokenType::LOGIC_AND:
      return StyioOpType::Logic_AND;
    case StyioTokenType::LOGIC_OR:
      return StyioOpType::Logic_OR;
    default:
      return StyioOpType::Undefined;
  }
}

class StyioExprSubsetParser
{
private:
  StyioContext& context_;

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
        return NameAST::Create(name);
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

      const StyioOpType op = expr_map_op(tok);
      if (op == StyioOpType::Undefined) {
        break;
      }

      context_.move_forward(1, "new_expr:binop");
      const int next_min = expr_is_right_assoc(tok) ? prec : (prec + 1);
      StyioAST* rhs = parse_expression(next_min);
      lhs = BinOpAST::Create(op, lhs, rhs);
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
    case StyioTokenType::TOK_PLUS:
    case StyioTokenType::TOK_MINUS:
    case StyioTokenType::TOK_STAR:
    case StyioTokenType::TOK_SLASH:
    case StyioTokenType::TOK_PERCENT:
    case StyioTokenType::BINOP_POW:
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
    case StyioTokenType::PRINT:
    case StyioTokenType::TOK_COMMA:
    case StyioTokenType::TOK_EQUAL:
    case StyioTokenType::TOK_COLON:
    case StyioTokenType::WALRUS:
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
  return type == StyioTokenType::PRINT || styio_new_parser_is_expr_subset_start(type);
}

namespace {

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
