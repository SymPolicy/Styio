#include "CallableSpecializationGraph.hpp"

#include <sstream>
#include <utility>

#include "../StyioException/Exception.hpp"

namespace styio::sema {

CallableSpecializationGraph::CallableSpecializationGraph()
  : CallableSpecializationGraph(Limits{}) {}

CallableSpecializationGraph::CallableSpecializationGraph(
  Limits limits
) : limits_(limits) {
  if (limits_.maximum_expansion_depth == 0
      || limits_.maximum_items == 0) {
    throw StyioTypeError(
      "callable specialization limits must be greater than zero"
    );
  }
}

void
CallableSpecializationGraph::reset() {
  nodes_.clear();
  active_path_.clear();
  active_positions_.clear();
  edge_count_ = 0;
}

bool
CallableSpecializationGraph::register_item(
  std::string content_digest,
  std::string display_name
) {
  if (content_digest.empty() || display_name.empty()) {
    throw StyioTypeError(
      "callable specialization item requires a digest and display name"
    );
  }

  auto existing = nodes_.find(content_digest);
  if (existing != nodes_.end()) {
    if (existing->second.display_name != display_name) {
      throw StyioTypeError(
        "callable specialization content digest collision between `"
        + existing->second.display_name + "` and `" + display_name + "`"
      );
    }
    if (!active_path_.empty()) {
      auto& uses =
        nodes_.at(active_path_.back().content_digest).uses;
      if (uses.insert(content_digest).second) {
        ++edge_count_;
      }
    }
    else {
      existing->second.root = true;
    }
    return false;
  }

  if (nodes_.size() >= limits_.maximum_items) {
    throw StyioTypeError(
      "callable specialization growth ceiling of "
      + std::to_string(limits_.maximum_items)
      + " instance(s) exceeded; instance path: "
      + format_instance_path(display_name)
    );
  }

  Node node;
  node.display_name = std::move(display_name);
  node.root = active_path_.empty();
  auto [inserted, ok] =
    nodes_.emplace(std::move(content_digest), std::move(node));
  if (!ok) {
    throw StyioTypeError(
      "internal error: callable specialization item insertion failed"
    );
  }
  if (!active_path_.empty()) {
    auto& uses =
      nodes_.at(active_path_.back().content_digest).uses;
    if (uses.insert(inserted->first).second) {
      ++edge_count_;
    }
  }
  return true;
}

bool
CallableSpecializationGraph::begin_expansion(
  std::string_view content_digest
) {
  const auto node = nodes_.find(std::string(content_digest));
  if (node == nodes_.end()) {
    throw StyioTypeError(
      "internal error: unregistered callable specialization expansion"
    );
  }
  if (active_positions_.count(node->first) != 0) {
    return false;
  }
  if (active_path_.size() >= limits_.maximum_expansion_depth) {
    throw StyioTypeError(
      "callable specialization recursion ceiling of "
      + std::to_string(limits_.maximum_expansion_depth)
      + " instance(s) exceeded; instance path: "
      + format_instance_path(node->second.display_name)
    );
  }

  active_positions_[node->first] = active_path_.size();
  active_path_.push_back(
    ActiveItem{node->first, node->second.display_name});
  return true;
}

void
CallableSpecializationGraph::end_expansion(
  std::string_view content_digest
) {
  if (active_path_.empty()
      || active_path_.back().content_digest != content_digest) {
    throw StyioTypeError(
      "internal error: callable specialization expansion stack mismatch"
    );
  }
  active_positions_.erase(active_path_.back().content_digest);
  active_path_.pop_back();
}

bool
CallableSpecializationGraph::has_edge(
  std::string_view from_digest,
  std::string_view to_digest
) const {
  const auto from = nodes_.find(std::string(from_digest));
  return from != nodes_.end()
         && from->second.uses.count(std::string(to_digest)) != 0;
}

bool
CallableSpecializationGraph::is_root(
  std::string_view content_digest
) const {
  const auto node = nodes_.find(std::string(content_digest));
  return node != nodes_.end() && node->second.root;
}

std::string
CallableSpecializationGraph::format_instance_path(
  std::string_view candidate
) const {
  std::ostringstream output;
  for (std::size_t i = 0; i < active_path_.size(); ++i) {
    if (i != 0) {
      output << " -> ";
    }
    output << active_path_[i].display_name;
  }
  if (!candidate.empty()) {
    if (!active_path_.empty()) {
      output << " -> ";
    }
    output << candidate;
  }
  return output.str();
}

}  // namespace styio::sema
