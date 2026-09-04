#include "ResourceTopology.hpp"

#include <algorithm>
#include <deque>
#include <functional>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

#include "../StyioAST/AST.hpp"
#include "../StyioException/Exception.hpp"
#include "../StyioUtil/BuiltinMethods.hpp"
#include "../StyioUtil/ResourceNames.hpp"

namespace styio::resource_topology {
namespace {

constexpr std::size_t kNoNode = std::numeric_limits<std::size_t>::max();

StyioDataType
undefined_type() {
  return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
}

Capability
capabilities_from_type(const StyioDataType& type) {
  Capability caps = Capability::None;
  if (styio_type_has_capability(type, StyioTypeCapability::Readable)
      || styio_type_has_capability(type, StyioTypeCapability::Pull)) {
    caps |= Capability::Pull;
  }
  if (styio_type_has_capability(type, StyioTypeCapability::Iterable)) {
    caps |= Capability::Iter;
  }
  if (styio_type_has_capability(type, StyioTypeCapability::Writable)
      || styio_type_has_capability(type, StyioTypeCapability::Push)) {
    caps |= Capability::Push;
  }
  if (styio_type_has_capability(type, StyioTypeCapability::Close)) {
    caps |= Capability::Close;
  }
  if (styio_type_has_capability(type, StyioTypeCapability::Cloneable)) {
    caps |= Capability::Clone;
  }
  if (type.handle_family == StyioHandleFamily::Task) {
    caps |= Capability::Task;
  }
  if (type.handle_family == StyioHandleFamily::File) {
    caps |= Capability::Pull | Capability::Iter | Capability::Push | Capability::Close;
  }
  if (styio_is_topology_resource_type(type)) {
    caps |= Capability::Pull | Capability::Push | Capability::Iter | Capability::Checkpoint;
  }
  return caps;
}

TypeState
state_from_type(const StyioDataType& type) {
  switch (type.state) {
    case StyioTypeState::Open:
      return TypeState::Open;
    case StyioTypeState::Materialized:
      return TypeState::Materialized;
    case StyioTypeState::Ready:
    case StyioTypeState::Done:
      return TypeState::Ready;
    case StyioTypeState::Closed:
      return TypeState::Closed;
    default:
      return TypeState::Unknown;
  }
}

std::string
std_stream_label(StdStreamKind kind) {
  return styio_std_stream_resource_label(kind);
}

std::string
file_label(FileResourceAST* ast) {
  if (ast == nullptr || ast->getPath() == nullptr) {
    return "@file(<null>)";
  }
  if (auto* s = dynamic_cast<StringAST*>(ast->getPath())) {
    return ast->isAutoDetect()
      ? std::string("@{") + s->getValue() + "}"
      : std::string("@file(") + s->getValue() + ")";
  }
  return ast->isAutoDetect() ? "@{<expr>}" : "@file(<expr>)";
}

std::string
state_slot_label(StateDeclAST* ast) {
  if (ast == nullptr) {
    return "state:<null>";
  }
  if (ast->getExportVar() != nullptr) {
    return std::string("state:") + ast->getExportVar()->getNameAsStr();
  }
  if (ast->getAccName() != nullptr) {
    return std::string("state:") + ast->getAccName()->getAsStr();
  }
  return "state:<anonymous>";
}

bool
int_ast_positive(IntAST* ast) {
  if (ast == nullptr) {
    return false;
  }
  try {
    return std::stoll(ast->getValue()) > 0;
  } catch (...) {
    return false;
  }
}

bool
resource_free_scalar_expression(StyioAST* ast) {
  if (ast == nullptr) {
    return false;
  }
  switch (ast->getNodeType()) {
    case StyioNodeType::Integer:
    case StyioNodeType::Float:
    case StyioNodeType::Bool:
    case StyioNodeType::Char:
    case StyioNodeType::String:
      return true;
    case StyioNodeType::Id: {
      const StyioDataType type = static_cast<NameAST*>(ast)->getDataType();
      return !styio_is_topology_resource_type(type)
        && !styio_type_is_resource_handle(type)
        && !styio_is_callable_type(type);
    }
    case StyioNodeType::BinOp: {
      auto* binop = static_cast<BinOpAST*>(ast);
      return resource_free_scalar_expression(binop->getLHS())
        && resource_free_scalar_expression(binop->getRHS());
    }
    default:
      return false;
  }
}

class Builder
{
  struct OwnerPath
  {
    std::size_t parent = kNoNode;
    std::string component;
  };

  struct AstSiteKey
  {
    const StyioAST* ast = nullptr;
    std::string subsite;

    friend bool operator==(const AstSiteKey&, const AstSiteKey&) = default;
  };

  struct AstSiteKeyHash
  {
    std::size_t operator()(const AstSiteKey& key) const noexcept {
      std::size_t value = std::hash<const StyioAST*>{}(key.ast);
      value ^= std::hash<std::string>{}(key.subsite)
        + 0x9e3779b9u + (value << 6u) + (value >> 2u);
      return value;
    }
  };

  struct SiblingKey
  {
    std::size_t owner_path = 0;
    SemanticRole role = SemanticRole::Value;
    std::string signature;

    friend bool operator==(const SiblingKey&, const SiblingKey&) = default;
  };

  struct SiblingKeyHash
  {
    std::size_t operator()(const SiblingKey& key) const noexcept {
      std::size_t value = std::hash<std::size_t>{}(key.owner_path);
      value ^= std::hash<unsigned>{}(static_cast<unsigned>(key.role))
        + 0x9e3779b9u + (value << 6u) + (value >> 2u);
      value ^= std::hash<std::string>{}(key.signature)
        + 0x9e3779b9u + (value << 6u) + (value >> 2u);
      return value;
    }
  };

  struct NamedOwnerKey
  {
    std::size_t parent_path = 0;
    std::string component;

    friend bool operator==(const NamedOwnerKey&, const NamedOwnerKey&) = default;
  };

  struct NamedOwnerKeyHash
  {
    std::size_t operator()(const NamedOwnerKey& key) const noexcept {
      std::size_t value = std::hash<std::size_t>{}(key.parent_path);
      value ^= std::hash<std::string>{}(key.component)
        + 0x9e3779b9u + (value << 6u) + (value >> 2u);
      return value;
    }
  };

  BuildOptions options_;
  BuildResult result_;
  std::unordered_map<AstSiteKey, std::size_t, AstSiteKeyHash> ast_nodes_;
  std::vector<OwnerPath> owner_paths_;
  std::vector<std::size_t> node_owner_paths_;
  std::unordered_map<SiblingKey, std::size_t, SiblingKeyHash> sibling_occurrences_;
  std::unordered_map<NamedOwnerKey, std::size_t, NamedOwnerKeyHash>
    named_owner_occurrences_;
  styio::semantic_identity::CollisionGuard identity_guard_;
  std::unordered_map<std::string, std::size_t> binding_nodes_;
  std::unordered_map<std::string, StyioDataType> binding_types_;
  std::unordered_map<std::string, std::size_t> handle_bindings_;
  std::unordered_map<std::string, std::size_t> resource_nodes_;
  std::unordered_map<std::string, std::size_t> state_slots_;
  std::unordered_map<std::string, std::size_t> task_bindings_;
  std::unordered_map<std::string, std::size_t> block_bindings_;
  struct ResourceMethodInfo
  {
    bool consuming = false;
    bool property = false;
    std::size_t param_count = 0;
  };
  std::unordered_map<std::string, std::unordered_map<std::string, ResourceMethodInfo>> resource_methods_;
  std::unordered_set<std::string> consumed_tasks_;
  std::unordered_set<std::size_t> destroyed_resources_;
  std::unordered_set<std::size_t> unordered_execution_nodes_;

  struct ResourceAccess
  {
    std::size_t resource = kNoNode;
    std::size_t owner = kNoNode;
    bool exclusive = false;
    const StyioAST* source = nullptr;
    std::string label;
  };
  std::vector<ResourceAccess> resource_accesses_;

  struct Context
  {
    std::size_t owner = kNoNode;
    bool in_state_decl = false;
    bool top_level_resource_scope = false;
    std::size_t semantic_owner_path = kNoNode;
    std::string semantic_relation;
  };

public:
  explicit Builder(BuildOptions options) :
      options_(std::move(options)) {
    owner_paths_.push_back(OwnerPath{kNoNode, "root"});
    for (const auto& method : styio_builtin_resource_methods_latest()) {
      resource_methods_[method.family][method.method] = ResourceMethodInfo{
        method.consuming,
        method.property,
        method.param_count,
      };
    }
  }

  BuildResult build(MainBlockAST* ast) {
    const std::size_t program = add_semantic_node(
      NodeKind::Program,
      SemanticRole::Program,
      "program",
      Capability::None,
      TypeState::Ready,
      ast,
      0,
      "program");
    if (ast == nullptr) {
      error("main block is null", nullptr);
      return std::move(result_);
    }
    Context ctx{program, false, true};
    for (auto* stmt : ast->getStmts()) {
      visit(stmt, related_context(ctx, "statement"));
    }
    finalize();
    return std::move(result_);
  }

  BuildResult build(BlockAST* ast) {
    const std::size_t block = add_semantic_node(
      NodeKind::StreamOp,
      SemanticRole::StandaloneBlock,
      "block",
      Capability::None,
      TypeState::Ready,
      ast,
      0,
      "standalone-block");
    if (ast == nullptr) {
      error("block is null", nullptr);
      return std::move(result_);
    }
    Context ctx{block, false, false};
    for (auto* stmt : ast->stmts) {
      visit(stmt, related_context(ctx, "statement"));
    }
    finalize();
    return std::move(result_);
  }

private:
  std::size_t append_owner_path(std::size_t parent, std::string component) {
    owner_paths_.push_back(OwnerPath{parent, std::move(component)});
    return owner_paths_.size() - 1;
  }

  std::size_t node_owner_path(std::size_t node) const {
    return node < node_owner_paths_.size() ? node_owner_paths_[node] : 0;
  }

