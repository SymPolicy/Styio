#pragma once
#ifndef STYIO_PARSER_H_
#define STYIO_PARSER_H_

#include <algorithm>
#include <regex>

#include "../StyioAST/AST.hpp"
#include "../StyioToken/Token.hpp"
#include "../StyioUnicode/Unicode.hpp"
#include "ParserLookahead.hpp"

using std::pair;
using std::string;
using std::unordered_map;
using std::vector;

using std::cout;
using std::endl;

using std::make_shared;
using std::make_unique;
using std::shared_ptr;
using std::unique_ptr;

class StyioContext;
class StyioParser;

enum class StyioParserEngine
{
  Legacy,
  Nightly,
  New = Nightly,
};

enum class StyioParseMode
{
  Strict,
  Recovery,
};

struct StyioParserRouteStats
{
  size_t nightly_subset_statements = 0;
  size_t nightly_declined_statements = 0;
  size_t legacy_fallback_statements = 0;
  size_t nightly_internal_legacy_bridges = 0;
};

struct StyioParseDiagnostic
{
  size_t start = 0;
  size_t end = 0;
  std::string message;
};

class StyioContext
{
private:
  static StyioToken* eof_fallback_token() {
    static StyioToken* tok = StyioToken::CreatePersistent(StyioTokenType::TOK_EOF, "EOF");
    return tok;
  }

  bool has_char_index(size_t idx) const {
    return idx < code.size();
  }

  char char_at_or_nul(size_t idx) const {
    return has_char_index(idx) ? code[idx] : '\0';
  }

  size_t cur_pos = 0; /* current position */

  string file_name;
  string code;
  vector<pair<size_t, size_t>> line_seps; /* line separations */

  size_t index_of_token = 0;
  std::vector<StyioToken*> tokens;

  shared_ptr<StyioAST> ast;
  unordered_map<string, shared_ptr<StyioAST>> constants;
  unordered_map<string, shared_ptr<StyioAST>> variables;
  StyioParserRouteStats* parser_route_stats = nullptr;
  StyioParseMode parse_mode_ = StyioParseMode::Strict;
  std::vector<StyioParseDiagnostic> parse_diagnostics_;

  bool debug_mode = false;

  std::vector<std::vector<std::pair<size_t, size_t>>> token_segmentation; /* offset, length */
  std::vector<std::pair<size_t, size_t>> token_coordinates;               /* row, col */
  std::vector<std::string> token_lines;                                   /* lines */
  std::vector<size_t> token_cursor_positions;                             /* token index -> cur_pos */
  std::vector<size_t> next_non_trivia_index;                              /* token index -> next non-trivia */
  std::vector<size_t> next_non_space_no_linebreak_index;                  /* token index -> next non-space */
  bool diagnostic_cache_ready = false;

  StyioRepr* ast_repr = nullptr;

  void initialize_token_navigation_tables() {
    token_cursor_positions.assign(tokens.size() + 1, 0);
    for (size_t i = 0; i < tokens.size(); i++) {
      token_cursor_positions[i + 1] = token_cursor_positions[i] + tokens[i]->length();
    }

    next_non_trivia_index.assign(tokens.size() + 1, tokens.size());
    next_non_space_no_linebreak_index.assign(tokens.size() + 1, tokens.size());

    size_t next_non_trivia = tokens.size();
    size_t next_non_space = tokens.size();
    for (size_t cursor = tokens.size(); cursor > 0; cursor--) {
      const size_t i = cursor - 1;
      if (!styio_is_trivia_token(tokens[i]->type)) {
        next_non_trivia = i;
      }
      if (tokens[i]->type != StyioTokenType::TOK_SPACE) {
        next_non_space = i;
      }
      next_non_trivia_index[i] = next_non_trivia;
      next_non_space_no_linebreak_index[i] = next_non_space;
    }
  }

  void initialize_token_coordinates_and_segmentations() {
    token_coordinates.clear();
    token_segmentation.clear();
    token_coordinates.reserve(tokens.size());

    /* token_segmentation */
    size_t offset = 0;
    std::vector<std::pair<size_t, size_t>> seg_line;

    /* token_coordinates */
    size_t row = 0;
    size_t col = 0;

    /* token_lines */
    for (size_t i = 0; i < tokens.size(); i++) {
      token_coordinates.push_back(std::make_pair(row, col));

      if (tokens[i]->type == StyioTokenType::TOK_LF) {
        /* token_coordinates */
        seg_line.push_back(std::make_pair(offset, tokens[i]->length()));
        token_segmentation.push_back(seg_line);
        seg_line.clear();
        offset = 0; /* reset to the start of the line */

        /* tok_loc */
        row += 1;
        col = 0;
      }
      else {
        /* token_segmentation */
        seg_line.push_back(std::make_pair(offset, tokens[i]->length()));
        offset += tokens[i]->length();

        /* token_coordinates */
        col += 1;
      }
    }
  }

  void initialize_token_lines() {
    token_lines.clear();

    /* token_lines */
    std::string tmp_line;
    for (auto c : code) {
      tmp_line += c;

      if (c == '\n') {
        token_lines.push_back(tmp_line);
        tmp_line.clear();
      }
    }
  }

