#include "Syntax.hpp"

#include <algorithm>
#include <cctype>
#include <stack>
#include <unordered_set>

#include "TreeSitterBackend.hpp"

namespace styio::ide {

namespace {

struct TolerantToken
{
  StyioTokenType type = StyioTokenType::UNKNOWN;
  std::string lexeme;
};

bool
is_identifier_start(char ch) {
  return std::isalpha(static_cast<unsigned char>(ch)) != 0 || ch == '_';
}

bool
is_identifier_continue(char ch) {
  return std::isalnum(static_cast<unsigned char>(ch)) != 0 || ch == '_';
}

std::vector<TolerantToken>
tokenize_tolerant(const std::string& text, std::vector<Diagnostic>& diagnostics) {
  std::vector<TolerantToken> tokens;
  std::size_t i = 0;

  auto push = [&](StyioTokenType type, std::string lexeme)
  {
    tokens.push_back(TolerantToken{type, std::move(lexeme)});
  };

  while (i < text.size()) {
    const char ch = text[i];
    if (ch == ' ' || ch == '\t' || ch == '\v' || ch == '\f') {
      push(StyioTokenType::TOK_SPACE, std::string(1, ch));
      i += 1;
      continue;
    }
    if (ch == '\n') {
      push(StyioTokenType::TOK_LF, "\n");
      i += 1;
      continue;
    }
    if (ch == '\r') {
      push(StyioTokenType::TOK_CR, "\r");
      i += 1;
      continue;
    }
    if (i + 1 < text.size() && text.compare(i, 2, "//") == 0) {
      std::size_t end = i + 2;
      while (end < text.size() && text[end] != '\n' && text[end] != '\r') {
        end += 1;
      }
      push(StyioTokenType::COMMENT_LINE, text.substr(i, end - i));
      i = end;
      continue;
    }
    if (i + 1 < text.size() && text.compare(i, 2, "/*") == 0) {
      std::size_t end = i + 2;
      while (end + 1 < text.size() && text.compare(end, 2, "*/") != 0) {
        end += 1;
      }
      if (end + 1 >= text.size()) {
        diagnostics.push_back(Diagnostic{
          TextRange{i, text.size()},
          DiagnosticSeverity::Error,
          "syntax",
          "unterminated block comment"});
        push(StyioTokenType::COMMENT_CLOSED, text.substr(i));
        break;
      }
      end += 2;
      push(StyioTokenType::COMMENT_CLOSED, text.substr(i, end - i));
      i = end;
      continue;
    }
    if (ch == '"') {
      std::size_t end = i + 1;
      while (end < text.size() && text[end] != '"') {
        if (text[end] == '\n' || text[end] == '\r') {
          break;
        }
        end += 1;
      }
      if (end >= text.size() || text[end] != '"') {
        diagnostics.push_back(Diagnostic{
          TextRange{i, end},
          DiagnosticSeverity::Error,
          "syntax",
          "unterminated string literal"});
        push(StyioTokenType::STRING, text.substr(i, end - i));
        i = end;
        continue;
      }
      end += 1;
      push(StyioTokenType::STRING, text.substr(i, end - i));
      i = end;
      continue;
    }
    if (is_identifier_start(ch)) {
      std::size_t end = i + 1;
      while (end < text.size() && is_identifier_continue(text[end])) {
        end += 1;
      }
      const std::string ident = text.substr(i, end - i);
      push(ident == "_" ? StyioTokenType::TOK_UNDLINE : StyioTokenType::NAME, ident);
      i = end;
      continue;
    }
    if (std::isdigit(static_cast<unsigned char>(ch)) != 0) {
      std::size_t end = i + 1;
      while (end < text.size() && std::isdigit(static_cast<unsigned char>(text[end])) != 0) {
        end += 1;
      }
      if (end + 1 < text.size() && text[end] == '.' && std::isdigit(static_cast<unsigned char>(text[end + 1])) != 0) {
        end += 1;
        while (end < text.size() && std::isdigit(static_cast<unsigned char>(text[end])) != 0) {
          end += 1;
        }
        push(StyioTokenType::DECIMAL, text.substr(i, end - i));
      } else {
        push(StyioTokenType::INTEGER, text.substr(i, end - i));
      }
      i = end;
      continue;
    }

    const auto maybe_two = [&](const char* token) -> bool
    {
      return i + 1 < text.size() && text.compare(i, 2, token) == 0;
    };

    if (i + 2 < text.size() && text.compare(i, 3, "...") == 0) {
      push(StyioTokenType::ELLIPSIS, "...");
      i += 3;
      continue;
    }
    if (maybe_two(":=")) {
      push(StyioTokenType::WALRUS, ":=");
      i += 2;
      continue;
    }
    if (maybe_two("?=")) {
      push(StyioTokenType::MATCH, "?=");
      i += 2;
      continue;
    }
    if (maybe_two("=>")) {
      push(StyioTokenType::ARROW_DOUBLE_RIGHT, "=>");
      i += 2;
      continue;
    }
    if (maybe_two("<|")) {
      push(StyioTokenType::YIELD_PIPE, "<|");
      i += 2;
      continue;
    }
    if (maybe_two("->")) {
      push(StyioTokenType::ARROW_SINGLE_RIGHT, "->");
      i += 2;
      continue;
    }
    if (maybe_two("<-")) {
      push(StyioTokenType::ARROW_SINGLE_LEFT, "<-");
      i += 2;
      continue;
    }
    if (maybe_two("<<")) {
      push(StyioTokenType::EXTRACTOR, "<<");
      i += 2;
      continue;
    }
    if (maybe_two(">>")) {
      push(StyioTokenType::ITERATOR, ">>");
      i += 2;
      continue;
    }
    if (maybe_two(">=")) {
      push(StyioTokenType::BINOP_GE, ">=");
      i += 2;
      continue;
    }
    if (maybe_two("<=")) {
      push(StyioTokenType::BINOP_LE, "<=");
      i += 2;
      continue;
    }
    if (maybe_two("==")) {
      push(StyioTokenType::BINOP_EQ, "==");
      i += 2;
      continue;
    }
    if (maybe_two("!=")) {
      push(StyioTokenType::BINOP_NE, "!=");
      i += 2;
      continue;
    }
    if (maybe_two("&&")) {
      push(StyioTokenType::LOGIC_AND, "&&");
      i += 2;
      continue;
    }
    if (maybe_two("||")) {
      push(StyioTokenType::LOGIC_OR, "||");
      i += 2;
      continue;
    }
    if (maybe_two("**")) {
      push(StyioTokenType::BINOP_POW, "**");
      i += 2;
      continue;
    }
    if (maybe_two("+=")) {
      push(StyioTokenType::COMPOUND_ADD, "+=");
      i += 2;
      continue;
    }
    if (maybe_two("-=")) {
      push(StyioTokenType::COMPOUND_SUB, "-=");
      i += 2;
      continue;
    }
    if (maybe_two("*=")) {
      push(StyioTokenType::COMPOUND_MUL, "*=");
      i += 2;
      continue;
    }
    if (maybe_two("/=")) {
      push(StyioTokenType::COMPOUND_DIV, "/=");
      i += 2;
      continue;
    }
    if (maybe_two("%=")) {
      push(StyioTokenType::COMPOUND_MOD, "%=");
      i += 2;
      continue;
    }
    if (maybe_two("[|")) {
      push(StyioTokenType::BOUNDED_BUFFER_OPEN, "[|");
      i += 2;
      continue;
    }
    if (maybe_two("|]")) {
      push(StyioTokenType::BOUNDED_BUFFER_CLOSE, "|]");
      i += 2;
      continue;
    }
    if (maybe_two("<~")) {
      push(StyioTokenType::WAVE_LEFT, "<~");
      i += 2;
      continue;
    }
    if (maybe_two("~>")) {
      push(StyioTokenType::WAVE_RIGHT, "~>");
      i += 2;
      continue;
    }
    if (maybe_two("??")) {
      push(StyioTokenType::DBQUESTION, "??");
      i += 2;
      continue;
    }

    switch (ch) {
      case '#':
        push(StyioTokenType::TOK_HASH, "#");
        break;
      case '$':
        push(StyioTokenType::TOK_DOLLAR, "$");
        break;
      case '(':
        push(StyioTokenType::TOK_LPAREN, "(");
        break;
      case ')':
        push(StyioTokenType::TOK_RPAREN, ")");
        break;
      case '[':
        push(StyioTokenType::TOK_LBOXBRAC, "[");
        break;
      case ']':
        push(StyioTokenType::TOK_RBOXBRAC, "]");
        break;
      case '{':
        push(StyioTokenType::TOK_LCURBRAC, "{");
        break;
      case '}':
        push(StyioTokenType::TOK_RCURBRAC, "}");
        break;
      case ':':
        push(StyioTokenType::TOK_COLON, ":");
        break;
      case ',':
        push(StyioTokenType::TOK_COMMA, ",");
        break;
      case '.':
        push(StyioTokenType::TOK_DOT, ".");
        break;
      case '+':
        push(StyioTokenType::TOK_PLUS, "+");
        break;
      case '-':
        push(StyioTokenType::TOK_MINUS, "-");
        break;
      case '*':
        push(StyioTokenType::TOK_STAR, "*");
        break;
      case '/':
        push(StyioTokenType::TOK_SLASH, "/");
        break;
      case '%':
        push(StyioTokenType::TOK_PERCENT, "%");
        break;
      case '=':
        push(StyioTokenType::TOK_EQUAL, "=");
        break;
      case '?':
        push(StyioTokenType::TOK_QUEST, "?");
        break;
      case '@':
        push(StyioTokenType::TOK_AT, "@");
        break;
      case '<':
        push(StyioTokenType::TOK_LANGBRAC, "<");
        break;
      case '>':
        push(StyioTokenType::TOK_RANGBRAC, ">");
        break;
      case '|':
        push(StyioTokenType::TOK_PIPE, "|");
        break;
      case '!':
        push(StyioTokenType::TOK_EXCLAM, "!");
        break;
      case '^':
        push(StyioTokenType::TOK_HAT, "^");
        break;
      case '~':
        push(StyioTokenType::TOK_TILDE, "~");
        break;
      default:
        push(StyioTokenType::UNKNOWN, std::string(1, ch));
        break;
    }
    i += 1;
  }

  tokens.push_back(TolerantToken{StyioTokenType::TOK_EOF, ""});
  return tokens;
}

bool
is_open_group(StyioTokenType type) {
  return type == StyioTokenType::TOK_LPAREN
    || type == StyioTokenType::TOK_LBOXBRAC
    || type == StyioTokenType::TOK_LCURBRAC
    || type == StyioTokenType::BOUNDED_BUFFER_OPEN;
}

bool
is_close_group(StyioTokenType type) {
  return type == StyioTokenType::TOK_RPAREN
    || type == StyioTokenType::TOK_RBOXBRAC
    || type == StyioTokenType::TOK_RCURBRAC
    || type == StyioTokenType::BOUNDED_BUFFER_CLOSE;
}

bool
is_matching_pair(StyioTokenType open, StyioTokenType close) {
  return (open == StyioTokenType::TOK_LPAREN && close == StyioTokenType::TOK_RPAREN)
    || (open == StyioTokenType::TOK_LBOXBRAC && close == StyioTokenType::TOK_RBOXBRAC)
    || (open == StyioTokenType::TOK_LCURBRAC && close == StyioTokenType::TOK_RCURBRAC)
    || (open == StyioTokenType::BOUNDED_BUFFER_OPEN && close == StyioTokenType::BOUNDED_BUFFER_CLOSE);
}

SyntaxNodeKind
node_kind_for_token(StyioTokenType type) {
  switch (type) {
    case StyioTokenType::TOK_LCURBRAC:
      return SyntaxNodeKind::Block;
    case StyioTokenType::TOK_LPAREN:
    case StyioTokenType::TOK_LBOXBRAC:
    case StyioTokenType::BOUNDED_BUFFER_OPEN:
      return SyntaxNodeKind::Group;
    default:
      return SyntaxNodeKind::Group;
  }
}

void
append_unique_diagnostic(
  std::vector<Diagnostic>& diagnostics,
  std::unordered_set<std::string>& seen,
  Diagnostic diagnostic
) {
  const std::string key =
    std::to_string(diagnostic.range.start) + ":" + std::to_string(diagnostic.range.end) + ":" + diagnostic.message;
  if (seen.insert(key).second) {
    diagnostics.push_back(std::move(diagnostic));
  }
}

void
append_unique_folding_range(
  std::vector<FoldingRange>& folding_ranges,
  std::unordered_set<std::string>& seen,
  FoldingRange range
) {
  const std::string key = std::to_string(range.range.start) + ":" + std::to_string(range.range.end);
  if (seen.insert(key).second) {
    folding_ranges.push_back(std::move(range));
  }
}

}  // namespace

bool
SyntaxToken::is_trivia() const {
  switch (type) {
    case StyioTokenType::TOK_SPACE:
    case StyioTokenType::TOK_LF:
    case StyioTokenType::TOK_CR:
    case StyioTokenType::COMMENT_LINE:
    case StyioTokenType::COMMENT_CLOSED:
      return true;
    default:
      return false;
  }
}

std::optional<std::size_t>
SyntaxSnapshot::token_index_at(std::size_t offset) const {
  for (std::size_t i = 0; i < tokens.size(); ++i) {
    if (tokens[i].range.start <= offset && offset <= tokens[i].range.end) {
      return i;
    }
  }
  return std::nullopt;
}

std::optional<std::size_t>
SyntaxSnapshot::previous_non_trivia_index(std::size_t offset) const {
  std::optional<std::size_t> best;
  for (std::size_t i = 0; i < tokens.size(); ++i) {
    if (tokens[i].range.end > offset) {
      break;
    }
    if (!tokens[i].is_trivia() && tokens[i].type != StyioTokenType::TOK_EOF) {
      best = i;
    }
  }
  return best;
}

std::optional<std::size_t>
SyntaxSnapshot::next_non_trivia_index(std::size_t offset) const {
  for (std::size_t i = 0; i < tokens.size(); ++i) {
    if (tokens[i].range.start < offset) {
      continue;
    }
    if (!tokens[i].is_trivia() && tokens[i].type != StyioTokenType::TOK_EOF) {
      return i;
    }
  }
  return std::nullopt;
}

PositionKind
SyntaxSnapshot::position_kind_at(std::size_t offset) const {
  const auto prev = previous_non_trivia_index(offset);
  const auto next = next_non_trivia_index(offset);
  const ScopeId depth = scope_hint_at(offset);

  if (prev.has_value()) {
    const StyioTokenType prev_type = tokens[*prev].type;
    if (prev_type == StyioTokenType::TOK_DOT) {
      return PositionKind::MemberAccess;
    }
    if (prev_type == StyioTokenType::TOK_COLON) {
      return PositionKind::Type;
    }
    if (prev_type == StyioTokenType::MATCH) {
      return PositionKind::Pattern;
    }
    if (prev_type == StyioTokenType::TOK_AT) {
      return PositionKind::AttrName;
    }
    if (prev_type == StyioTokenType::TOK_COMMA) {
      return PositionKind::CallArg;
    }
    if (prev_type == StyioTokenType::TOK_LF || prev_type == StyioTokenType::TOK_LCURBRAC) {
      return depth == 0 ? PositionKind::TopLevel : PositionKind::StmtStart;
    }
  }

  if (!prev.has_value() || (next.has_value() && tokens[*next].range.start == 0)) {
    return PositionKind::TopLevel;
  }

  return depth == 0 ? PositionKind::Expr : PositionKind::Expr;
}

std::vector<std::string>
SyntaxSnapshot::expected_tokens_at(std::size_t offset) const {
  switch (position_kind_at(offset)) {
    case PositionKind::TopLevel:
    case PositionKind::StmtStart:
      return {"NAME", "#", "@", "["};
    case PositionKind::Type:
      return {"NAME"};
    case PositionKind::Pattern:
      return {"NAME", "INTEGER", "STRING", "_"};
    case PositionKind::MemberAccess:
    case PositionKind::AttrName:
      return {"NAME"};
    case PositionKind::CallArg:
    case PositionKind::Expr:
      return {"NAME", "INTEGER", "STRING", "(", "[", "@"};
    case PositionKind::ImportPath:
      return {"NAME"};
  }
  return {"NAME"};
}

std::vector<std::string>
SyntaxSnapshot::expected_categories_at(std::size_t offset) const {
  switch (position_kind_at(offset)) {
    case PositionKind::TopLevel:
    case PositionKind::StmtStart:
      return {"value", "function", "keyword", "snippet"};
    case PositionKind::Type:
      return {"type"};
    case PositionKind::Pattern:
      return {"pattern", "literal"};
    case PositionKind::MemberAccess:
      return {"member"};
    case PositionKind::AttrName:
      return {"resource", "attribute"};
    case PositionKind::CallArg:
    case PositionKind::Expr:
      return {"value", "function", "builtin"};
    case PositionKind::ImportPath:
      return {"module"};
  }
  return {"value"};
}

std::string
SyntaxSnapshot::prefix_at(std::size_t offset) const {
  if (offset > buffer.size()) {
    offset = buffer.size();
  }

  std::size_t start = offset;
  const std::string& text = buffer.text();
  while (start > 0 && is_identifier_continue(text[start - 1])) {
    start -= 1;
  }
  return text.substr(start, offset - start);
}

ScopeId
SyntaxSnapshot::scope_hint_at(std::size_t offset) const {
  ScopeId depth = 0;
  std::vector<StyioTokenType> stack;
  for (const auto& token : tokens) {
    if (token.range.start > offset) {
      break;
    }
    if (is_open_group(token.type)) {
      stack.push_back(token.type);
    } else if (is_close_group(token.type) && !stack.empty() && is_matching_pair(stack.back(), token.type)) {
      stack.pop_back();
    }
  }
  for (StyioTokenType type : stack) {
    if (type == StyioTokenType::TOK_LCURBRAC) {
      depth += 1;
    }
  }
  return depth;
}

std::vector<std::size_t>
SyntaxSnapshot::node_path_at(std::size_t offset) const {
  std::vector<std::size_t> path;
  for (std::size_t i = 0; i < nodes.size(); ++i) {
    if (nodes[i].range.contains(offset)) {
      path.push_back(i);
    }
  }

  std::sort(
    path.begin(),
    path.end(),
    [&](std::size_t lhs, std::size_t rhs)
    {
      const std::size_t lhs_len = nodes[lhs].range.length();
      const std::size_t rhs_len = nodes[rhs].range.length();
      if (lhs_len != rhs_len) {
        return lhs_len > rhs_len;
      }
      if (nodes[lhs].range.start != nodes[rhs].range.start) {
        return nodes[lhs].range.start < nodes[rhs].range.start;
      }
      return lhs < rhs;
    });
  return path;
}

const SyntaxNode*
SyntaxSnapshot::node_at_offset(std::size_t offset) const {
  const auto path = node_path_at(offset);
  if (path.empty()) {
    return nullptr;
  }
  return &nodes[path.back()];
}

SyntaxSnapshot
SyntaxParser::parse(const DocumentSnapshot& snapshot) const {
  SyntaxSnapshot syntax;
  syntax.file_id = snapshot.file_id;
  syntax.snapshot_id = snapshot.snapshot_id;
  syntax.path = snapshot.path;
  syntax.buffer = snapshot.buffer;

  std::vector<Diagnostic> diagnostics;
  std::unordered_set<std::string> diagnostic_keys;
  std::unordered_set<std::string> folding_keys;
  const std::vector<TolerantToken> tolerant_tokens = tokenize_tolerant(snapshot.buffer.text(), diagnostics);
  for (const auto& diagnostic : diagnostics) {
    diagnostic_keys.insert(
      std::to_string(diagnostic.range.start) + ":" + std::to_string(diagnostic.range.end) + ":" + diagnostic.message);
  }

  syntax.tokens.reserve(tolerant_tokens.size());
  std::size_t cursor = 0;
  for (const auto& token : tolerant_tokens) {
    syntax.tokens.push_back(SyntaxToken{
      token.type,
      token.lexeme,
      TextRange{cursor, cursor + token.lexeme.size()}});
    cursor += token.lexeme.size();
  }

  if (const auto tree_sitter_result = parse_with_tree_sitter(snapshot); tree_sitter_result.has_value()) {
    syntax.backend = SyntaxBackendKind::TreeSitter;
    syntax.nodes = tree_sitter_result->nodes;
    syntax.folding_ranges = tree_sitter_result->folding_ranges;
    for (const auto& range : syntax.folding_ranges) {
      folding_keys.insert(std::to_string(range.range.start) + ":" + std::to_string(range.range.end));
    }
    for (auto diagnostic : tree_sitter_result->diagnostics) {
      append_unique_diagnostic(diagnostics, diagnostic_keys, std::move(diagnostic));
    }
  } else {
    syntax.backend = SyntaxBackendKind::Tolerant;
    syntax.nodes.push_back(SyntaxNode{
      SyntaxNodeKind::Root,
      "root",
      TextRange{0, snapshot.buffer.size()},
      {}});
  }

  std::vector<std::pair<std::size_t, StyioTokenType>> group_stack;
  for (std::size_t i = 0; i < syntax.tokens.size(); ++i) {
    const auto& token = syntax.tokens[i];
    if (is_open_group(token.type)) {
      group_stack.emplace_back(i, token.type);
      continue;
    }

    if (!is_close_group(token.type)) {
      continue;
    }

    if (group_stack.empty() || !is_matching_pair(group_stack.back().second, token.type)) {
      append_unique_diagnostic(
        diagnostics,
        diagnostic_keys,
        Diagnostic{
          token.range,
          DiagnosticSeverity::Error,
          "syntax",
          "unmatched closing token " + token.lexeme});
      continue;
    }

    const std::size_t open_index = group_stack.back().first;
    const auto open_type = group_stack.back().second;
    group_stack.pop_back();
    syntax.matching_tokens[open_index] = i;
    syntax.matching_tokens[i] = open_index;

    const TextRange node_range{syntax.tokens[open_index].range.start, token.range.end};
    if (syntax.backend == SyntaxBackendKind::Tolerant) {
      syntax.nodes.push_back(SyntaxNode{
        node_kind_for_token(open_type),
        syntax.tokens[open_index].lexeme,
        node_range,
        {}});
    }
    if (open_type == StyioTokenType::TOK_LCURBRAC
        || open_type == StyioTokenType::TOK_LPAREN
        || open_type == StyioTokenType::TOK_LBOXBRAC) {
      append_unique_folding_range(syntax.folding_ranges, folding_keys, FoldingRange{node_range});
    }
  }

  for (const auto& open : group_stack) {
    append_unique_diagnostic(
      diagnostics,
      diagnostic_keys,
      Diagnostic{
        syntax.tokens[open.first].range,
        DiagnosticSeverity::Error,
        "syntax",
        "unclosed opening token " + syntax.tokens[open.first].lexeme});
  }

  syntax.diagnostics = std::move(diagnostics);
  return syntax;
}

}  // namespace styio::ide