  std::size_t semantic_owner_path(const Context& ctx) const {
    if (ctx.semantic_owner_path != kNoNode) {
      return ctx.semantic_owner_path;
    }
    return ctx.owner == kNoNode ? 0 : node_owner_path(ctx.owner);
  }

  std::vector<std::string> owner_components(std::size_t path) const {
    std::vector<std::string> reversed;
    while (path != kNoNode) {
      reversed.push_back(owner_paths_[path].component);
      path = owner_paths_[path].parent;
    }
    std::reverse(reversed.begin(), reversed.end());
    return reversed;
  }

  Context related_context(Context ctx, std::string relation) const {
    ctx.semantic_relation = std::move(relation);
    return ctx;
  }

  Context child_context(
    const Context& ctx,
    std::size_t owner,
    std::string relation
  ) const {
    return Context{
      owner,
      ctx.in_state_decl,
      false,
      kNoNode,
      std::move(relation)};
  }

  Context detached_context(const Context& ctx, std::string relation) const {
    return Context{
      kNoNode,
      ctx.in_state_decl,
      false,
      semantic_owner_path(ctx),
      std::move(relation)};
  }

  Context named_owner_context(
    const Context& ctx,
    std::string owner_component,
    std::string relation
  ) {
    const std::size_t parent_path = semantic_owner_path(ctx);
    const std::size_t occurrence = named_owner_occurrences_[NamedOwnerKey{
      parent_path,
      owner_component}]++;
    const std::size_t named_path = append_owner_path(
      parent_path,
      std::move(owner_component));
    return Context{
      ctx.owner,
      ctx.in_state_decl,
      false,
      append_owner_path(
        named_path,
        "occurrence:" + std::to_string(occurrence)),
      std::move(relation)};
  }

  static SemanticRole role_for(StyioAST* ast, NodeKind kind) {
    if (dynamic_cast<ResourceMethodDefAST*>(ast) != nullptr) return SemanticRole::ResourceMethod;
    if (dynamic_cast<ResourceDeclAST*>(ast) != nullptr) return SemanticRole::ResourceSlot;
    if (dynamic_cast<SnapshotDeclAST*>(ast) != nullptr) return SemanticRole::Snapshot;
    if (dynamic_cast<StateDeclAST*>(ast) != nullptr) return SemanticRole::StateSlot;
    if (dynamic_cast<SeriesIntrinsicAST*>(ast) != nullptr) return SemanticRole::SeriesLedger;
    switch (kind) {
      case NodeKind::Program: return SemanticRole::Program;
      case NodeKind::DriverSource: return SemanticRole::DriverSource;
      case NodeKind::Handle: return SemanticRole::ResourceHandle;
      case NodeKind::StreamOp: return SemanticRole::StreamOperation;
      case NodeKind::StateSlot: return SemanticRole::StateSlot;
      case NodeKind::HiddenLedger: return SemanticRole::SeriesLedger;
      case NodeKind::Sink: return SemanticRole::Sink;
      case NodeKind::Task: return SemanticRole::Task;
      case NodeKind::FailureDomain: return SemanticRole::TaskFailureDomain;
      case NodeKind::Value: return SemanticRole::Value;
    }
    return SemanticRole::Value;
  }

  static std::string binding_name_for(StyioAST* ast) {
    if (auto* name = dynamic_cast<NameAST*>(ast)) {
      return name->getAsStr();
    }
    if (auto* var = dynamic_cast<VarAST*>(ast)) {
      return var->getNameAsStr();
    }
    return {};
  }

  std::string ast_signature(StyioAST* ast, NodeKind kind, std::string_view subsite) {
    std::ostringstream out;
    out << "ast:" << (ast == nullptr ? -1 : static_cast<int>(ast->getNodeType()))
        << ":kind:" << static_cast<int>(kind);
    if (!subsite.empty()) out << ":subsite:" << subsite;
    if (auto* name = dynamic_cast<NameAST*>(ast)) {
      out << ":name:" << name->getAsStr();
    } else if (auto* var = dynamic_cast<VarAST*>(ast)) {
      out << ":binding:" << var->getNameAsStr();
    } else if (auto* function = dynamic_cast<FunctionAST*>(ast)) {
      out << ":function:" << function->getNameAsStr();
    } else if (auto* function = dynamic_cast<SimpleFuncAST*>(ast)) {
      if (function->func_name != nullptr) {
        out << ":function:" << function->func_name->getAsStr();
      }
    } else if (auto* stream = dynamic_cast<StdStreamAST*>(ast)) {
      out << ":stream:" << static_cast<int>(stream->getStreamKind());
    } else if (auto* receiver = dynamic_cast<ResourceReceiverAST*>(ast)) {
      out << ":family:" << receiver->getFamilyName();
    } else if (auto* method = dynamic_cast<ResourceMethodDefAST*>(ast)) {
      out << ":method:" << method->getFamilyName() << ':' << method->getMethodName();
    } else if (auto* ref = dynamic_cast<ResourceRefAST*>(ast)) {
      out << ":resource:" << ref->getNameStr()
          << ":selector:" << static_cast<int>(ref->getSelectorKind());
    } else if (auto* acquire = dynamic_cast<HandleAcquireAST*>(ast)) {
      out << ":binding:" << acquire->getVar()->getNameAsStr();
    } else if (auto* state = dynamic_cast<StateDeclAST*>(ast)) {
      if (state->getExportVar() != nullptr) out << ":state:" << state->getExportVar()->getNameAsStr();
      if (state->getAccName() != nullptr) out << ":acc:" << state->getAccName()->getAsStr();
    } else if (auto* snap = dynamic_cast<SnapshotDeclAST*>(ast)) {
      out << ":snapshot:" << snap->getVar()->getAsStr();
    } else if (auto* flow = dynamic_cast<FlowBindAST*>(ast)) {
      out << ":flow:" << flow->getTargetNameAsStr();
    } else if (auto* flex = dynamic_cast<FlexBindAST*>(ast)) {
      out << ":binding:" << flex->getNameAsStr();
    } else if (auto* final_bind = dynamic_cast<FinalBindAST*>(ast)) {
      out << ":binding:" << final_bind->getName();
    } else if (auto* call = dynamic_cast<FuncCallAST*>(ast)) {
      out << ":call:" << call->getNameAsStr();
      if (call->func_callee != nullptr) {
        out << ":callee-family:" << resource_family_for_expr(call->func_callee);
      }
    } else if (auto* bin = dynamic_cast<BinOpAST*>(ast)) {
      out << ":operator:" << static_cast<int>(bin->getOp());
    } else if (auto* comparison = dynamic_cast<BinCompAST*>(ast)) {
      out << ":operator:" << static_cast<int>(comparison->getSign());
    } else if (auto* condition = dynamic_cast<CondAST*>(ast)) {
      out << ":operator:" << static_cast<int>(condition->getSign());
    } else if (auto* attribute = dynamic_cast<AttrAST*>(ast)) {
      if (auto* attribute_name = dynamic_cast<NameAST*>(attribute->attr)) {
        out << ":attribute:" << attribute_name->getAsStr();
      }
    } else if (auto* series = dynamic_cast<SeriesIntrinsicAST*>(ast)) {
      out << ":series:" << static_cast<int>(series->getOp());
    }
    const StyioDataType type = type_hint(ast);
    if (!type.isUndefined()) out << ":type:" << type.name;
    return out.str();
  }

  std::size_t add_semantic_node(
    NodeKind kind,
    SemanticRole role,
    std::string label,
    Capability caps,
    TypeState state,
    const StyioAST* source,
    std::size_t owner_path,
    std::string signature
  ) {
    std::vector<std::string> owners = owner_components(owner_path);
    const std::size_t occurrence = sibling_occurrences_[SiblingKey{
      owner_path,
      role,
      signature}]++;
    const std::vector<std::string> discriminators{
      std::move(signature),
      "occurrence:" + std::to_string(occurrence)};
    const auto semantic_id = identity_guard_.derive_and_record(
      options_.identity_scope,
      owners,
      to_string(role),
      discriminators);
    const std::size_t id = result_.graph.add_node(
      kind, role, semantic_id, std::move(label), caps, state, source);
    node_owner_paths_.push_back(append_owner_path(
      owner_path,
      to_string(role) + ":" + discriminators.front()
        + ":" + discriminators.back()));
    return id;
  }

  void error(std::string msg, const StyioAST* source) {
    result_.report.errors.push_back(ValidationError{
      options_.phase + ": " + std::move(msg),
      source});
  }

  std::size_t add_ast_node(
    StyioAST* ast,
    NodeKind kind,
    std::string label,
    Capability caps,
    TypeState state,
    Context ctx,
    std::string subsite = ""
  ) {
    const std::string memo_subsite = subsite;
    const std::string semantic_subsite = subsite.empty()
      ? ctx.semantic_relation
      : subsite;
    if (ast != nullptr) {
      const auto found = ast_nodes_.find(AstSiteKey{ast, memo_subsite});
      if (found != ast_nodes_.end()) return found->second;
    }
    const SemanticRole role = role_for(ast, kind);
    const std::string signature = ast_signature(ast, kind, semantic_subsite);
    const std::size_t id = add_semantic_node(
      kind,
      role,
      std::move(label),
      caps,
      state,
      ast,
      semantic_owner_path(ctx),
      signature);
    if (ast != nullptr) {
      ast_nodes_.emplace(AstSiteKey{ast, memo_subsite}, id);
    }
    if (ctx.owner != kNoNode && id != ctx.owner) {
      result_.graph.add_edge(EdgeKind::Ownership, ctx.owner, id);
    }
    return id;
  }