  void ensure_diagnostic_cache() {
    if (diagnostic_cache_ready) {
      return;
    }
    initialize_token_coordinates_and_segmentations();
    initialize_token_lines();
    diagnostic_cache_ready = true;
  }

  void ensure_ast_repr() {
    if (ast_repr == nullptr) {
      ast_repr = new StyioRepr();
    }
  }

  size_t token_cursor_position_at(size_t index) const {
    if (index >= token_cursor_positions.size()) {
      if (token_cursor_positions.empty()) {
        return 0;
      }
      return token_cursor_positions.back();
    }
    return token_cursor_positions[index];
  }

  size_t next_non_trivia_from(size_t index) const {
    if (index >= next_non_trivia_index.size()) {
      return tokens.size();
    }
    return next_non_trivia_index[index];
  }

  size_t next_non_space_no_linebreak_from(size_t index) const {
    if (index >= next_non_space_no_linebreak_index.size()) {
      return tokens.size();
    }
    return next_non_space_no_linebreak_index[index];
  }

  void advance_to_token_index(size_t target) {
    if (target > tokens.size()) {
      target = tokens.size();
    }
    index_of_token = target;
    cur_pos = token_cursor_position_at(target);
  }

  struct TokenNesting
  {
    int paren = 0;
    int bracket = 0;
    int brace = 0;
    int bounded = 0;
  };

  TokenNesting token_nesting_before(size_t index) const {
    TokenNesting nesting;
    const size_t limit = std::min(index, tokens.size());
    for (size_t i = 0; i < limit; ++i) {
      switch (tokens[i]->type) {
        case StyioTokenType::TOK_LPAREN:
          nesting.paren += 1;
          break;
        case StyioTokenType::TOK_RPAREN:
          nesting.paren = std::max(0, nesting.paren - 1);
          break;
        case StyioTokenType::TOK_LBOXBRAC:
          nesting.bracket += 1;
          break;
        case StyioTokenType::TOK_RBOXBRAC:
          nesting.bracket = std::max(0, nesting.bracket - 1);
          break;
        case StyioTokenType::TOK_LCURBRAC:
          nesting.brace += 1;
          break;
        case StyioTokenType::TOK_RCURBRAC:
          nesting.brace = std::max(0, nesting.brace - 1);
          break;
        case StyioTokenType::BOUNDED_BUFFER_OPEN:
          nesting.bounded += 1;
          break;
        case StyioTokenType::BOUNDED_BUFFER_CLOSE:
          nesting.bounded = std::max(0, nesting.bounded - 1);
          break;
        default:
          break;
      }
    }
    return nesting;
  }

public:
  StyioContext(
    const string& file_name,
    const string& code_text,
    vector<pair<size_t, size_t>> line_seps,
    std::vector<StyioToken*> tokens,
    bool debug_mode = false
  ) :
      file_name(file_name),
      code(code_text),
      line_seps(line_seps),
      tokens(tokens),
      debug_mode(debug_mode) {
    initialize_token_navigation_tables();
  }

  ~StyioContext() {
    delete ast_repr;
  }

  static StyioContext* Create(
    const string& file_name,
    const string& code_text,
    vector<pair<size_t, size_t>> line_seps,
    std::vector<StyioToken*> tokens,
    bool debug_mode = false
  ) {
    return new StyioContext(
      file_name,
      code_text,
      line_seps,
      tokens,
      debug_mode
    );
  }

  /* Get `code` */
  const string&
  get_code() const {
    return code;
  }

  /*
    === Token Start
  */

  StyioToken* cur_tok() {
    if (index_of_token >= tokens.size()) {
      return eof_fallback_token();
    }
    return tokens[index_of_token];
  }

  StyioTokenType cur_tok_type() {
    return cur_tok()->type;
  }

  const std::vector<StyioToken*>&
  get_tokens() const {
    return tokens;
  }

  size_t
  get_token_index() const {
    return index_of_token;
  }

  void move_forward(size_t steps = 1, std::string caller = "") {
    // std::cout << "[" << index_of_token << "] " << caller << "(`" << cur_tok()->as_str() << "`)" << ", step: " << steps << std::endl;
    if (index_of_token >= tokens.size()) {
      advance_to_token_index(tokens.size());
      return;
    }
    advance_to_token_index(index_of_token + steps);
  }

  std::pair<size_t, size_t>
  save_cursor() const {
    return {index_of_token, cur_pos};
  }

  void
  restore_cursor(std::pair<size_t, size_t> c) {
    index_of_token = c.first;
    cur_pos = c.second;
  }

  void
  set_parse_mode(StyioParseMode mode) {
    parse_mode_ = mode;
  }

  StyioParseMode
  parse_mode() const {
    return parse_mode_;
  }

  bool
  is_recovery_mode() const {
    return parse_mode_ == StyioParseMode::Recovery;
  }

  void
  clear_parse_diagnostics() {
    parse_diagnostics_.clear();
  }

