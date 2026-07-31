#pragma once
#ifndef STYIO_CALLABLE_SPECIALIZATION_GRAPH_HPP_
#define STYIO_CALLABLE_SPECIALIZATION_GRAPH_HPP_

#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace styio::sema {

class CallableSpecializationGraph
{
public:
  struct Limits
  {
    std::size_t maximum_expansion_depth = 64;
    std::size_t maximum_items = 4096;
  };

  CallableSpecializationGraph();
  explicit CallableSpecializationGraph(Limits limits);

  void reset();

  bool register_item(
    std::string content_digest,
    std::string display_name
  );

  bool begin_expansion(
    std::string_view content_digest
  );

  void end_expansion(
    std::string_view content_digest
  );

  std::size_t node_count() const {
    return nodes_.size();
  }

  std::size_t edge_count() const {
    return edge_count_;
  }

  bool has_edge(
    std::string_view from_digest,
    std::string_view to_digest
  ) const;

  bool is_root(std::string_view content_digest) const;

private:
  struct Node
  {
    std::string display_name;
    bool root = false;
    std::unordered_set<std::string> uses;
  };

  struct ActiveItem
  {
    std::string content_digest;
    std::string display_name;
  };

  std::string format_instance_path(
    std::string_view candidate = {}
  ) const;

  Limits limits_;
  std::unordered_map<std::string, Node> nodes_;
  std::vector<ActiveItem> active_path_;
  std::unordered_map<std::string, std::size_t> active_positions_;
  std::size_t edge_count_ = 0;
};

}  // namespace styio::sema

#endif  // STYIO_CALLABLE_SPECIALIZATION_GRAPH_HPP_
