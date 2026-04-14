#include "TreeSitterBackend.hpp"

#include <cstring>
#include <string>
#include <unordered_set>

#ifdef STYIO_HAS_TREE_SITTER
#include <tree_sitter/api.h>

extern "C" const TSLanguage* tree_sitter_styio(void);
#endif

namespace styio::ide {

namespace {

bool
contains_newline(const std::string& text, TextRange range) {
  if (range.end > text.size()) {
    range.end = text.size();
  }
  if (range.start > range.end) {
    return false;
  }
  return text.find('\n', range.start) != std::string::npos
    && text.find('\n', range.start) < range.end;
}

SyntaxNodeKind
kind_for_tree_sitter_node(const char* type, bool is_error) {
  if (is_error || std::strcmp(type, "ERROR") == 0) {
    return SyntaxNodeKind::Error;
  }
  if (std::strcmp(type, "source_file") == 0) {
    return SyntaxNodeKind::Root;
  }
  if (std::strcmp(type, "block") == 0) {
    return SyntaxNodeKind::Block;
  }
  if (std::strcmp(type, "param_list") == 0
      || std::strcmp(type, "arg_list") == 0
      || std::strcmp(type, "list_expr") == 0) {
    return SyntaxNodeKind::List;
  }
  if (std::strstr(type, "_stmt") != nullptr
      || std::strcmp(type, "function_decl") == 0
      || std::strcmp(type, "binding_stmt") == 0) {
    return SyntaxNodeKind::Statement;
  }
  return SyntaxNodeKind::Group;
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

#ifdef STYIO_HAS_TREE_SITTER

std::size_t
append_tree_sitter_node(
  TSNode node,
  const std::string& text,
  std::vector<SyntaxNode>& nodes,
  std::vector<Diagnostic>& diagnostics,
  std::unordered_set<std::string>& diag_seen,
  std::vector<FoldingRange>& folding_ranges
) {
  const char* type = ts_node_type(node);
  const bool is_error = ts_node_is_error(node) || ts_node_is_missing(node);
  const TextRange range{ts_node_start_byte(node), ts_node_end_byte(node)};

  const std::size_t index = nodes.size();
  nodes.push_back(SyntaxNode{
    kind_for_tree_sitter_node(type, is_error),
    type,
    range,
    {}});

  if (is_error) {
    std::string message = ts_node_is_missing(node) ? "missing syntax near " : "syntax error near ";
    message += type;
    append_unique_diagnostic(
      diagnostics,
      diag_seen,
      Diagnostic{range, DiagnosticSeverity::Error, "syntax", std::move(message)});
  }

  if ((nodes[index].kind == SyntaxNodeKind::Block
       || nodes[index].kind == SyntaxNodeKind::List
       || nodes[index].kind == SyntaxNodeKind::Group)
      && contains_newline(text, range)) {
    folding_ranges.push_back(FoldingRange{range});
  }

  const uint32_t child_count = ts_node_child_count(node);
  nodes[index].children.reserve(child_count);
  for (uint32_t child_index = 0; child_index < child_count; ++child_index) {
    const TSNode child = ts_node_child(node, child_index);
    if (ts_node_is_null(child)) {
      continue;
    }
    const std::size_t appended_index =
      append_tree_sitter_node(child, text, nodes, diagnostics, diag_seen, folding_ranges);
    nodes[index].children.push_back(appended_index);
  }

  return index;
}

#endif

}  // namespace

std::optional<TreeSitterParseResult>
parse_with_tree_sitter(const DocumentSnapshot& snapshot) {
#ifdef STYIO_HAS_TREE_SITTER
  TreeSitterParseResult result;

  TSParser* parser = ts_parser_new();
  if (parser == nullptr) {
    return std::nullopt;
  }

  const TSLanguage* language = tree_sitter_styio();
  if (language == nullptr || !ts_parser_set_language(parser, language)) {
    ts_parser_delete(parser);
    return std::nullopt;
  }

  TSTree* tree = ts_parser_parse_string(
    parser,
    nullptr,
    snapshot.buffer.text().c_str(),
    static_cast<uint32_t>(snapshot.buffer.text().size()));
  ts_parser_delete(parser);

  if (tree == nullptr) {
    return std::nullopt;
  }

  const TSNode root = ts_tree_root_node(tree);
  if (ts_node_is_null(root)) {
    ts_tree_delete(tree);
    return std::nullopt;
  }

  std::unordered_set<std::string> diag_seen;
  append_tree_sitter_node(
    root,
    snapshot.buffer.text(),
    result.nodes,
    result.diagnostics,
    diag_seen,
    result.folding_ranges);

  ts_tree_delete(tree);
  return result;
#else
  (void)snapshot;
  return std::nullopt;
#endif
}

}  // namespace styio::ide