  const std::vector<StyioParseDiagnostic>&
  parse_diagnostics() const {
    return parse_diagnostics_;
  }

  size_t
  current_token_end_pos() const {
    if (index_of_token >= tokens.size()) {
      return token_cursor_position_at(tokens.size());
    }
    return token_cursor_position_at(index_of_token + 1);
  }

  void
  record_parse_diagnostic(size_t start, size_t end, std::string message) {
    if (end < start) {
      end = start;
    }
    if (end == start) {
      end += 1;
    }
    parse_diagnostics_.push_back(StyioParseDiagnostic{
      start,
      end,
      std::move(message)});
  }

  bool
  recover_to_statement_boundary(size_t statement_start_index) {
    const TokenNesting base = token_nesting_before(statement_start_index);
    TokenNesting nesting = base;
    size_t scan = std::max(statement_start_index, index_of_token);

    while (scan < tokens.size()) {
      const StyioTokenType type = tokens[scan]->type;

      if (type == StyioTokenType::TOK_RCURBRAC
          && nesting.paren == base.paren
          && nesting.bracket == base.bracket
          && nesting.bounded == base.bounded
          && nesting.brace == base.brace) {
        advance_to_token_index(scan);
        return false;
      }

      if ((type == StyioTokenType::TOK_LF || type == StyioTokenType::TOK_CR)
          && nesting.paren == base.paren
          && nesting.bracket == base.bracket
          && nesting.bounded == base.bounded
          && nesting.brace == base.brace) {
        advance_to_token_index(scan + 1);
        skip();
        return true;
      }

      switch (type) {
        case StyioTokenType::TOK_LPAREN:
          nesting.paren += 1;
          break;
        case StyioTokenType::TOK_RPAREN:
          nesting.paren = std::max(base.paren, nesting.paren - 1);
          break;
        case StyioTokenType::TOK_LBOXBRAC:
          nesting.bracket += 1;
          break;
        case StyioTokenType::TOK_RBOXBRAC:
          nesting.bracket = std::max(base.bracket, nesting.bracket - 1);
          break;
        case StyioTokenType::TOK_LCURBRAC:
          nesting.brace += 1;
          break;
        case StyioTokenType::TOK_RCURBRAC:
          nesting.brace = std::max(base.brace, nesting.brace - 1);
          if (nesting.paren == base.paren
              && nesting.bracket == base.bracket
              && nesting.bounded == base.bounded
              && nesting.brace == base.brace) {
            advance_to_token_index(scan + 1);
            skip();
            return true;
          }
          break;
        case StyioTokenType::BOUNDED_BUFFER_OPEN:
          nesting.bounded += 1;
          break;
        case StyioTokenType::BOUNDED_BUFFER_CLOSE:
          nesting.bounded = std::max(base.bounded, nesting.bounded - 1);
          break;
        default:
          break;
      }

      scan += 1;
    }

    advance_to_token_index(tokens.size());
    return false;
  }

  void
  set_parser_route_stats_latest(StyioParserRouteStats* stats) {
    parser_route_stats = stats;
  }

  StyioParserRouteStats*
  parser_route_stats_latest() {
    return parser_route_stats;
  }

  void
  note_nightly_internal_legacy_bridge_latest() {
    if (parser_route_stats != nullptr) {
      parser_route_stats->nightly_internal_legacy_bridges += 1;
    }
  }

  inline void skip() {
    if (index_of_token >= tokens.size()) {
      return;
    }
    const size_t next = next_non_trivia_from(index_of_token);
    if (next != index_of_token) {
      advance_to_token_index(next);
    }
  }

  /** Spaces only — do not cross newlines (so `b` and `(` on different lines are not a call). */
  inline void skip_spaces_no_linebreak() {
    if (index_of_token >= tokens.size()) {
      return;
    }
    const size_t next = next_non_space_no_linebreak_from(index_of_token);
    if (next != index_of_token) {
      advance_to_token_index(next);
    }
  }

  /* check length of consecutive sequence of token */
  size_t check_seq_of(StyioTokenType type) {
    size_t start = this->index_of_token;
    size_t count = 0;

    while (
      start + count < tokens.size()
      && tokens.at(start + count)->type == type
    ) {
      count += 1;
    }

    return count;
  }

  bool check(StyioTokenType type) {
    return type == cur_tok_type();
  }

  bool try_check(StyioTokenType target) {
    const size_t probe = next_non_trivia_from(index_of_token);
    if (probe >= tokens.size()) {
      return false;
    }
    return tokens[probe]->type == target;
  }

  bool match(StyioTokenType type) {
    auto cur_type = this->cur_tok_type();
    if (type == cur_type) {
      this->move_forward(1, "match");
      return true;
    }

    return false;
  }

  bool match_panic(StyioTokenType type, std::string errmsg = "") {
    if (cur_tok_type() == type) {
      this->move_forward(1, "match_panic");
      return true;
    }

    if (errmsg.empty()) {
      throw StyioSyntaxError(
        string("match_panic(token)"), mark_cur_tok(std::string("which is expected to be ") + StyioToken::getTokName(type))
      );
    }
    else {
      throw StyioSyntaxError(mark_cur_tok(errmsg));
    }
  }

