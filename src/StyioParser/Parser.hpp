#pragma once
#ifndef STYIO_PARSER_H_
#define STYIO_PARSER_H_

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
  New,
};

class StyioContext
{
private:
  static StyioToken* eof_fallback_token() {
    static StyioToken* tok = StyioToken::Create(StyioTokenType::TOK_EOF, "EOF");
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

  bool debug_mode = false;

  std::vector<std::vector<std::pair<size_t, size_t>>> token_segmentation; /* offset, length */
  std::vector<std::pair<size_t, size_t>> token_coordinates;               /* row, col */
  std::vector<std::string> token_lines;                                   /* lines */

  StyioRepr* ast_repr = new StyioRepr();

  void initialize_token_coordinates_and_segmentations() {
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
    initialize_token_coordinates_and_segmentations();
    initialize_token_lines();
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

    for (size_t i = 0; i < steps; i++) {
      if (index_of_token >= tokens.size()) {
        index_of_token = tokens.size();
        return;
      }
      this->cur_pos += tokens[index_of_token]->length();
      this->index_of_token += 1;
    }
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

  inline void skip() {
    while (styio_is_trivia_token(cur_tok()->type)) {
      this->move_forward(1, "skip");
    }
  }

  /** Spaces only — do not cross newlines (so `b` and `(` on different lines are not a call). */
  inline void skip_spaces_no_linebreak() {
    while (cur_tok()->type == StyioTokenType::TOK_SPACE) {
      this->move_forward(1, "skip_space_inline");
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
    return styio_try_check_non_trivia(tokens, index_of_token, target);
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
      move_forward(1, "try_match");
      return true;
    }

    size_t offset = 0;
    while (index_of_token + offset < tokens.size()) {
      switch (tokens.at(index_of_token + offset)->type) {
        /* white spaces */
        case StyioTokenType::TOK_SPACE: {
          offset += 1;
        } break;

        /* \n */
        case StyioTokenType::TOK_LF: {
          offset += 1;
        } break;

        /* \r */
        case StyioTokenType::TOK_CR: {
          offset += 1;
        } break;

        // comments like this
        case StyioTokenType::COMMENT_LINE: {
          offset += 1;
        } break;

        /* comments like this */
        case StyioTokenType::COMMENT_CLOSED: {
          offset += 1;
        } break;

        default: {
          if (tokens.at(index_of_token + offset)->type == target) {
            move_forward(offset + 1, "try_match");
            return true;
          }
          else {
            return false;
          }
        } break;
      }
    }

    return false;
  }

  bool try_match_panic(StyioTokenType target, std::string errmsg = "") {
    // just match
    if (index_of_token < tokens.size() && tokens[index_of_token]->type == target) {
      move_forward(1, "try_match_panic");
      return true;
    }

    // skip spaces and match token
    size_t offset = 0;
    while (index_of_token + offset < tokens.size()) {
      switch (tokens.at(index_of_token + offset)->type) {
        /* white spaces */
        case StyioTokenType::TOK_SPACE: {
          offset += 1;
        } break;

        /* \n */
        case StyioTokenType::TOK_LF: {
          offset += 1;
        } break;

        /* \r */
        case StyioTokenType::TOK_CR: {
          offset += 1;
        } break;

        // comments like this
        case StyioTokenType::COMMENT_LINE: {
          offset += 1;
        } break;

        /* comments like this */
        case StyioTokenType::COMMENT_CLOSED: {
          offset += 1;
        } break;

        default: {
          if (tokens.at(index_of_token + offset)->type == target) {
            move_forward(offset + 1, "try_match_panic");
            return true;
          }
          else {
            if (errmsg.empty()) {
              throw StyioSyntaxError(
                string("try_match_panic(token)")
                + label_cur_line(
                  cur_pos,
                  std::string("which is expected to be ") + StyioToken::getTokName(target)
                )
              );
            }
            else {
              throw StyioSyntaxError(label_cur_line(cur_pos, errmsg));
            }
          }
        } break;
      }
    }

    throw StyioParseError(label_cur_line(
      cur_pos,
      "try_match_panic(token): Couldn't find " + StyioToken::getTokName(target) + " until the end of the file."
    ));
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

  /*
    usage:
    1. the current position is after a known operator
    like: a + b
             ^     right here, after +, the current position is a white space
    2. there is a variable or a value behind the current position
    like: 1 + 2
             ^     this space is followed by the value of 2
    3. the expected operator is behind that variable or value
    like: 1 + 2 * 3
             ^     curr_pos is a white space, the expected operator is *, which is behind 2.
  */
  string peak_operator(int num = 1) {
    if (num <= 0) {
      return "EOF";
    }

    size_t tmp_pos = cur_pos;
    for (int i = 0; i < num; i++) {
      while (tmp_pos < code.size()) {
        if (StyioUnicode::is_space(code[tmp_pos])) {
          tmp_pos += 1;
          continue;
        }

        if (tmp_pos + 1 < code.size() && code.compare(tmp_pos, 2, "//") == 0) {
          tmp_pos += 2;
          while (tmp_pos < code.size() && code[tmp_pos] != '\n') {
            tmp_pos += 1;
          }
          if (tmp_pos < code.size()) {
            tmp_pos += 1;
          }
          continue;
        }

        if (tmp_pos + 1 < code.size() && code.compare(tmp_pos, 2, "/*") == 0) {
          tmp_pos += 2;
          while (tmp_pos + 1 < code.size() && code.compare(tmp_pos, 2, "*/") != 0) {
            tmp_pos += 1;
          }
          if (tmp_pos + 1 < code.size() && code.compare(tmp_pos, 2, "*/") == 0) {
            tmp_pos += 2;
            continue;
          }
          return "EOF";
        }

        if (StyioUnicode::is_ascii_alnum(code[tmp_pos]) || code[tmp_pos] == '_') {
          tmp_pos += 1;
          continue;
        }

        break;
      }

      if (tmp_pos >= code.size()) {
        return "EOF";
      }

      size_t op_start = tmp_pos;
      while (tmp_pos < code.size()) {
        if (StyioUnicode::is_space(code[tmp_pos])) {
          break;
        }
        if (tmp_pos + 1 < code.size() && (code.compare(tmp_pos, 2, "/*") == 0 || code.compare(tmp_pos, 2, "//") == 0)) {
          break;
        }
        if (StyioUnicode::is_ascii_alnum(code[tmp_pos]) || code[tmp_pos] == '_') {
          break;
        }
        tmp_pos += 1;
      }

      if (op_start == tmp_pos) {
        return "EOF";
      }

      if (i == num - 1) {
        return code.substr(op_start, tmp_pos - op_start);
      }
    }

    return "EOF";
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
BinOpAST*
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

StyioAST*
parse_var_name_or_value_expr(StyioContext& context);

/*
  parse_resources
*/
ResourceAST*
parse_resources(StyioContext& context);

ResourceAST*
parse_resources_after_at(StyioContext& context);

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
parse_stmt_or_expr(StyioContext& context);

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
parse_cases_only(StyioContext& context);

/*
  >> Iterator
*/
StyioAST*
parse_iterator_only(StyioContext& context, StyioAST* collection);

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
parse_main_block(StyioContext& context);

bool
styio_parse_parser_engine(const std::string& raw, StyioParserEngine& out);

const char*
styio_parser_engine_name(StyioParserEngine engine);

MainBlockAST*
parse_main_block_with_engine(StyioContext& context, StyioParserEngine engine);

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
parse_list_exprs(StyioContext& context);

ReturnAST*
parse_return(StyioContext& context);

#endif