  StyioDataType type_hint(StyioAST* ast) const {
    if (ast == nullptr) {
      return undefined_type();
    }

    if (auto* name = dynamic_cast<NameAST*>(ast)) {
      auto hit = binding_types_.find(name->getAsStr());
      if (hit != binding_types_.end()) {
        return hit->second;
      }
      return ast->getDataType();
    }

    if (auto* attr = dynamic_cast<AttrAST*>(ast)) {
      auto* attr_name = dynamic_cast<NameAST*>(attr->attr);
      if (attr_name == nullptr) {
        return undefined_type();
      }
      const StyioDataType base = type_hint(attr->body);
      if ((attr_name->getAsStr() == "length" || attr_name->getAsStr() == "size")
          && styio_type_is_sized(base)) {
        return StyioDataType{StyioDataTypeOption::Integer, "i64", 64};
      }
      if (attr_name->getAsStr() == "keys" && styio_is_dict_type(base)) {
        return styio_make_list_type(styio_dict_key_type_name(base));
      }
      if (attr_name->getAsStr() == "values" && styio_is_dict_type(base)) {
        return styio_make_list_type(styio_dict_value_type_name(base));
      }
      return ast->getDataType();
    }

    if (auto* access = dynamic_cast<ListOpAST*>(ast)) {
      const StyioDataType base = type_hint(access->getList());
      if (access->getOp() == StyioNodeType::Access_By_Name && styio_is_dict_type(base)) {
        return styio_data_type_from_name(styio_dict_value_type_name(base));
      }
      if (access->getOp() == StyioNodeType::Access_By_Index
          && (styio_is_list_type(base) || styio_is_dict_type(base) || styio_is_matrix_type(base))) {
        return styio_data_type_from_name(styio_type_item_type_name(base));
      }
      if (access->getOp() == StyioNodeType::Access_By_Slice && styio_is_list_type(base)) {
        return base;
      }
      if (access->getOp() == StyioNodeType::Access_By_Slice && styio_is_matrix_type(base)) {
        return styio_make_list_type(styio_type_item_type_name(base));
      }
      if (access->getOp() == StyioNodeType::Access_By_Slice && styio_is_dict_type(base)) {
        return styio_make_list_type(styio_dict_value_type_name(base));
      }
      return ast->getDataType();
    }

    if (auto* call = dynamic_cast<FuncCallAST*>(ast)) {
      const StyioBuiltinMethodKind builtin_method = styio_builtin_method_kind(call->getNameAsStr());
      if (call->func_callee != nullptr
          && styio_is_predefined_string_operation_kind(builtin_method)) {
        const StyioDataType callee = type_hint(call->func_callee);
        if (callee.option == StyioDataTypeOption::String) {
          return styio_make_list_type(
            builtin_method == StyioBuiltinMethodKind::StringChars
              ? "char"
              : "string");
        }
      }
      if (call->func_callee != nullptr
          && styio_is_predefined_list_operation_kind(builtin_method)) {
        return StyioDataType{StyioDataTypeOption::Integer, "i64", 64};
      }
      return ast->getDataType();
    }

    return ast->getDataType();
  }

  StyioDataType resource_selector_value_type(ResourceRefAST* ref) const {
    if (ref == nullptr || ref->isWholeResource()) {
      return undefined_type();
    }
    auto hit = binding_types_.find(std::string("@") + ref->getNameStr());
    if (hit == binding_types_.end()) {
      return ref->getDataType();
    }
    StyioDataType value_type = styio_topology_resource_value_type(hit->second);
    if (ref->getSelectorKind() == ResourceSelectorKind::SliceFrom
        || ref->getSelectorKind() == ResourceSelectorKind::SnapshotAll) {
      return styio_make_list_type(value_type.name);
    }
    return value_type;
  }

  std::size_t add_value(StyioAST* ast, std::string label, Context ctx) {
    const StyioDataType type = type_hint(ast);
    return add_ast_node(
      ast,
      NodeKind::Value,
      std::move(label),
      capabilities_from_type(type),
      state_from_type(type),
      ctx);
  }

  void record_binding(const std::string& name, std::size_t node, const StyioDataType& type) {
    if (name.empty() || node == kNoNode) {
      return;
    }
    binding_nodes_[name] = node;
    binding_types_[name] = type;
  }

  void require_cap(std::size_t node_id, Capability cap, const std::string& msg, const StyioAST* source) {
    if (node_id == kNoNode) {
      error(msg + " (missing node)", source);
      return;
    }
    const Node& node = result_.graph.node(node_id);
    if (!has_capability(node.capabilities, cap)) {
      error(msg + ": " + node.label, source);
    }
  }

  void own_if_close(std::size_t owner, std::size_t resource) {
    if (owner == kNoNode || resource == kNoNode) {
      return;
    }
    if (has_capability(result_.graph.node(resource).capabilities, Capability::Close)) {
      result_.graph.add_edge(EdgeKind::Ownership, owner, resource, "close-owner");
    }
  }

  void record_access(
    std::size_t resource,
    std::size_t owner,
    bool exclusive,
    const StyioAST* source,
    std::string label
  ) {
    if (resource == kNoNode) {
      return;
    }
    resource_accesses_.push_back(ResourceAccess{
      resource,
      owner == kNoNode ? 0 : owner,
      exclusive,
      source,
      std::move(label)});
  }

  std::size_t named_execution_node(StyioAST* ast) const {
    if (auto* name = dynamic_cast<NameAST*>(ast)) {
      const std::string key = name->getAsStr();
      auto bit = block_bindings_.find(key);
      if (bit != block_bindings_.end()) {
        return bit->second;
      }
      auto tit = task_bindings_.find(key);
      if (tit != task_bindings_.end()) {
        return tit->second;
      }
      auto hit = binding_nodes_.find(key);
      if (hit != binding_nodes_.end()) {
        return hit->second;
      }
    }
    return kNoNode;
  }

  bool happens_before(std::size_t before, std::size_t after) const {
    if (before == kNoNode || after == kNoNode || before == after) {
      return before == after;
    }
    std::vector<std::size_t> stack{before};
    std::unordered_set<std::size_t> seen;
    while (!stack.empty()) {
      const std::size_t cur = stack.back();
      stack.pop_back();
      if (!seen.insert(cur).second) {
        continue;
      }
      for (const auto& edge : result_.graph.edges()) {
        if (edge.kind != EdgeKind::HappensBefore || edge.from != cur) {
          continue;
        }
        if (edge.to == after) {
          return true;
        }
        stack.push_back(edge.to);
      }
    }
    return false;
  }

  std::string resource_family_for_expr(StyioAST* expr) const {
    if (expr == nullptr) {
      return "";
    }
    if (dynamic_cast<FileResourceAST*>(expr) != nullptr) {
      return "file";
    }
    if (auto* stream = dynamic_cast<StdStreamAST*>(expr)) {
      return styio_std_stream_family_name(stream->getStreamKind());
    }
    if (auto* receiver = dynamic_cast<ResourceReceiverAST*>(expr)) {
      return receiver->getFamilyName();
    }
    if (dynamic_cast<ResourceRefAST*>(expr) != nullptr) {
      return "resource";
    }
    const StyioDataType type = type_hint(expr);
    switch (type.handle_family) {
      case StyioHandleFamily::File:
        return "file";
      case StyioHandleFamily::Stream:
        if (type.has_std_stream_kind) {
          return styio_std_stream_family_name(static_cast<StdStreamKind>(type.std_stream_kind));
        }
        return "stream";
      default:
        break;
    }
    if (styio_is_topology_resource_type(type)) {
      return "resource";
    }
    return "";
  }

  const ResourceMethodInfo* find_resource_method(const std::string& family, const std::string& method) const {
    auto family_it = resource_methods_.find(family);
    if (family_it == resource_methods_.end()) {
      return nullptr;
    }
    auto method_it = family_it->second.find(method);
    if (method_it == family_it->second.end()) {
      return nullptr;
    }
    return &method_it->second;
  }

  bool resource_method_body_consumes_receiver(StyioAST* ast, const std::string& family) const {
    if (ast == nullptr) {
      return false;
    }
    if (auto* redirect = dynamic_cast<ResourceRedirectAST*>(ast)) {
      auto* receiver = dynamic_cast<ResourceReceiverAST*>(redirect->getData());
      if (receiver != nullptr && receiver->getFamilyName() == family
          && dynamic_cast<EmptyResourceAST*>(redirect->getResource()) != nullptr) {
        return true;
      }
      return resource_method_body_consumes_receiver(redirect->getData(), family)
        || resource_method_body_consumes_receiver(redirect->getResource(), family);
    }
    if (auto* call = dynamic_cast<FuncCallAST*>(ast)) {
      auto* receiver = dynamic_cast<ResourceReceiverAST*>(call->func_callee);
      if (receiver != nullptr && receiver->getFamilyName() == family) {
        const ResourceMethodInfo* info = find_resource_method(family, call->getNameAsStr());
        if (info != nullptr && info->consuming) {
          return true;
        }
      }
      for (auto* arg : call->getArgList()) {
        if (resource_method_body_consumes_receiver(arg, family)) {
          return true;
        }
      }
      return false;
    }
    if (auto* block = dynamic_cast<BlockAST*>(ast)) {
      for (auto* stmt : block->stmts) {
        if (resource_method_body_consumes_receiver(stmt, family)) {
          return true;
        }
      }
      return false;
    }
    if (auto* bin = dynamic_cast<BinOpAST*>(ast)) {
      return resource_method_body_consumes_receiver(bin->LHS, family)
        || resource_method_body_consumes_receiver(bin->RHS, family);
    }
    if (auto* cond = dynamic_cast<CondAST*>(ast)) {
      return resource_method_body_consumes_receiver(cond->getValue(), family)
        || resource_method_body_consumes_receiver(cond->getLHS(), family)
        || resource_method_body_consumes_receiver(cond->getRHS(), family);
    }
    return false;
  }