  bool map_match(StyioTokenType target) {
    auto it = StyioTokenMap.find(target);
    /* found */
    if (it != StyioTokenMap.end()) {
      bool is_same = true;
      auto tok_seq = it->second;
      if (index_of_token + tok_seq.size() > tokens.size()) {
        return false;
      }
      for (size_t i = 0; i < tok_seq.size(); i++) {
        if (tok_seq.at(i) != tokens[index_of_token + i]->type) {
          std::cout << "map match " << StyioToken::getTokName(tok_seq.at(i)) << " not equal "
                    << StyioToken::getTokName(tokens[index_of_token + i]->type) << std::endl;
          is_same = false;
        }
      }

      if (is_same) {
        move_forward(tok_seq.size(), "map_match");
      }

      return is_same;
    }
    /* not found */
    else {
      std::string errmsg = "Undefined: " + StyioToken::getTokName(target) + " not found in StyioTokenMap.";
      throw StyioSyntaxError(label_cur_line(cur_pos, errmsg));
    }
  }

  bool try_match(StyioTokenType target) {
    // just match
    if (index_of_token < tokens.size() && tokens[index_of_token]->type == target) {
      advance_to_token_index(index_of_token + 1);
      return true;
    }

    const size_t probe = next_non_trivia_from(index_of_token);
    if (probe < tokens.size() && tokens[probe]->type == target) {
      advance_to_token_index(probe + 1);
      return true;
    }
    return false;
  }

  bool try_match_panic(StyioTokenType target, std::string errmsg = "") {
    // just match
    if (index_of_token < tokens.size() && tokens[index_of_token]->type == target) {
      advance_to_token_index(index_of_token + 1);
      return true;
    }

    const size_t probe = next_non_trivia_from(index_of_token);
    if (probe < tokens.size() && tokens[probe]->type == target) {
      advance_to_token_index(probe + 1);
      return true;
    }

    if (probe >= tokens.size()) {
      throw StyioParseError(label_cur_line(
        cur_pos,
        "try_match_panic(token): Couldn't find " + StyioToken::getTokName(target) + " until the end of the file."
      ));
    }

    if (errmsg.empty()) {
      throw StyioSyntaxError(
        string("try_match_panic(token)")
        + label_cur_line(
          cur_pos,
          std::string("which is expected to be ") + StyioToken::getTokName(target)
        )
      );
    }
    throw StyioSyntaxError(label_cur_line(cur_pos, errmsg));
  }

  /*
    === Token End ===
  */

  /* Get `pos` */
  size_t get_curr_pos() {
    return cur_pos;
  }

  /* Get Current Character */
  char get_curr_char() {
    return char_at_or_nul(cur_pos);
  }

  size_t find_line_index(
    int p = -1
  ) {
    const size_t total_lines = line_seps.size();
    if (total_lines == 0) {
      return 0;
    }
    size_t line_index = 0;

    if (p < 0) {
      p = cur_pos;
    }

    if (debug_mode) {
      cout << "find_line_index(), starts with position: " << p << " current character: " << get_curr_char() << "\ninitial: line [" << line_index << "]" << endl;
    }

    bool binary_search = false;
    if (binary_search) {
      line_index = total_lines / 2;

      for (size_t i = 0; i < total_lines; i++) {
        if (debug_mode) {
          cout << "[" << line_index << "] is ";
        }

        if (p < line_seps[line_index].first) {
          line_index = line_index / 2;
          if (debug_mode) {
            cout << "too large, go to: [" << line_index << "]" << endl;
          }
        }
        else if (p > (line_seps[line_index].first + line_seps[line_index].second)) {
          line_index = (line_index + total_lines) / 2;
          if (debug_mode) {
            cout << "too small, go to: [" << line_index << "]" << endl;
          }
        }
        else {
          if (debug_mode) {
            cout << "result: [" << line_index << "]" << endl;
          }
          break;
        }
      }
    }
    else {
      size_t pos = static_cast<size_t>(p);
      for (size_t curr_line_index = 0; curr_line_index < total_lines; curr_line_index += 1) {
        if (line_seps[curr_line_index].first <= pos
            && pos <= (line_seps[curr_line_index].first + line_seps[curr_line_index].second)) {
          return curr_line_index;
        }
      }
    }

    return line_index;
  }

