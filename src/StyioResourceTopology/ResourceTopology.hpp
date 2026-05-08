#pragma once
#ifndef STYIO_RESOURCE_TOPOLOGY_HPP_
#define STYIO_RESOURCE_TOPOLOGY_HPP_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

class BlockAST;
class MainBlockAST;
class StyioAST;

namespace styio::resource_topology {

enum class Capability : std::uint32_t
{
  None = 0,
  Pull = 1u << 0,
  Iter = 1u << 1,
  Push = 1u << 2,
  Close = 1u << 3,
  Clone = 1u << 4,
  Checkpoint = 1u << 5,
  Task = 1u << 6,
  StateRead = 1u << 7,
  StateWrite = 1u << 8,
};

inline Capability operator|(Capability lhs, Capability rhs) {
  return static_cast<Capability>(
    static_cast<std::uint32_t>(lhs) | static_cast<std::uint32_t>(rhs));
}

inline Capability operator&(Capability lhs, Capability rhs) {
  return static_cast<Capability>(
    static_cast<std::uint32_t>(lhs) & static_cast<std::uint32_t>(rhs));
}

inline Capability& operator|=(Capability& lhs, Capability rhs) {
  lhs = lhs | rhs;
  return lhs;
}

inline bool has_capability(Capability caps, Capability cap) {
  return (static_cast<std::uint32_t>(caps & cap) != 0);
}

enum class NodeKind : std::uint8_t
{
  Program,
  DriverSource,
  Handle,
  StreamOp,
  StateSlot,
  HiddenLedger,
  Sink,
  Task,
  FailureDomain,
  Value,
};

enum class EdgeKind : std::uint8_t
{
  Flow,
  Intent,
  Ownership,
  Borrow,
  Mutation,
  Backpressure,
  Commit,
  Failure,
  Placement,
};

enum class TypeState : std::uint8_t
{
  Unknown,
  Declared,
  Open,
  Eof,
  Closed,
  Materialized,
  Ready,
};

struct Node
{
  std::size_t id = 0;
  NodeKind kind = NodeKind::Value;
  std::string label;
  Capability capabilities = Capability::None;
  TypeState state = TypeState::Unknown;
  const StyioAST* source = nullptr;
};

struct Edge
{
  EdgeKind kind = EdgeKind::Flow;
  std::size_t from = 0;
  std::size_t to = 0;
  std::string label;
};

class Graph
{
  std::vector<Node> nodes_;
  std::vector<Edge> edges_;

public:
  std::size_t add_node(
    NodeKind kind,
    std::string label,
    Capability capabilities,
    TypeState state,
    const StyioAST* source = nullptr);

  void add_edge(EdgeKind kind, std::size_t from, std::size_t to, std::string label = "");

  const std::vector<Node>& nodes() const {
    return nodes_;
  }

  const std::vector<Edge>& edges() const {
    return edges_;
  }

  const Node& node(std::size_t id) const;

  std::size_t node_count(NodeKind kind) const;
  std::size_t edge_count(EdgeKind kind) const;
  std::string debug_string() const;
};

struct ValidationError
{
  std::string message;
  const StyioAST* source = nullptr;
};

struct ValidationReport
{
  std::vector<ValidationError> errors;

  bool ok() const {
    return errors.empty();
  }

  std::string message() const;
};

struct BuildOptions
{
  std::string phase = "resource-topology";
  bool require_close_owner = true;
  bool validate_series_scope = true;
};

struct BuildResult
{
  Graph graph;
  ValidationReport report;
};

BuildResult build(MainBlockAST* ast, BuildOptions options = {});
BuildResult build(BlockAST* ast, BuildOptions options = {});

void validate_or_throw(MainBlockAST* ast, std::string phase);
void validate_or_throw(BlockAST* ast, std::string phase);

std::string to_string(NodeKind kind);
std::string to_string(EdgeKind kind);
std::string to_string(TypeState state);
std::string to_string(Capability capabilities);

} // namespace styio::resource_topology

#endif // STYIO_RESOURCE_TOPOLOGY_HPP_