  std::size_t visit_resource_sink_write(
    StyioAST* ast,
    StyioAST* data_expr,
    StyioAST* resource_expr,
    Context ctx,
    const char* sink_label,
    const char* data_edge_label,
    const char* commit_label,
    const char* mutation_label,
    const char* cap_msg,
    const char* access_label
  ) {
    const std::size_t resource = visit(
      resource_expr,
      detached_context(ctx, "resource"));
    const std::size_t sink = add_ast_node(
      ast,
      NodeKind::Sink,
      sink_label,
      Capability::None,
      TypeState::Ready,
      ctx);
    const std::size_t data = visit(
      data_expr,
      child_context(ctx, sink, "data"));
    require_cap(resource, Capability::Push, cap_msg, resource_expr);
    if (data != kNoNode) {
      result_.graph.add_edge(EdgeKind::Flow, data, sink, data_edge_label);
    }
    if (resource != kNoNode) {
      const bool logical = dynamic_cast<ResourceRefAST*>(resource_expr) != nullptr;
      result_.graph.add_edge(
        EdgeKind::Commit,
        sink,
        resource,
        logical ? "pending-write-commit" : commit_label);
      result_.graph.add_edge(
        EdgeKind::Mutation,
        sink,
        resource,
        logical ? "pending-write" : mutation_label);
      result_.graph.add_edge(EdgeKind::Backpressure, sink, resource, "write-pressure");
      record_access(resource, ctx.owner, true, ast, access_label);
      own_if_close(sink, resource);
    }
    return sink;
  }