  string label_cur_line(
    int start = -1,
    std::string endswith = ""
  ) {
    string output("\n");

    if (start < 0)
      start = cur_pos;

    if (start < 0) {
      start = 0;
    }

    size_t pos = static_cast<size_t>(start);
    if (line_seps.empty()) {
      if (pos > code.size()) {
        pos = code.size();
      }

      output += "File \"" + file_name + "\", Line 0, At " + std::to_string(pos) + ":\n\n";
      if (code.empty()) {
        output += "<empty>\n";
      }
      else {
        output += code + "\n";
      }
      output += std::string(pos, ' ') + std::string("^");
      if (endswith.empty()) {
        output += "\n";
      }
      else {
        output += " " + endswith + "\n";
      }
      return output;
    }

    size_t lindex = find_line_index(static_cast<int>(pos));
    if (lindex >= line_seps.size()) {
      lindex = line_seps.size() - 1;
    }

    size_t line_start = line_seps[lindex].first;
    if (line_start > code.size()) {
      line_start = code.size();
    }

    size_t line_len = line_seps[lindex].second;
    if (line_start + line_len > code.size()) {
      line_len = code.size() - line_start;
    }

    size_t offset = 0;
    if (pos > line_start) {
      offset = pos - line_start;
      if (offset > line_len) {
        offset = line_len;
      }
    }

    output += "File \"" + file_name + "\", Line " + std::to_string(lindex) + ", At " + std::to_string(offset) + ":\n\n";
    output += code.substr(line_start, line_len) + "\n";
    output += std::string(offset, ' ') + std::string("^");

    if (endswith.empty()) {
      size_t tail = 0;
      if (line_len > offset) {
        tail = line_len - offset - 1;
      }
      output += std::string(tail, '-') + "\n";
    }
    else {
      output += " " + endswith + "\n";
    }

    return output;
  }

  std::string mark_cur_tok(std::string comment = "") {
    std::string result;
    ensure_diagnostic_cache();

    if (index_of_token >= token_coordinates.size()) {
      return comment.empty() ? std::string("Unknown token location") : comment;
    }

    auto row_num = token_coordinates[index_of_token].first;
    auto col_num = token_coordinates[index_of_token].second;

    if (row_num >= token_segmentation.size() || row_num >= token_lines.size()) {
      return comment.empty() ? std::string("Unknown token location") : comment;
    }
    if (col_num >= token_segmentation[row_num].size()) {
      return comment.empty() ? std::string("Unknown token location") : comment;
    }

    auto offset = token_segmentation[row_num][col_num].first;
    auto length = token_segmentation[row_num][col_num].second;

    auto that_line = token_lines[row_num];

    if (offset > that_line.length()) {
      offset = that_line.length();
    }
    if (offset + length > that_line.length()) {
      length = that_line.length() - offset;
    }

    result += that_line;
    result += std::string(offset, ' ') + std::string(length, '^') + std::string((that_line.length() - offset - length), '-') + " " + comment;

    return result;
  }

  // No Boundary Check !
  // | + n => move forward n steps
  // | - n => move backward n steps
  void move(size_t steps) {
    if (cur_pos >= code.size()) {
      cur_pos = code.size();
      return;
    }
    if (steps > code.size() - cur_pos) {
      cur_pos = code.size();
      return;
    }
    cur_pos += steps;
  }

  /* Check Value */
  bool check_next(char value) {
    return has_char_index(cur_pos) && code[cur_pos] == value;
  }

  /* Check Value */
  bool check_next(const string& value) {
    if (cur_pos > code.size()) {
      return false;
    }
    if (value.empty()) {
      return true;
    }
    if (cur_pos + value.size() > code.size()) {
      return false;
    }
    return code.compare(cur_pos, value.size(), value) == 0;
  }

  /* Move Until */
  void move_until(char value) {
    while (cur_pos < code.size() && not check_next(value)) {
      move(1);
    }
  }

  void move_until(const string& value) {
    while (cur_pos < code.size() && not check_next(value)) {
      move(1);
    }
  }

  /* Check & Drop */
  bool check_drop(char value) {
    if (check_next(value)) {
      move(1);
      return true;
    }
    else {
      return false;
    }
  }

  /* Check & Drop */
  bool check_drop(const string& value) {
    if (check_next(value)) {
      move(value.size());
      return true;
    }
    else {
      return false;
    }
  }

  /* Find & Drop */
  bool find_drop(char value) {
    while (cur_pos < code.size()) {
      if (StyioUnicode::is_space(get_curr_char())) {
        move(1);
      }
      else if (check_next("//")) {
        pass_over('\n');
      }
      else if (check_next("/*")) {
        pass_over("*/");
      }
      else {
        if (check_next(value)) {
          move(1);
          return true;
        }
        else {
          return false;
        }
      }
    }

    return false;
  }

  /* Find & Drop */
  bool find_drop(string value) {
    while (cur_pos < code.size()) {
      if (StyioUnicode::is_space(get_curr_char())) {
        move(1);
      }
      else if (check_next("//")) {
        pass_over('\n');
      }
      else if (check_next("/*")) {
        pass_over("*/");
      }
      else {
        if (check_next(value)) {
          move(value.size());
          return true;
        }
        else {
          return false;
        }
      }
    }

    return false;
  }

  /* Pass Over */
  void pass_over(char value) {
    while (cur_pos < code.size()) {
      if (check_next(value)) {
        move(1);
        return;
      }
      else {
        move(1);
      }
    }
  }

  /* Pass Over */
  void pass_over(const string& value) {
    while (cur_pos < code.size()) {
      if (check_next(value)) {
        move(value.size());
        return;
      }
      else {
        move(1);
      }
    }
  }

  /* Peak Check */
  bool check_ahead(int steps, char value) {
    if (steps >= 0) {
      size_t idx = cur_pos + static_cast<size_t>(steps);
      if (idx < cur_pos) {
        return false;
      }
      return has_char_index(idx) && code[idx] == value;
    }

    size_t back = static_cast<size_t>(-steps);
    if (back > cur_pos) {
      return false;
    }
    size_t idx = cur_pos - back;
    return has_char_index(idx) && code[idx] == value;
  }

  bool peak_isdigit(int steps) {
    if (steps < 0) {
      size_t back = static_cast<size_t>(-steps);
      if (back > cur_pos) {
        return false;
      }
      return StyioUnicode::is_digit(code[cur_pos - back]);
    }

    size_t idx = cur_pos + static_cast<size_t>(steps);
    if (idx < cur_pos || !has_char_index(idx)) {
      return false;
    }
    return StyioUnicode::is_digit(code[idx]);
  }

  /* Drop White Spaces */
  void drop_white_spaces() {
    while (check_next(' ')) {
      move(1);
    }
  }

  /* Drop Spaces */
  void drop_all_spaces() {
    while (has_char_index(cur_pos) && StyioUnicode::is_space(code[cur_pos])) {
      move(1);
    }
  }

  /* Drop Spaces & Comments */
  void drop_all_spaces_comments() {
    while (has_char_index(cur_pos)) {
      if (StyioUnicode::is_space(code[cur_pos])) {
        move(1);
      }
      else if (check_next("//")) {
        pass_over('\n');
      }
      else if (check_next("/*")) {
        pass_over("*/");
      }
      else {
        break;
      }
    }
  }

  /* Match(Next) -> Panic */
  bool check_drop_panic(char value, std::string errmsg = "") {
    if (check_next(value)) {
      move(1);
      return true;
    }

    if (errmsg.empty()) {
      throw StyioSyntaxError(string("check_drop_panic(char)") + label_cur_line(cur_pos, std::string("which is expected to be ") + std::string(1, char(value))));
    }
    else {
      throw StyioSyntaxError(label_cur_line(cur_pos, errmsg));
    }
  }

  /* (Char) Find & Drop -> Panic */
  bool find_drop_panic(char value) {
    /* ! No Boundary Check ! */
    while (true) {
      if (StyioUnicode::is_space(get_curr_char())) {
        move(1);
      }
      else if (check_next("//")) {
        pass_over('\n');
      }
      else if (check_next("/*")) {
        pass_over("*/");
      }
      else {
        if (check_next(value)) {
          move(1);
          return true;
        }
        else {
          string errmsg = string("find_drop_panic(char)") + label_cur_line(cur_pos, std::string("which is expected to be ") + std::string(1, char(value)));
          throw StyioSyntaxError(errmsg);
        }
      }
    }
  }

  /* (String) Find & Drop -> Panic */
  bool find_drop_panic(string value) {
    /* ! No Boundary Check ! */
    while (true) {
      if (StyioUnicode::is_space(get_curr_char()))
        move(1);
      else if (check_next("//"))
        pass_over('\n');
      else if (check_next("/*"))
        pass_over("*/");
      else {
        if (check_next(value)) {
          move(value.size());
          return true;
        }
        else {
          string errmsg = string("find_drop_panic(string)") + label_cur_line(cur_pos, std::string("which is expected to be ") + value);
          throw StyioSyntaxError(errmsg);
        }
      }
    }
  }

  /* Find & Drop -> Panic */
  bool find_panic(const string& value) {
    /* ! No Boundary Check ! */
    while (true) {
      if (StyioUnicode::is_space(get_curr_char())) {
        move(1);
      }
      else if (check_next("//")) {
        pass_over('\n');
      }
      else if (check_next("/*")) {
        pass_over("*/");
      }
      else {
        if (check_next(value)) {
          move(value.size());
          return true;
        }
        else {
          string errmsg = string("find_panic(string)") + label_cur_line(cur_pos, std::string("which is expected to be ") + value);
          throw StyioSyntaxError(errmsg);
        }
      }
    }
  }

  /* Check isalpha or _ */
  bool check_isal_() {
    return has_char_index(cur_pos) && StyioUnicode::is_identifier_start(code[cur_pos]);
  }

  /* Check isalpha or isnum or _ */
  bool check_isalnum_() {
    return has_char_index(cur_pos) && StyioUnicode::is_identifier_continue(code[cur_pos]);
  }

  /* Check isdigit */
  bool check_isdigit() {
    return has_char_index(cur_pos) && StyioUnicode::is_digit(code[cur_pos]);
  }

  /* Tuple Operations */
  bool check_tuple_ops() {
    return check_next("<<")     // extract
           or check_next(">>")  // iterate
           or check_next("=>")  // next
      ;
  }

  /* Check Chain of Data Processing */
  bool check_codp() {
    return check_next("filter")
           or check_next("sort")
           or check_next("map")
           or check_next("slice")
           or check_next("print");
  }