  std::size_t visit(StyioAST* ast, Context ctx) {
    if (ast == nullptr) {
      return kNoNode;
    }

    if (auto* f = dynamic_cast<FileResourceAST*>(ast)) {
      return add_ast_node(
        ast,
        NodeKind::DriverSource,
        file_label(f),
        Capability::Pull | Capability::Iter | Capability::Push | Capability::Close,
        TypeState::Declared,
        ctx);
    }

    if (auto* s = dynamic_cast<StdStreamAST*>(ast)) {
      Capability caps = Capability::None;
      if (s->getStreamKind() == StdStreamKind::Stdin) {
        caps = Capability::Pull | Capability::Iter;
      }
      else {
        caps = Capability::Push;
      }
      return add_ast_node(ast, NodeKind::DriverSource, std_stream_label(s->getStreamKind()), caps, TypeState::Open, ctx);
    }

    if (dynamic_cast<EmptyResourceAST*>(ast) != nullptr) {
      return add_ast_node(
        ast,
        NodeKind::Sink,
        "@()",
        Capability::None,
        TypeState::Closed,
        ctx);
    }

    if (auto* receiver = dynamic_cast<ResourceReceiverAST*>(ast)) {
      Capability caps = Capability::Pull | Capability::Iter | Capability::Push | Capability::Close;
      if (styio_is_stdin_resource_family_name(receiver->getFamilyName())) {
        caps = Capability::Pull | Capability::Iter;
      }
      else if (styio_is_stdout_resource_family_name(receiver->getFamilyName())
               || styio_is_stderr_resource_family_name(receiver->getFamilyName())) {
        caps = Capability::Push;
      }
      return add_ast_node(
        ast,
        NodeKind::Handle,
        std::string("receiver:@") + receiver->getFamilyName(),
        caps,
        TypeState::Open,
        ctx);
    }

    if (auto* method = dynamic_cast<ResourceMethodDefAST*>(ast)) {
      resource_methods_[method->getFamilyName()][method->getMethodName()] = ResourceMethodInfo{
        !method->isProperty()
          && resource_method_body_consumes_receiver(method->getBody(), method->getFamilyName()),
        method->isProperty(),
        method->getParams().size(),
      };
      const std::size_t node = add_ast_node(
        ast,
        NodeKind::Value,
        std::string("resource-method:@") + method->getFamilyName() + "::" + method->getMethodName(),
        Capability::None,
        TypeState::Ready,
        ctx);
      if (method->getBody() != nullptr) {
        visit(method->getBody(), child_context(ctx, node, "body"));
      }
      return node;
    }

    if (auto* order = dynamic_cast<ResourceOrderAST*>(ast)) {
      std::size_t before = named_execution_node(order->getBefore());
      if (before == kNoNode) {
        before = visit(order->getBefore(), related_context(ctx, "before"));
      }
      std::size_t after = named_execution_node(order->getAfter());
      if (after == kNoNode) {
        after = visit(order->getAfter(), related_context(ctx, "after"));
      }
      result_.graph.add_edge(EdgeKind::HappensBefore, before, after, "sequence");
      return after;
    }

    if (auto* decl = dynamic_cast<ResourceDeclAST*>(ast)) {
      if (!ctx.top_level_resource_scope) {
        error("resource declarations are top-level only", ast);
      }
      std::size_t last = kNoNode;
      for (const auto& slot : decl->getSlots()) {
        StyioDataType type = styio_normalize_resource_decl_type(slot.type->getDataType());
        const std::size_t node = add_ast_node(
          ast,
          NodeKind::StateSlot,
          std::string("resource:@") + slot.name->getAsStr(),
          capabilities_from_type(type),
          TypeState::Declared,
          ctx,
          "slot:" + slot.name->getAsStr() + ":" + type.name);
        resource_nodes_[slot.name->getAsStr()] = node;
        record_binding(std::string("@") + slot.name->getAsStr(), node, type);
        last = node;
      }
      if (decl->getDriver() != nullptr) {
        visit(
          decl->getDriver(),
          child_context(
            ctx,
            last == kNoNode ? ctx.owner : last,
            "driver"));
      }
      return last;
    }

    if (auto* ref = dynamic_cast<ResourceRefAST*>(ast)) {
      auto hit = resource_nodes_.find(ref->getNameStr());
      if (hit == resource_nodes_.end()) {
        error("unknown resource: @" + ref->getNameStr(), ast);
        return add_value(ast, "resource-ref:@" + ref->getNameStr(), ctx);
      }
      const std::size_t resource = hit->second;
      if (ref->isWholeResource()) {
        result_.graph.add_edge(EdgeKind::Borrow, ctx.owner, resource, "resource-ref");
        record_access(resource, ctx.owner, false, ast, "resource-ref");
        return resource;
      }
      require_cap(resource, Capability::Pull, "resource selector needs read capability", ast);
      require_cap(resource, Capability::Checkpoint, "resource selector needs snapshot capability", ast);
      const auto type_hit = binding_types_.find(std::string("@") + ref->getNameStr());
      const StyioDataType resource_type = type_hit != binding_types_.end()
        ? type_hit->second
        : ref->getDataType();
      const bool bounded_history =
        (resource_type.resource_shape == StyioResourceShapeKind::Fixed
         || resource_type.resource_shape == StyioResourceShapeKind::Recent)
        && resource_type.resource_shape_bound > 0;
      if ((ref->getSelectorKind() == ResourceSelectorKind::SliceFrom
           || ref->getSelectorKind() == ResourceSelectorKind::SnapshotAll)
          && !bounded_history) {
        error(
          "resource `" + ref->getNameStr()
            + "` slice/snapshot selection requires a bounded topology resource",
          ast);
        return add_ast_node(
          ast,
          NodeKind::Value,
          std::string("resource-select:@") + ref->getNameStr(),
          Capability::None,
          TypeState::Unknown,
          ctx);
      }
      if (ref->getSelectorKind() == ResourceSelectorKind::SliceFrom) {
        if (ref->getSelectorOffset() == std::numeric_limits<int>::min()) {
          error("resource slice selector depth exceeds supported selector depth", ast);
          return add_ast_node(
            ast,
            NodeKind::Value,
            std::string("resource-select:@") + ref->getNameStr(),
            Capability::None,
            TypeState::Unknown,
            ctx);
        }
        const int depth = -ref->getSelectorOffset();
        if (depth <= 0) {
          error("resource slice selector requires a negative history offset", ast);
          return add_ast_node(
            ast,
            NodeKind::Value,
            std::string("resource-select:@") + ref->getNameStr(),
            Capability::None,
            TypeState::Unknown,
            ctx);
        }
        if (static_cast<std::size_t>(depth) > resource_type.resource_shape_bound) {
          error(
            "resource selector depth exceeds resource `@" + ref->getNameStr()
              + "` history bound",
            ast);
          return add_ast_node(
            ast,
            NodeKind::Value,
            std::string("resource-select:@") + ref->getNameStr(),
            Capability::None,
            TypeState::Unknown,
            ctx);
        }
      }
      const StyioDataType selector_type = resource_selector_value_type(ref);
      const std::size_t value = add_ast_node(
        ast,
        NodeKind::Value,
        std::string("resource-select:@") + ref->getNameStr(),
        capabilities_from_type(selector_type),
        state_from_type(selector_type),
        ctx);
      result_.graph.add_edge(EdgeKind::Flow, resource, value, "committed-snapshot-read");
      result_.graph.add_edge(EdgeKind::Borrow, value, resource, "selector-borrow");
      record_access(resource, ctx.owner, false, ast, "selector-read");
      return value;
    }

    if (auto* name = dynamic_cast<NameAST*>(ast)) {
      auto hit = handle_bindings_.find(name->getAsStr());
      if (hit != handle_bindings_.end()) {
        result_.graph.add_edge(EdgeKind::Borrow, ctx.owner, hit->second, "name-ref");
        record_access(hit->second, ctx.owner, false, ast, "name-ref");
        return hit->second;
      }
      hit = state_slots_.find(name->getAsStr());
      if (hit != state_slots_.end()) {
        result_.graph.add_edge(EdgeKind::Borrow, ctx.owner, hit->second, "state-ref");
        return hit->second;
      }
      hit = task_bindings_.find(name->getAsStr());
      if (hit != task_bindings_.end()) {
        result_.graph.add_edge(EdgeKind::Borrow, ctx.owner, hit->second, "task_ref");
        return hit->second;
      }
      hit = binding_nodes_.find(name->getAsStr());
      if (hit != binding_nodes_.end()) {
        result_.graph.add_edge(EdgeKind::Borrow, ctx.owner, hit->second, "binding-ref");
        return hit->second;
      }
      return add_value(ast, std::string("value:") + name->getAsStr(), ctx);
    }

    if (auto* acq = dynamic_cast<HandleAcquireAST*>(ast)) {
      const std::string binding_name = acq->getVar()->getNameAsStr();
      const Context binding_context = named_owner_context(
        ctx,
        "binding:" + binding_name,
        "initializer");
      const std::size_t source = visit(
        acq->getResource(),
        detached_context(binding_context, "resource"));
      StyioDataType binding_type = acq->getVar()->getDType()->getDataType();
      if (binding_type.isUndefined()) {
        binding_type = type_hint(acq->getResource());
      }
      if (acq->isFlexBind()) {
        if (auto* stream = dynamic_cast<StdStreamAST*>(acq->getResource())) {
          if (stream->getStreamKind() == StdStreamKind::Stdin) {
            binding_type = styio_make_list_type("string");
          }
        }
      }
      const std::size_t handle = add_ast_node(
        ast,
        NodeKind::Handle,
        std::string("handle:") + binding_name,
        capabilities_from_type(binding_type),
        state_from_type(binding_type),
        ctx);
      if (source != kNoNode) {
        result_.graph.add_edge(EdgeKind::Ownership, handle, source, "acquire");
        result_.graph.add_edge(EdgeKind::Mutation, handle, source, "open");
      }
      handle_bindings_[binding_name] = handle;
      record_binding(binding_name, handle, binding_type);
      return handle;
    }

    if (auto* effect = dynamic_cast<ResourceEffectAST*>(ast)) {
      const StyioDataType effect_type = type_hint(effect);
      const bool value_like = !effect_type.isUndefined();
      const std::size_t node = add_ast_node(
        ast,
        value_like ? NodeKind::Value : NodeKind::StreamOp,
        value_like ? "value:resource_effect" : "resource:effect",
        capabilities_from_type(effect_type),
        state_from_type(effect_type),
        ctx);
      const std::size_t op = visit(
        effect->getOperation(),
        child_context(ctx, node, "operation"));
      if (op != kNoNode) {
        result_.graph.add_edge(EdgeKind::Flow, op, node, "resource-effect-operation");
        result_.graph.add_edge(EdgeKind::Failure, op, node, "resource-effect-settlement");
      }
      for (const auto& handler : effect->getHandlers()) {
        const std::size_t body = visit(
          handler.body,
          child_context(ctx, node, "handler:" + handler.effect_name));
        if (body != kNoNode) {
          result_.graph.add_edge(EdgeKind::Flow, node, body, "resource-effect-handler");
        }
      }
      if (effect->hasFallback()) {
        const std::size_t fallback = visit(
          effect->getFallback(),
          child_context(ctx, node, "fallback"));
        if (fallback != kNoNode) {
          result_.graph.add_edge(EdgeKind::Flow, node, fallback, "resource-effect-fallback");
        }
      }
      return node;
    }

    if (auto* wr = dynamic_cast<ResourceWriteAST*>(ast)) {
      if (auto* target_name = dynamic_cast<NameAST*>(wr->getData())) {
        if (auto* stream = dynamic_cast<StdStreamAST*>(wr->getResource())) {
          if (stream->getStreamKind() == StdStreamKind::Stdin
              && binding_nodes_.find(target_name->getAsStr()) == binding_nodes_.end()) {
            const std::string binding_name = target_name->getAsStr();
            const Context binding_context = named_owner_context(
              ctx,
              "binding:" + binding_name,
              "initializer");
            const std::size_t resource = visit(
              wr->getResource(),
              detached_context(binding_context, "resource"));
            const StyioDataType collected_type = styio_make_list_type("string");
            const std::size_t binding = add_ast_node(
              ast,
              NodeKind::StateSlot,
              std::string("collect:stdin:") + binding_name,
              capabilities_from_type(collected_type),
              state_from_type(collected_type),
              ctx,
              "binding:" + binding_name);
            if (resource != kNoNode) {
              result_.graph.add_edge(EdgeKind::Flow, resource, binding, "stdin-collect");
              result_.graph.add_edge(EdgeKind::Mutation, binding, resource, "collect-read");
              result_.graph.add_edge(EdgeKind::Backpressure, binding, resource, "collect-pressure");
            }
            record_binding(binding_name, binding, collected_type);
            return binding;
          }
        }
      }
      return visit_resource_sink_write(
        ast,
        wr->getData(),
        wr->getResource(),
        ctx,
        "sink:resource_write",
        "write-data",
        "write-commit",
        "write",
        "write target must have push capability",
        "write");
    }

    if (auto* redir = dynamic_cast<ResourceRedirectAST*>(ast)) {
      if (dynamic_cast<EmptyResourceAST*>(redir->getResource()) != nullptr) {
        const std::size_t sink = add_ast_node(
          ast,
          NodeKind::Sink,
          "sink:resource_redirect",
          Capability::None,
          TypeState::Ready,
          ctx);
        const std::size_t data = visit(
          redir->getData(),
          child_context(ctx, sink, "data"));
        if (data != kNoNode) {
          result_.graph.add_edge(EdgeKind::Mutation, sink, data, "destroy");
          result_.graph.add_edge(EdgeKind::Commit, sink, data, "destroy-commit");
          record_access(data, ctx.owner, true, ast, "destroy");
          destroyed_resources_.insert(data);
        }
        return sink;
      }
      return visit_resource_sink_write(
        ast,
        redir->getData(),
        redir->getResource(),
        ctx,
        "sink:resource_redirect",
        "redirect-data",
        "redirect-commit",
        "redirect",
        "redirect target must have push capability",
        "redirect");
    }

    if (auto* iter = dynamic_cast<IteratorAST*>(ast)) {
      const std::size_t collection = visit(
        iter->collection,
        detached_context(ctx, "collection"));
      const std::size_t op = add_ast_node(
        ast,
        NodeKind::StreamOp,
        "stream:iterator",
        Capability::None,
        TypeState::Ready,
        ctx);
      require_cap(collection, Capability::Iter, "iterator source must have iter capability", iter->collection);
      if (collection != kNoNode) {
        result_.graph.add_edge(EdgeKind::Flow, collection, op, "iterate");
        result_.graph.add_edge(EdgeKind::Backpressure, op, collection, "iterator-pressure");
        record_access(collection, ctx.owner, false, ast, "iterate");
        own_if_close(op, collection);
      }
      for (auto* next : iter->following) {
        const std::size_t child = visit(
          next,
          child_context(ctx, op, "body"));
        if (child != kNoNode) {
          result_.graph.add_edge(EdgeKind::Flow, op, child, "iterator-body");
        }
      }
      return op;
    }

    if (auto* zip = dynamic_cast<StreamZipAST*>(ast)) {
      const std::size_t a = visit(
        zip->getCollectionA(),
        detached_context(ctx, "collection-a"));
      const std::size_t b = visit(
        zip->getCollectionB(),
        detached_context(ctx, "collection-b"));
      const std::size_t op = add_ast_node(
        ast,
        NodeKind::StreamOp,
        "stream:zip",
        Capability::None,
        TypeState::Ready,
        ctx);
      require_cap(a, Capability::Iter, "zip lhs must have iter capability", zip->getCollectionA());
      require_cap(b, Capability::Iter, "zip rhs must have iter capability", zip->getCollectionB());
      if (a != kNoNode) {
        result_.graph.add_edge(EdgeKind::Flow, a, op, "zip-a");
        result_.graph.add_edge(EdgeKind::Backpressure, op, a, "zip-a-pressure");
        record_access(a, ctx.owner, false, ast, "zip-a");
        own_if_close(op, a);
      }
      if (b != kNoNode) {
        result_.graph.add_edge(EdgeKind::Flow, b, op, "zip-b");
        result_.graph.add_edge(EdgeKind::Backpressure, op, b, "zip-b-pressure");
        record_access(b, ctx.owner, false, ast, "zip-b");
        own_if_close(op, b);
      }
      for (auto* next : zip->getFollowing()) {
        const std::size_t child = visit(
          next,
          child_context(ctx, op, "body"));
        if (child != kNoNode) {
          result_.graph.add_edge(EdgeKind::Flow, op, child, "zip-body");
        }
      }
      return op;
    }

    if (auto* snap = dynamic_cast<SnapshotDeclAST*>(ast)) {
      const std::string snapshot_name = snap->getVar()->getAsStr();
      const Context snapshot_context = named_owner_context(
        ctx,
        "snapshot:" + snapshot_name,
        "source");
      const std::size_t resource = visit(
        snap->getResource(),
        detached_context(snapshot_context, "resource"));
      const std::size_t state = add_ast_node(
        ast,
        NodeKind::StateSlot,
        std::string("snapshot:") + snapshot_name,
        Capability::StateRead | Capability::StateWrite,
        TypeState::Ready,
        ctx);
      state_slots_[snapshot_name] = state;
      require_cap(resource, Capability::Pull, "snapshot source must have pull capability", snap->getResource());
      if (resource != kNoNode) {
        result_.graph.add_edge(EdgeKind::Flow, resource, state, "snapshot");
        result_.graph.add_edge(EdgeKind::Mutation, state, resource, "snapshot-read");
        record_access(resource, ctx.owner, false, ast, "snapshot");
        own_if_close(state, resource);
      }
      return state;
    }

    if (auto* pull = dynamic_cast<InstantPullAST*>(ast)) {
      const std::size_t resource = visit(
        pull->getResource(),
        detached_context(ctx, "resource"));
      const std::size_t value = add_ast_node(ast, NodeKind::Value, "value:instant_pull", Capability::None, TypeState::Ready, ctx);
      require_cap(resource, Capability::Pull, "instant pull source must have pull capability", pull->getResource());
      if (resource != kNoNode) {
        result_.graph.add_edge(EdgeKind::Flow, resource, value, "instant-pull");
        result_.graph.add_edge(EdgeKind::Mutation, value, resource, "pull");
        record_access(resource, ctx.owner, false, ast, "pull");
        own_if_close(value, resource);
      }
      return value;
    }

    if (auto* sd = dynamic_cast<StateDeclAST*>(ast)) {
      const std::size_t state = add_ast_node(
        ast,
        NodeKind::StateSlot,
        state_slot_label(sd),
        Capability::StateRead | Capability::StateWrite | Capability::Pull | Capability::Push,
        TypeState::Ready,
        ctx);
      if (sd->getExportVar() != nullptr) {
        state_slots_[sd->getExportVar()->getNameAsStr()] = state;
      }
      if (sd->getAccName() != nullptr) {
        state_slots_[sd->getAccName()->getAsStr()] = state;
      }
      if (sd->getWindowHeader() != nullptr) {
        if (!int_ast_positive(sd->getWindowHeader())) {
          error("state window must be a positive integer", sd->getWindowHeader());
        }
        const std::size_t ledger = add_semantic_node(
          NodeKind::HiddenLedger,
          SemanticRole::StateWindowLedger,
          "hidden-ledger:window",
          Capability::StateRead | Capability::StateWrite,
          TypeState::Ready,
          sd->getWindowHeader(),
          node_owner_path(state),
          "state-window-ledger");
        result_.graph.add_edge(EdgeKind::Ownership, state, ledger, "state-window");
      }
      Context state_ctx{state, true};
      if (sd->getAccInit() != nullptr) {
        const std::size_t init = visit(
          sd->getAccInit(),
          related_context(state_ctx, "initializer"));
        if (init != kNoNode) {
          result_.graph.add_edge(EdgeKind::Flow, init, state, "state-init");
        }
      }
      const std::size_t update = visit(
        sd->getUpdateExpr(),
        related_context(state_ctx, "update"));
      if (update != kNoNode) {
        result_.graph.add_edge(EdgeKind::Mutation, state, update, "state-update");
      }
      return state;
    }

    if (auto* series = dynamic_cast<SeriesIntrinsicAST*>(ast)) {
      if (options_.validate_series_scope && !ctx.in_state_decl) {
        error("series intrinsic must be owned by a state declaration", ast);
      }
      const std::size_t ledger = add_ast_node(
        ast,
        NodeKind::HiddenLedger,
        "hidden-ledger:series",
        Capability::StateRead | Capability::StateWrite,
        TypeState::Ready,
        ctx);
      const std::size_t base = visit(
        series->getBase(),
        child_context(ctx, ledger, "base"));
      const std::size_t window = visit(
        series->getWindow(),
        child_context(ctx, ledger, "window"));
      if (auto* win_int = dynamic_cast<IntAST*>(series->getWindow())) {
        if (!int_ast_positive(win_int)) {
          error("series intrinsic window must be a positive integer", win_int);
        }
      }
      if (base != kNoNode) {
        result_.graph.add_edge(EdgeKind::Flow, base, ledger, "series-base");
      }
      if (window != kNoNode) {
        result_.graph.add_edge(EdgeKind::Intent, window, ledger, "series-window");
      }
      return ledger;
    }

    if (auto* task = dynamic_cast<TaskBlockAST*>(ast)) {
      const std::size_t node = add_ast_node(
        ast,
        NodeKind::Task,
        "task:block",
        Capability::Task | Capability::Close,
        TypeState::Ready,
        ctx);
      const std::size_t failure = add_semantic_node(
        NodeKind::FailureDomain,
        SemanticRole::TaskFailureDomain,
        "failure-domain:task",
        Capability::None,
        TypeState::Ready,
        task,
        node_owner_path(node),
        "task-failure-domain");
      result_.graph.add_edge(EdgeKind::Failure, node, failure, "task_failure");
      visit((*task).getBody(), child_context(ctx, node, "body"));
      return node;
    }

    if (auto* group = dynamic_cast<TaskGroupLaunchAST*>(ast)) {
      const std::size_t node = add_ast_node(
        ast,
        NodeKind::Task,
        "task:group",
        Capability::Task,
        TypeState::Ready,
        ctx);
      for (auto* entry : group->getEntries()) {
        visit(entry, child_context(ctx, node, "entry"));
      }
      return node;
    }

    if (auto* flow = dynamic_cast<FlowBindAST*>(ast)) {
      const std::string target_name = flow->getTargetNameAsStr();
      const Context target_context = named_owner_context(
        ctx,
        "binding:" + target_name,
        "source");
      const std::size_t source = visit(
        flow->getSource(),
        detached_context(target_context, "source"));
      const std::size_t sink = add_ast_node(
        ast,
        NodeKind::Sink,
        std::string("sink:flow_bind:") + target_name,
        Capability::None,
        TypeState::Ready,
        ctx);
      if (source != kNoNode) {
        result_.graph.add_edge(EdgeKind::Flow, source, sink, "flow-bind");
      }
      return sink;
    }

    if (auto* block = dynamic_cast<BlockAST*>(ast)) {
      const std::size_t block_node = add_ast_node(ast, NodeKind::StreamOp, "block", Capability::None, TypeState::Ready, ctx);
      for (auto* stmt : block->stmts) {
        visit(stmt, child_context(ctx, block_node, "statement"));
      }
      for (auto* following : block->followings) {
        visit(following, child_context(ctx, block_node, "following"));
      }
      return block_node;
    }

    if (auto* print = dynamic_cast<PrintAST*>(ast)) {
      const std::size_t sink = add_ast_node(ast, NodeKind::Sink, "sink:stdout_print", Capability::None, TypeState::Ready, ctx);
      for (auto* expr : print->exprs) {
        const std::size_t value = visit(
          expr,
          child_context(ctx, sink, "argument"));
        if (value != kNoNode) {
          result_.graph.add_edge(EdgeKind::Flow, value, sink, "print");
        }
      }
      return sink;
    }

    if (auto* var = dynamic_cast<VarAST*>(ast)) {
      if (var->val_init != nullptr) {
        return visit(
          var->val_init,
          named_owner_context(
            ctx,
            "binding:" + var->getNameAsStr(),
            "initializer"));
      }
      return add_value(ast, std::string("var:") + var->getNameAsStr(), ctx);
    }

    if (auto* flex = dynamic_cast<FlexBindAST*>(ast)) {
      const std::string binding_name = flex->getNameAsStr();
      const std::size_t value = visit(
        flex->getValue(),
        named_owner_context(
          ctx,
          "binding:" + binding_name,
          "initializer"));
      StyioDataType binding_type = flex->getVar()->getDType()->getDataType();
      if (binding_type.isUndefined()) {
        binding_type = type_hint(flex->getValue());
      }
      const std::size_t binding = add_ast_node(
        ast,
        NodeKind::Value,
        std::string("binding:") + binding_name,
        capabilities_from_type(binding_type),
        state_from_type(binding_type),
        ctx);
      if (value != kNoNode) {
        result_.graph.add_edge(EdgeKind::Flow, value, binding, "flex-bind");
      }
      record_binding(binding_name, binding, binding_type);
      if (value != kNoNode && result_.graph.node(value).kind == NodeKind::Task) {
        task_bindings_[binding_name] = value;
        unordered_execution_nodes_.insert(value);
      }
      if (value != kNoNode && result_.graph.node(value).kind == NodeKind::StreamOp) {
        block_bindings_[binding_name] = value;
        unordered_execution_nodes_.insert(value);
      }
      if (value != kNoNode
          && has_capability(result_.graph.node(value).capabilities, Capability::Close)) {
        handle_bindings_[binding_name] = value;
      }
      return binding;
    }

    if (auto* final_bind = dynamic_cast<FinalBindAST*>(ast)) {
      const std::string binding_name = final_bind->getName();
      const std::size_t value = visit(
        final_bind->getValue(),
        named_owner_context(
          ctx,
          "binding:" + binding_name,
          "initializer"));
      StyioDataType binding_type = final_bind->getVar()->getDType()->getDataType();
      if (binding_type.isUndefined()) {
        binding_type = type_hint(final_bind->getValue());
      }
      const std::size_t binding = add_ast_node(
        ast,
        NodeKind::Value,
        std::string("binding:") + binding_name,
        capabilities_from_type(binding_type),
        state_from_type(binding_type),
        ctx);
      if (value != kNoNode) {
        result_.graph.add_edge(EdgeKind::Flow, value, binding, "final-bind");
      }
      record_binding(binding_name, binding, binding_type);
      if (value != kNoNode && result_.graph.node(value).kind == NodeKind::Task) {
        task_bindings_[binding_name] = value;
        unordered_execution_nodes_.insert(value);
      }
      if (value != kNoNode && result_.graph.node(value).kind == NodeKind::StreamOp) {
        block_bindings_[binding_name] = value;
        unordered_execution_nodes_.insert(value);
      }
      if (value != kNoNode
          && has_capability(result_.graph.node(value).capabilities, Capability::Close)) {
        handle_bindings_[binding_name] = value;
      }
      return binding;
    }

    if (auto* par = dynamic_cast<ParallelAssignAST*>(ast)) {
      std::size_t last = kNoNode;
      const auto& lhs = par->getLHS();
      const auto& rhs = par->getRHS();
      for (std::size_t i = 0; i < rhs.size(); ++i) {
        const std::string binding_name = i < lhs.size()
          ? binding_name_for(lhs[i])
          : std::string{};
        last = visit(
          rhs[i],
          binding_name.empty()
            ? related_context(ctx, "parallel-value")
            : named_owner_context(
                ctx,
                "binding:" + binding_name,
                "initializer"));
      }
      return last;
    }

    if (auto* bin = dynamic_cast<BinOpAST*>(ast)) {
      const std::size_t node = add_value(ast, "value:binop", ctx);
      visit(bin->getLHS(), child_context(ctx, node, "lhs"));
      visit(bin->getRHS(), child_context(ctx, node, "rhs"));
      return node;
    }

    if (auto* cmp = dynamic_cast<BinCompAST*>(ast)) {
      const std::size_t node = add_value(ast, "value:compare", ctx);
      visit(cmp->getLHS(), child_context(ctx, node, "lhs"));
      visit(cmp->getRHS(), child_context(ctx, node, "rhs"));
      return node;
    }

    if (auto* cond = dynamic_cast<CondAST*>(ast)) {
      const std::size_t node = add_value(ast, "value:condition", ctx);
      visit(cond->getValue(), child_context(ctx, node, "value"));
      visit(cond->getLHS(), child_context(ctx, node, "lhs"));
      visit(cond->getRHS(), child_context(ctx, node, "rhs"));
      return node;
    }

    if (auto* call = dynamic_cast<FuncCallAST*>(ast)) {
      const std::size_t node = add_value(ast, std::string("value:call:") + call->getNameAsStr(), ctx);
      if (call->func_callee != nullptr) {
        const std::size_t receiver = visit(
          call->func_callee,
          related_context(ctx, "callee"));
        if (receiver != kNoNode) {
          const bool resource_like =
            has_capability(result_.graph.node(receiver).capabilities, Capability::Pull)
            || has_capability(result_.graph.node(receiver).capabilities, Capability::Push)
            || has_capability(result_.graph.node(receiver).capabilities, Capability::Close);
          if (resource_like) {
            const StyioBuiltinMethodKind builtin_method = styio_builtin_method_kind(call->getNameAsStr());
            const std::string family = resource_family_for_expr(call->func_callee);
            const ResourceMethodInfo* method = find_resource_method(family, call->getNameAsStr());
            const bool consuming = method != nullptr
              ? method->consuming
              : styio_is_resource_destroy_method_kind(builtin_method);
            const bool exclusive = consuming
              || (method != nullptr && styio_is_resource_write_method_kind(builtin_method));
            result_.graph.add_edge(
              exclusive ? EdgeKind::Mutation : EdgeKind::Borrow,
              node,
              receiver,
              std::string("resource-method:") + call->getNameAsStr());
            record_access(receiver, ctx.owner, exclusive, ast, call->getNameAsStr());
            if (consuming) {
              destroyed_resources_.insert(receiver);
            }
          }
        }
      }
      for (auto* arg : call->getArgList()) {
        visit(arg, child_context(ctx, node, "argument"));
      }
      return node;
    }

    if (auto* attr = dynamic_cast<AttrAST*>(ast)) {
      const std::size_t node = add_value(ast, "value:attr", ctx);
      visit(attr->body, child_context(ctx, node, "body"));
      visit(attr->attr, child_context(ctx, node, "attribute"));
      return node;
    }

    if (auto* list_op = dynamic_cast<ListOpAST*>(ast)) {
      const std::size_t node = add_value(ast, "value:listop", ctx);
      visit(list_op->getList(), child_context(ctx, node, "collection"));
      visit(list_op->getSlot1(), child_context(ctx, node, "slot-1"));
      visit(list_op->getSlot2(), child_context(ctx, node, "slot-2"));
      return node;
    }

    if (auto* list = dynamic_cast<ListAST*>(ast)) {
      const std::size_t node = add_value(ast, "value:list", ctx);
      for (auto* element : list->getElements()) {
        visit(element, child_context(ctx, node, "element"));
      }
      return node;
    }

    if (auto* tuple = dynamic_cast<TupleAST*>(ast)) {
      const std::size_t node = add_value(ast, "value:tuple", ctx);
      for (auto* element : tuple->getElements()) {
        visit(element, child_context(ctx, node, "element"));
      }
      return node;
    }

    if (auto* set = dynamic_cast<SetAST*>(ast)) {
      const std::size_t node = add_value(ast, "value:set", ctx);
      for (auto* element : set->getElements()) {
        visit(element, child_context(ctx, node, "element"));
      }
      return node;
    }

    if (auto* dict = dynamic_cast<DictAST*>(ast)) {
      const std::size_t node = add_value(ast, "value:dict", ctx);
      for (const auto& entry : dict->getEntries()) {
        visit(entry.key, child_context(ctx, node, "key"));
        visit(entry.value, child_context(ctx, node, "value"));
      }
      return node;
    }

    if (auto* fallback = dynamic_cast<FallbackAST*>(ast)) {
      const std::size_t node = add_value(ast, "value:fallback", ctx);
      visit(fallback->getPrimary(), child_context(ctx, node, "primary"));
      visit(fallback->getAlternate(), child_context(ctx, node, "alternate"));
      return node;
    }

    if (auto* guard = dynamic_cast<GuardSelectorAST*>(ast)) {
      const std::size_t node = add_value(ast, "value:guard", ctx);
      visit(guard->getBase(), child_context(ctx, node, "base"));
      visit(guard->getCond(), child_context(ctx, node, "condition"));
      return node;
    }

    if (auto* probe = dynamic_cast<EqProbeAST*>(ast)) {
      const std::size_t node = add_value(ast, "value:eq_probe", ctx);
      visit(probe->getBase(), child_context(ctx, node, "base"));
      visit(probe->getProbeValue(), child_context(ctx, node, "probe"));
      return node;
    }

    if (auto* wave = dynamic_cast<WaveMergeAST*>(ast)) {
      const std::size_t node = add_value(ast, "value:wave_merge", ctx);
      visit(wave->getCond(), child_context(ctx, node, "condition"));
      visit(wave->getTrueVal(), child_context(ctx, node, "true"));
      visit(wave->getFalseVal(), child_context(ctx, node, "false"));
      return node;
    }

    if (auto* dispatch = dynamic_cast<WaveDispatchAST*>(ast)) {
      const std::size_t node = add_value(ast, "value:wave_dispatch", ctx);
      visit(dispatch->getCond(), child_context(ctx, node, "condition"));
      visit(dispatch->getTrueArm(), child_context(ctx, node, "true"));
      visit(dispatch->getFalseArm(), child_context(ctx, node, "false"));
      return node;
    }

    return add_value(ast, std::string("value:") + reprASTType(ast->getNodeType()), ctx);
  }