  /* Check Binary Operator */
  bool check_binop() {
    if (!has_char_index(cur_pos)) {
      return false;
    }

    if (code[cur_pos] == '+' || code[cur_pos] == '-') {
      return true;
    }
    else if (code[cur_pos] == '*' || code[cur_pos] == '%') {
      return true;
    }
    else if (code[cur_pos] == '/') {
      /* Comments */
      if (check_ahead(1, '*') || check_ahead(1, '/')) {
        return false;
      }
      else {
        return true;
      }
    }
    else if (code[cur_pos] == '%') {
      return true;
    }

    return false;
  }

  std::tuple<bool, StyioOpType> get_binop_token() {
    if (!has_char_index(cur_pos)) {
      return {false, StyioOpType::Undefined};
    }

    switch (code[cur_pos]) {
      case '+': {
        return {true, StyioOpType::Binary_Add};
      } break;

      case '-': {
        return {true, StyioOpType::Binary_Sub};
      } break;

      case '*': {
        return {true, StyioOpType::Binary_Mul};
      } break;

      case '/': {
        if (check_ahead(1, '*')) {
          return {false, StyioOpType::Comment_MultiLine};
        }
        else if (check_ahead(1, '/')) {
          return {false, StyioOpType::Comment_SingleLine};
        }
        else {
          return {true, StyioOpType::Binary_Div};
        }
      } break;

      case '%': {
        return {true, StyioOpType::Binary_Mod};
      } break;

      default:
        break;
    }

    return {false, StyioOpType::Undefined};
  }

  void
  show_code_with_linenum() {
    for (size_t i = 0; i < line_seps.size(); i++) {
      std::string line = code.substr(line_seps.at(i).first, line_seps.at(i).second);

      std::regex newline_regex("\n");
      std::string replaced_text = std::regex_replace(line, newline_regex, "[NEWLINE]");

      std::cout
        << "|" << i << "|-[" << line_seps.at(i).first << ":" << (line_seps.at(i).first + line_seps.at(i).second) << "] "
        << line << std::endl;
    }
  }

  void show_ast(StyioAST* ast) {
    ensure_ast_repr();
    std::cout << ast->toString(ast_repr) << std::endl;
  }
};

template <typename Enumeration>
auto
type_to_int(Enumeration const value) ->
  typename std::underlying_type<Enumeration>::type {
  return static_cast<typename std::underlying_type<Enumeration>::type>(value);
}

/*
  =================
    Variable
  =================
*/

/*
  parse_id
*/
NameAST*
parse_name(StyioContext& context);

NameAST*
parse_name_unsafe(StyioContext& context);

/*
  =================
    Scalar Value
  =================
*/

/*
  parse_int
*/
IntAST*
parse_int(StyioContext& context);

/*
  parse_int_or_float
*/
StyioAST*
parse_int_or_float(StyioContext& context, char& cur_char);

/*
  parse_string
*/
StringAST*
parse_string(StyioContext& context);

/*
  parse_fmt_str
*/
FmtStrAST*
parse_fmt_str(StyioContext& context);

/*
  parse_path
*/
StyioAST*
parse_path(StyioContext& context);

/*
  parse_fill_arg
*/
ParamAST*
parse_argument(StyioContext& context);

StyioAST*
parse_list(StyioContext& context);

StyioAST*
parse_set(StyioContext& context);

StyioAST*
parse_struct(StyioContext& context);

/*
  =================
    Basic Operation
  =================
*/

/*
  parse_size_of
*/
SizeOfAST*
parse_size_of(StyioContext& context);

/*
  parse_call
*/
FuncCallAST*
parse_call(
  StyioContext& context,
  NameAST* func_name,
  StyioAST* callee = nullptr
);

AttrAST*
parse_attr(
  StyioContext& context
);

StyioAST*
parse_chain_of_call(
  StyioContext& context,
  StyioAST* callee
);

/*
  parse_bin_rhs
*/
StyioAST*
parse_binop_item(StyioContext& context);

/*
  parse_binop_rhs
*/
StyioAST*
parse_binop_rhs(StyioContext& context, StyioAST* lhs_ast, StyioOpType curr_tok);

/*
  parse_cond_item
*/
StyioAST*
parse_cond_item(StyioContext& context);

/*
  parse_cond: parse conditional expressions

  The following operators can be handled by parse_cond():
  >  Greater_Than
  <  Less_Than
  >= Greater_Than_Equal
  <= Less_Than_Equal
  == Eqaul
  != Not_Equal
  && Logic_AND
  ⊕ Logic_XOR
  || Logic_OR
*/
CondAST*
parse_cond(StyioContext& context);

/*
  parse_cond_flow
*/
StyioAST*
parse_cond_flow(StyioContext& context);

/*
  parse_list_op
*/
StyioAST*
parse_index_op(StyioContext& context, StyioAST* theList);

/*
  parse_var_tuple
*/
VarTupleAST*
parse_var_tuple(StyioContext& context);

/*
  parse_loop_or_iter
*/
StyioAST*
parse_loop_or_iter(StyioContext& context, StyioAST* collection);