  void finalize() {
    // Cycle detection (TASK-07): check HappensBefore + Flow edges.
    {
      auto cycles = result_.graph.detect_cycles();
      for (const auto& ci : cycles) {
        std::string desc;
        for (std::size_t i = 0; i < ci.node_ids.size(); ++i) {
          if (i > 0) {
            desc += " -> ";
          }
          desc += result_.graph.node(ci.node_ids[i]).label;
        }
        error("resource dependency cycle: " + desc,
              result_.graph.node(ci.node_ids.front()).source);
      }
    }

    for (std::size_t i = 0; i < resource_accesses_.size(); ++i) {
      for (std::size_t j = i + 1; j < resource_accesses_.size(); ++j) {
        const auto& a = resource_accesses_[i];
        const auto& b = resource_accesses_[j];
        if (a.resource != b.resource) {
          continue;
        }
        if (!a.exclusive && !b.exclusive) {
          continue;
        }
        if (a.owner == b.owner) {
          continue;
        }
        if (unordered_execution_nodes_.count(a.owner) == 0
            || unordered_execution_nodes_.count(b.owner) == 0) {
          continue;
        }
        if (happens_before(a.owner, b.owner) || happens_before(b.owner, a.owner)) {
          continue;
        }
        error(
          "unordered exclusive resource borrow: "
            + result_.graph.node(a.resource).label
            + " via " + a.label + " and " + b.label,
          a.source != nullptr ? a.source : b.source);
      }
    }

    std::vector<std::size_t> scope_drop_nodes;
    for (auto it = result_.graph.nodes().rbegin(); it != result_.graph.nodes().rend(); ++it) {
      const Node& node = *it;
      if (!has_capability(node.capabilities, Capability::Close)
          || destroyed_resources_.count(node.id) != 0) {
        continue;
      }
      scope_drop_nodes.push_back(node.id);
    }
    if (!scope_drop_nodes.empty()) {
      const std::size_t destroy_sink = add_semantic_node(
        NodeKind::Sink,
        SemanticRole::ScopeExitDestroySink,
        "@()",
        Capability::None,
        TypeState::Closed,
        nullptr,
        node_owner_path(0),
        "scope-exit-destroy-sink");
      for (std::size_t node_id : scope_drop_nodes) {
        result_.graph.add_edge(EdgeKind::Mutation, node_id, destroy_sink, "scope-exit-drop");
        result_.graph.add_edge(EdgeKind::Commit, node_id, destroy_sink, "scope-exit-drop-commit");
      }
    }

    if (!options_.require_close_owner) {
      return;
    }
    std::unordered_map<std::size_t, std::size_t> close_owner_count;
    for (const auto& edge : result_.graph.edges()) {
      if (edge.kind == EdgeKind::Ownership) {
        close_owner_count[edge.to] += 1;
      }
    }
    for (const auto& node : result_.graph.nodes()) {
      if (has_capability(node.capabilities, Capability::Close)
          && close_owner_count[node.id] == 0) {
        error("close-capable resource has no owner: " + node.label, node.source);
      }
    }
  }
};

} // namespace

std::size_t
Graph::add_node(
  NodeKind kind,
  SemanticRole semantic_role,
  styio::semantic_identity::SemanticIdentity semantic_id,
  std::string label,
  Capability capabilities,
  TypeState state,
  const StyioAST* source
) {
  const std::size_t id = nodes_.size();
  nodes_.push_back(Node{
    id,
    kind,
    semantic_role,
    semantic_id,
    std::move(label),
    capabilities,
    state,
    source});
  return id;
}

void
Graph::add_edge(EdgeKind kind, std::size_t from, std::size_t to, std::string label) {
  if (from == kNoNode || to == kNoNode) {
    return;
  }
  edges_.push_back(Edge{kind, from, to, std::move(label)});
}

const Node&
Graph::node(std::size_t id) const {
  if (id >= nodes_.size()) {
    throw std::out_of_range("resource topology node id out of range");
  }
  return nodes_[id];
}

std::size_t
Graph::node_count(NodeKind kind) const {
  return static_cast<std::size_t>(
    std::count_if(nodes_.begin(), nodes_.end(), [kind](const Node& n) {
      return n.kind == kind;
    }));
}

std::size_t
Graph::edge_count(EdgeKind kind) const {
  return static_cast<std::size_t>(
    std::count_if(edges_.begin(), edges_.end(), [kind](const Edge& e) {
      return e.kind == kind;
    }));
}

std::string
Graph::debug_string() const {
  std::ostringstream out;
  out << "nodes=" << nodes_.size() << " edges=" << edges_.size();
  for (const auto& node : nodes_) {
    out << "\nnode " << node.id << " " << to_string(node.kind)
        << " " << node.label << " caps=" << to_string(node.capabilities)
        << " state=" << to_string(node.state);
  }
  for (const auto& edge : edges_) {
    out << "\nedge " << edge.from << " -> " << edge.to << " "
        << to_string(edge.kind);
    if (!edge.label.empty()) {
      out << " " << edge.label;
    }
  }
  return out.str();
}

std::vector<CycleInfo>
Graph::detect_cycles() const {
  // Build adjacency list and in-degree map for ordering edges.
  std::unordered_map<std::size_t, std::vector<std::size_t>> adj;
  std::unordered_map<std::size_t, int> in_degree;

  // Use edge indices for cycle reporting.
  std::unordered_map<std::size_t, std::vector<std::size_t>> edge_adj;

  for (std::size_t ei = 0; ei < edges_.size(); ++ei) {
    const auto& e = edges_[ei];
    if (e.kind != EdgeKind::HappensBefore && e.kind != EdgeKind::Flow) {
      continue;
    }
    adj[e.from].push_back(e.to);
    edge_adj[e.from].push_back(ei);
    in_degree[e.to]++;
    if (in_degree.find(e.from) == in_degree.end()) {
      in_degree[e.from] = 0;
    }
  }

  // Kahn: collect zero-in-degree nodes.
  std::deque<std::size_t> queue;
  for (auto& [node, deg] : in_degree) {
    if (deg == 0) {
      queue.push_back(node);
    }
  }

  std::size_t processed = 0;
  const std::size_t total = in_degree.size();

  while (!queue.empty()) {
    std::size_t u = queue.front();
    queue.pop_front();
    processed++;

    for (std::size_t v : adj[u]) {
      if (--in_degree[v] == 0) {
        queue.push_back(v);
      }
    }
  }

  // If all nodes processed, graph is acyclic.
  if (processed == total) {
    return {};
  }

  // Remaining nodes form cycles — collect them.
  std::unordered_set<std::size_t> remaining;
  for (const auto& entry : in_degree) {
    if (entry.second > 0) {
      remaining.insert(entry.first);
    }
  }

  std::vector<CycleInfo> cycles;
  std::unordered_map<std::size_t, std::uint8_t> color;
  std::unordered_map<std::size_t, std::size_t> parent_node;
  std::unordered_map<std::size_t, std::size_t> parent_edge;

  auto record_cycle = [&](std::size_t from, std::size_t to, std::size_t edge_id) {
    CycleInfo ci;
    ci.node_ids.push_back(from);
    ci.edge_ids.push_back(edge_id);

    std::size_t cur = from;
    while (cur != to) {
      auto parent_it = parent_node.find(cur);
      auto edge_it = parent_edge.find(cur);
      if (parent_it == parent_node.end() || edge_it == parent_edge.end()) {
        return;
      }
      cur = parent_it->second;
      ci.node_ids.push_back(cur);
      ci.edge_ids.push_back(edge_it->second);
    }

    std::reverse(ci.node_ids.begin(), ci.node_ids.end());
    std::reverse(ci.edge_ids.begin(), ci.edge_ids.end());
    ci.node_ids.push_back(to);
    cycles.push_back(std::move(ci));
  };

  auto dfs = [&](auto&& self, std::size_t node) -> void {
    color[node] = 1;
    const auto adj_it = adj.find(node);
    const auto edge_it = edge_adj.find(node);
    if (adj_it != adj.end() && edge_it != edge_adj.end()) {
      const auto& targets = adj_it->second;
      const auto& edge_ids = edge_it->second;
      for (std::size_t i = 0; i < targets.size(); ++i) {
        const std::size_t next = targets[i];
        if (remaining.count(next) == 0) {
          continue;
        }
        const std::uint8_t next_color = color[next];
        if (next_color == 0) {
          parent_node[next] = node;
          parent_edge[next] = edge_ids[i];
          self(self, next);
        }
        else if (next_color == 1) {
          record_cycle(node, next, edge_ids[i]);
        }
      }
    }
    color[node] = 2;
  };

  for (std::size_t node : remaining) {
    if (color[node] == 0) {
      dfs(dfs, node);
    }
  }

  if (cycles.empty()) {
    for (std::size_t node : remaining) {
      CycleInfo ci;
      ci.node_ids.push_back(node);
      cycles.push_back(ci);
    }
  }

  return cycles;
}

std::string
ValidationReport::message() const {
  std::ostringstream out;
  for (std::size_t i = 0; i < errors.size(); ++i) {
    if (i != 0) {
      out << "\n";
    }
    out << errors[i].message;
  }
  return out.str();
}

ValidatedArtifact::ValidatedArtifact(
  Graph graph,
  styio::semantic_identity::Scope scope
) : graph_(std::move(graph)), scope_(std::move(scope)) {
  descriptors_.reserve(graph_.nodes().size());
  for (const auto& node : graph_.nodes()) {
    descriptors_.push_back(SemanticDescriptor{
      node.kind,
      node.semantic_role,
      node.semantic_id,
      scope_.is_globally_comparable()});
  }
}

BuildResult
build(MainBlockAST* ast, BuildOptions options) {
  return Builder(std::move(options)).build(ast);
}

BuildResult
build(BlockAST* ast, BuildOptions options) {
  return Builder(std::move(options)).build(ast);
}

bool
validation_is_noop_for_scalar_program(MainBlockAST* ast) {
  if (ast == nullptr || ast->getStmts().empty()) {
    return false;
  }
  for (auto* statement : ast->getStmts()) {
    if (auto* bind = dynamic_cast<FlexBindAST*>(statement)) {
      if (!resource_free_scalar_expression(bind->getValue())) {
        return false;
      }
      continue;
    }
    if (auto* print = dynamic_cast<PrintAST*>(statement)) {
      for (auto* expression : print->exprs) {
        if (!resource_free_scalar_expression(expression)) {
          return false;
        }
      }
      continue;
    }
    return false;
  }
  return true;
}

ValidatedArtifact
validate_or_throw(MainBlockAST* ast, std::string phase) {
  return validate_or_throw(
    ast,
    std::move(phase),
    styio::semantic_identity::Scope::anonymous());
}

ValidatedArtifact
validate_or_throw(
  MainBlockAST* ast,
  std::string phase,
  styio::semantic_identity::Scope scope
) {
  BuildOptions options;
  options.phase = std::move(phase);
  options.identity_scope = scope;
  BuildResult result = build(ast, options);
  if (!result.report.ok()) {
    throw StyioTypeError(result.report.message());
  }
  return ValidatedArtifact(std::move(result.graph), std::move(scope));
}

void
validate_or_throw(BlockAST* ast, std::string phase) {
  BuildOptions options;
  options.phase = std::move(phase);
  BuildResult result = build(ast, options);
  if (!result.report.ok()) {
    throw StyioTypeError(result.report.message());
  }
}

std::string
to_string(NodeKind kind) {
  switch (kind) {
    case NodeKind::Program: return "Program";
    case NodeKind::DriverSource: return "DriverSource";
    case NodeKind::Handle: return "Handle";
    case NodeKind::StreamOp: return "StreamOp";
    case NodeKind::StateSlot: return "StateSlot";
    case NodeKind::HiddenLedger: return "HiddenLedger";
    case NodeKind::Sink: return "Sink";
    case NodeKind::Task: return "Task";
    case NodeKind::FailureDomain: return "FailureDomain";
    case NodeKind::Value: return "Value";
  }
  return "UnknownNode";
}

std::string
to_string(SemanticRole role) {
  switch (role) {
    case SemanticRole::Program: return "Program";
    case SemanticRole::StandaloneBlock: return "StandaloneBlock";
    case SemanticRole::DriverSource: return "DriverSource";
    case SemanticRole::ResourceSlot: return "ResourceSlot";
    case SemanticRole::ResourceHandle: return "ResourceHandle";
    case SemanticRole::ResourceMethod: return "ResourceMethod";
    case SemanticRole::StreamOperation: return "StreamOperation";
    case SemanticRole::StateSlot: return "StateSlot";
    case SemanticRole::Snapshot: return "Snapshot";
    case SemanticRole::Value: return "Value";
    case SemanticRole::Sink: return "Sink";
    case SemanticRole::Task: return "Task";
    case SemanticRole::StateWindowLedger: return "StateWindowLedger";
    case SemanticRole::SeriesLedger: return "SeriesLedger";
    case SemanticRole::TaskFailureDomain: return "TaskFailureDomain";
    case SemanticRole::ScopeExitDestroySink: return "ScopeExitDestroySink";
  }
  return "UnknownSemanticRole";
}

std::string
to_string(EdgeKind kind) {
  switch (kind) {
    case EdgeKind::Flow: return "Flow";
    case EdgeKind::Intent: return "Intent";
    case EdgeKind::Ownership: return "Ownership";
    case EdgeKind::Borrow: return "Borrow";
    case EdgeKind::Mutation: return "Mutation";
    case EdgeKind::Backpressure: return "Backpressure";
    case EdgeKind::Commit: return "Commit";
    case EdgeKind::HappensBefore: return "HappensBefore";
    case EdgeKind::Failure: return "Failure";
    case EdgeKind::Placement: return "Placement";
  }
  return "UnknownEdge";
}

std::string
to_string(TypeState state) {
  switch (state) {
    case TypeState::Unknown: return "Unknown";
    case TypeState::Declared: return "Declared";
    case TypeState::Open: return "Open";
    case TypeState::Eof: return "Eof";
    case TypeState::Closed: return "Closed";
    case TypeState::Materialized: return "Materialized";
    case TypeState::Ready: return "Ready";
  }
  return "UnknownState";
}

std::string
to_string(Capability capabilities) {
  if (capabilities == Capability::None) {
    return "none";
  }
  std::vector<std::string> parts;
  auto push_if = [&](Capability cap, const char* name) {
    if (has_capability(capabilities, cap)) {
      parts.emplace_back(name);
    }
  };
  push_if(Capability::Pull, "pull");
  push_if(Capability::Iter, "iter");
  push_if(Capability::Push, "push");
  push_if(Capability::Close, "close");
  push_if(Capability::Clone, "clone");
  push_if(Capability::Checkpoint, "checkpoint");
  push_if(Capability::Task, "task");
  push_if(Capability::StateRead, "state-read");
  push_if(Capability::StateWrite, "state-write");
  std::ostringstream out;
  for (std::size_t i = 0; i < parts.size(); ++i) {
    if (i != 0) {
      out << "|";
    }
    out << parts[i];
  }
  return out.str();
}

} // namespace styio::resource_topology