/*
  parse_list_or_loop
*/
StyioAST*
parse_list_or_loop(StyioContext& context);

/*
  parse_loop
*/
StyioAST*
parse_loop(StyioContext& context, char& cur_char);

/*
  parse_simple_value
*/
StyioAST*
parse_value_expr(StyioContext& context);

/*
  parse_expr
*/
StyioAST*
parse_expr(StyioContext& context);

std::vector<std::string>
parse_name_with_spaces_unsafe(StyioContext& context);

StyioAST*
parse_var_name_or_value_expr(StyioContext& context);

/*
  parse_resources
*/
ResourceAST*
parse_resources(StyioContext& context);

ResourceAST*
parse_resources_after_at(StyioContext& context);

StyioAST*
parse_resource_file_atom_latest(StyioContext& context);

StyioAST*
parse_after_at_common(StyioContext& context, bool file_only_resource);

TypeAST*
parse_styio_type(StyioContext& context);

/*
  parse_pipeline
*/
StyioAST*
parse_hash_tag(StyioContext& context);

/*
  parse_read_file
*/
StyioAST*
parse_read_file(StyioContext& context, NameAST* id_ast);

/*
  parse_one_or_many_repr
*/
StyioAST*
parse_one_or_many_repr(StyioContext& context);

/*
  parse_print
*/
StyioAST*
parse_print(StyioContext& context);

/*
  parse_panic
*/
StyioAST*
parse_panic(StyioContext& context);

/*
  parse_stmt
*/
StyioAST*
parse_stmt_or_expr_legacy(StyioContext& context);

/*
  parse_ext_elem
*/
string
parse_ext_elem(StyioContext& context);

/*
  parse_ext_pack

  Dependencies should be written like a list of paths
  like this -> ["ab/c", "x/yz"]

  // 1. The dependencies should be parsed before any domain
  (statement/expression).
  // 2. The left square bracket `[` is only eliminated after entering this
  function (parse_ext_pack)
  |-- "[" <PATH>+ "]"

  If ? ( "the program starts with a left square bracket `[`" ),
  then -> {
    "parse_ext_pack() starts";
    "eliminate the left square bracket `[`";
    "parse dependency paths, which take comma `,` as delimeter";
    "eliminate the right square bracket `]`";
  }
  else :  {
    "parse_ext_pack() should NOT be invoked in this case";
    "if starts with left curly brace `{`, try parseSpace()";
    "otherwise, try parseScript()";
  }
*/
ExtPackAST*
parse_ext_pack(StyioContext& context);

std::vector<ParamAST*>
parse_params(StyioContext& context);

std::vector<StyioAST*>
parse_forward_as_list(StyioContext& context);

/*
  => { Code Block }
*/
BlockAST*
parse_block_only(StyioContext& context);

/*
  ?= Match Cases
*/
CasesAST*
parse_cases_only_latest(StyioContext& context);

StyioAST*
parse_at_stmt_or_expr_latest(StyioContext& context);

StyioAST*
parse_state_decl_after_at_latest(StyioContext& context);

/*
  >> Iterator
*/
StyioAST*
parse_iterator_only_latest(StyioContext& context, StyioAST* collection);

/*
  parse_something_with_forward
*/

BlockAST*
parse_block_with_forward(StyioContext& context);

CasesAST*
parse_cases_with_forward(StyioContext& context);

StyioAST*
parse_iterator_with_forward(StyioContext& context, StyioAST* collection);

BackwardAST*
parse_backward(StyioContext& context, bool is_func = false);

CODPAST*
parse_codp(StyioContext& context, CODPAST* prev_op = nullptr);

MainBlockAST*
parse_main_block_legacy(StyioContext& context);

bool
styio_parse_parser_engine_latest(const std::string& raw, StyioParserEngine& out);

const char*
styio_parser_engine_name_latest(StyioParserEngine engine);

MainBlockAST*
parse_main_block_with_engine_latest(
  StyioContext& context,
  StyioParserEngine engine,
  StyioParserRouteStats* route_stats = nullptr,
  StyioParseMode mode = StyioParseMode::Strict);

StyioAST*
parse_expr(StyioContext& context);

/*
  parse_var_name_or_value_expr
  - might be variable name
  - or something else after a variable name
*/
StyioAST*
parse_var_name_or_value_expr(
  StyioContext& context
);

StyioAST*
parse_tuple(
  StyioContext& context
);

StyioAST*
parse_tuple_no_braces(
  StyioContext& context,
  StyioAST* first_element = nullptr
);

/*
  parse_tuple_exprs
  - tuple
  - tuple operations
  - something else after tuple
*/
StyioAST*
parse_tuple_exprs(
  StyioContext& context
);

ExtractorAST*
parse_tuple_operations(
  StyioContext& context,
  TupleAST* the_tuple
);

/*
  parse_list_exprs
  - list
  - list operations
  - something else after list
*/
StyioAST*
parse_list_exprs_latest_draft(StyioContext& context);

ReturnAST*
parse_return(StyioContext& context);

#endif
