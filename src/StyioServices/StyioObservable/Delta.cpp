#include "Delta.hpp"

#include "JsonSupport.hpp"

#include <algorithm>
#include <unordered_set>
#include <utility>

namespace styio::observable {
namespace {

using json_detail::CompactJson;
using json_detail::JsonValue;

std::string metadata_key() { return "snapshot"; }

void emit_string_array(CompactJson& json, const std::vector<std::string>& values) {
  json.begin_array();
  for (const auto& value : values) {
    json.string_value(value);
  }
  json.end_array();
}

std::string json_string(std::string_view value) {
  return json_detail::quote_string(value);
}

std::string json_string_array(const std::vector<std::string>& values) {
  CompactJson json;
  emit_string_array(json, values);
  return json.buf;
}

std::string json_null() { return "null"; }

std::string node_json(const SnapshotNode& node) {
  CompactJson json;
  json.begin_object();
  json.key("id");
  json.string_value(node.id);
  json.key("kind");
  json.string_value(node.kind);
  json.key("role");
  json.string_value(node.role);
  json.key("anchors");
  emit_string_array(json, node.anchors);
  json.key("evidence");
  json.string_value(node.evidence);
  json.end_object();
  return json.buf;
}

std::string edge_json(const SnapshotEdge& edge) {
  CompactJson json;
  json.begin_object();
  json.key("id");
  json.string_value(edge.id);
  json.key("kind");
  json.string_value(edge.kind);
  json.key("from");
  json.string_value(edge.from);
  json.key("to");
  json.string_value(edge.to);
  json.key("evidence");
  json.string_value(edge.evidence);
  json.end_object();
  return json.buf;
}

std::string fact_json(const SnapshotFact& fact) {
  CompactJson json;
  json.begin_object();
  json.key("id");
  json.string_value(fact.id);
  json.key("subject");
  json.string_value(fact.subject);
  json.key("predicate");
  json.string_value(fact.predicate);
  json.key("value");
  if (fact.value_is_array) {
    emit_string_array(json, fact.array_value);
  } else {
    json.string_value(fact.string_value);
  }
  json.key("evidence");
  json.string_value(fact.evidence);
  json.end_object();
  return json.buf;
}

std::string diagnostic_json(const SnapshotDiagnostic& diagnostic) {
  CompactJson json;
  json.begin_object();
  json.key("id");
  json.string_value(diagnostic.id);
  json.key("code");
  json.string_value(diagnostic.code);
  json.key("severity");
  json.string_value(diagnostic.severity);
  json.key("subject");
  json.string_value(diagnostic.subject);
  json.key("evidence");
  json.string_value(diagnostic.evidence);
  json.end_object();
  return json.buf;
}

std::string anchor_json(const SnapshotAnchor& anchor) {
  CompactJson json;
  json.begin_object();
  json.key("ref");
  json.string_value(anchor.id);
  json.key("path");
  json.string_value(anchor.path);
  json.key("precision");
  json.string_value(anchor.precision);
  json.end_object();
  return json.buf;
}

std::string evidence_json(const SnapshotEvidence& evidence) {
  CompactJson json;
  json.begin_object();
  json.key("ref");
  json.string_value(evidence.id);
  json.key("producer_rule");
  json.string_value(evidence.producer_rule);
  json.key("rule_version");
  json.string_value(evidence.rule_version);
  json.key("subjects");
  emit_string_array(json, evidence.subjects);
  json.key("prerequisites");
  emit_string_array(json, evidence.prerequisites);
  json.key("anchors");
  emit_string_array(json, evidence.anchors);
  json.end_object();
  return json.buf;
}

std::string lineage_json(const LineageRecord& lineage) {
  CompactJson json;
  json.begin_object();
  json.key("id");
  json.string_value(lineage.id);
  json.key("kind");
  json.string_value(lineage.kind);
  json.key("prior");
  emit_string_array(json, lineage.prior);
  json.key("target");
  emit_string_array(json, lineage.target);
  json.key("producer_rule");
  json.string_value(lineage.producer_rule);
  json.key("rule_version");
  json.string_value(lineage.rule_version);
  json.key("evidence");
  emit_string_array(json, lineage.evidence);
  json.key("completeness");
  json.string_value(lineage.completeness);
  json.end_object();
  return json.buf;
}

struct NamedField
{
  std::string name;
  std::string json;
};

std::vector<NamedField> metadata_fields(const Snapshot& snapshot) {
  std::vector<NamedField> fields;
  fields.push_back({"completeness", json_string(snapshot.completeness)});
  fields.push_back({"producer_name", json_string(snapshot.producer.name)});
  fields.push_back({"producer_version", json_string(snapshot.producer.version)});
  fields.push_back({"root", snapshot.root_is_null ? json_null() : json_string(snapshot.root)});
  fields.push_back({"capabilities", json_string_array(snapshot.capabilities)});
  fields.push_back({"package_name", json_string(snapshot.compilation_unit.package_name)});
  fields.push_back({"manifest_path", json_string(snapshot.compilation_unit.manifest_path)});
  fields.push_back({"entry_path", json_string(snapshot.compilation_unit.entry_path)});
  fields.push_back({
    "parent_snapshot_id",
    snapshot.parent_snapshot_id.empty() ? json_null() : json_string(snapshot.parent_snapshot_id)
  });
  return fields;
}

std::vector<NamedField> node_fields(const SnapshotNode& node) {
  return {
    {"kind", json_string(node.kind)},
    {"role", json_string(node.role)},
    {"anchors", json_string_array(node.anchors)},
    {"evidence", json_string(node.evidence)},
  };
}

std::vector<NamedField> edge_fields(const SnapshotEdge& edge) {
  return {
    {"kind", json_string(edge.kind)},
    {"from", json_string(edge.from)},
    {"to", json_string(edge.to)},
    {"evidence", json_string(edge.evidence)},
  };
}

std::vector<NamedField> fact_fields(const SnapshotFact& fact) {
  return {
    {"subject", json_string(fact.subject)},
    {"predicate", json_string(fact.predicate)},
    {
      "value",
      fact.value_is_array ? json_string_array(fact.array_value) : json_string(fact.string_value)
    },
    {"evidence", json_string(fact.evidence)},
  };
}

std::vector<NamedField> diagnostic_fields(const SnapshotDiagnostic& diagnostic) {
  return {
    {"code", json_string(diagnostic.code)},
    {"severity", json_string(diagnostic.severity)},
    {"subject", json_string(diagnostic.subject)},
    {"evidence", json_string(diagnostic.evidence)},
  };
}

std::vector<NamedField> anchor_fields(const SnapshotAnchor& anchor) {
  return {
    {"path", json_string(anchor.path)},
    {"precision", json_string(anchor.precision)},
  };
}

std::vector<NamedField> evidence_fields(const SnapshotEvidence& evidence) {
  return {
    {"producer_rule", json_string(evidence.producer_rule)},
    {"rule_version", json_string(evidence.rule_version)},
    {"subjects", json_string_array(evidence.subjects)},
    {"prerequisites", json_string_array(evidence.prerequisites)},
    {"anchors", json_string_array(evidence.anchors)},
  };
}

std::vector<NamedField> lineage_fields(const LineageRecord& lineage) {
  return {
    {"kind", json_string(lineage.kind)},
    {"prior", json_string_array(lineage.prior)},
    {"target", json_string_array(lineage.target)},
    {"producer_rule", json_string(lineage.producer_rule)},
    {"rule_version", json_string(lineage.rule_version)},
    {"evidence", json_string_array(lineage.evidence)},
    {"completeness", json_string(lineage.completeness)},
  };
}

void append_replacements(
  std::vector<DeltaOperation>& operations,
  RecordCategory category,
  const std::string& key,
  const std::vector<NamedField>& before,
  const std::vector<NamedField>& after
) {
  DeltaOperation op;
  op.kind = DeltaOpKind::ReplaceFields;
  op.category = category;
  op.key = key;
  for (std::size_t i = 0; i < before.size(); ++i) {
    if (before[i].json != after[i].json) {
      op.fields.push_back(FieldReplacement{before[i].name, before[i].json, after[i].json});
    }
  }
  if (!op.fields.empty()) {
    operations.push_back(std::move(op));
  }
}

template <typename Record, typename FieldsFn, typename JsonFn>
void merge_category(
  std::vector<DeltaOperation>& operations,
  RecordCategory category,
  const std::vector<Record>& parent,
  const std::vector<Record>& child,
  FieldsFn fields,
  JsonFn to_json
) {
  std::size_t i = 0;
  std::size_t j = 0;
  while (i < parent.size() || j < child.size()) {
    if (j == child.size() || (i < parent.size() && parent[i].id < child[j].id)) {
      DeltaOperation op;
      op.kind = DeltaOpKind::Remove;
      op.category = category;
      op.key = parent[i].id;
      operations.push_back(std::move(op));
      ++i;
      continue;
    }
    if (i == parent.size() || child[j].id < parent[i].id) {
      DeltaOperation op;
      op.kind = DeltaOpKind::Add;
      op.category = category;
      op.key = child[j].id;
      op.record_json = to_json(child[j]);
      operations.push_back(std::move(op));
      ++j;
      continue;
    }
    append_replacements(operations, category, parent[i].id, fields(parent[i]), fields(child[j]));
    ++i;
    ++j;
  }
}

JsonValue parse_required(std::string_view json, std::string& error) {
  JsonValue value;
  if (!json_detail::parse_json(json, value, error)) {
    return {};
  }
  return value;
}

std::string field_as_json(const JsonValue& value) {
  CompactJson json;
  switch (value.kind) {
    case JsonValue::Kind::Null:
      json.null_value();
      break;
    case JsonValue::Kind::Bool:
      json.bool_value(value.bool_value);
      break;
    case JsonValue::Kind::Int:
      json.integer_value(value.int_value);
      break;
    case JsonValue::Kind::String:
      json.string_value(value.string_value);
      break;
    case JsonValue::Kind::Array:
      json.begin_array();
      for (const auto& item : value.array_value) {
        if (item.kind == JsonValue::Kind::String) {
          json.string_value(item.string_value);
        } else {
          json.raw_value(field_as_json(item));
        }
      }
      json.end_array();
      break;
    case JsonValue::Kind::Object:
      json.begin_object();
      for (const auto& entry : value.object_value) {
        if (entry.second == nullptr) {
          continue;
        }
        json.key(entry.first);
        json.raw_value(field_as_json(*entry.second));
      }
      json.end_object();
      break;
  }
  return json.buf;
}

bool decode_string_array(const JsonValue& value, std::vector<std::string>& out) {
  if (value.kind != JsonValue::Kind::Array) {
    return false;
  }
  out.clear();
  for (const auto& item : value.array_value) {
    if (item.kind != JsonValue::Kind::String) {
      return false;
    }
    out.push_back(item.string_value);
  }
  return true;
}

bool apply_metadata_field(Snapshot& snapshot, const FieldReplacement& field, std::string& error) {
  JsonValue parsed;
  if (!json_detail::parse_json(field.after, parsed, error)) {
    return false;
  }
  if (field.name == "completeness" && parsed.kind == JsonValue::Kind::String) {
    snapshot.completeness = parsed.string_value;
    return true;
  }
  if (field.name == "producer_name" && parsed.kind == JsonValue::Kind::String) {
    snapshot.producer.name = parsed.string_value;
    return true;
  }
  if (field.name == "producer_version" && parsed.kind == JsonValue::Kind::String) {
    snapshot.producer.version = parsed.string_value;
    return true;
  }
  if (field.name == "root") {
    if (parsed.is_null()) {
      snapshot.root_is_null = true;
      snapshot.root.clear();
      return true;
    }
    if (parsed.kind == JsonValue::Kind::String) {
      snapshot.root_is_null = false;
      snapshot.root = parsed.string_value;
      return true;
    }
  }
  if (field.name == "capabilities") {
    return decode_string_array(parsed, snapshot.capabilities);
  }
  if (field.name == "package_name" && parsed.kind == JsonValue::Kind::String) {
    snapshot.compilation_unit.package_name = parsed.string_value;
    return true;
  }
  if (field.name == "manifest_path" && parsed.kind == JsonValue::Kind::String) {
    snapshot.compilation_unit.manifest_path = parsed.string_value;
    return true;
  }
  if (field.name == "entry_path" && parsed.kind == JsonValue::Kind::String) {
    snapshot.compilation_unit.entry_path = parsed.string_value;
    return true;
  }
  if (field.name == "parent_snapshot_id") {
    if (parsed.is_null()) {
      snapshot.parent_snapshot_id.clear();
      return true;
    }
    if (parsed.kind == JsonValue::Kind::String) {
      snapshot.parent_snapshot_id = parsed.string_value;
      return true;
    }
  }
  error = "unknown metadata field: " + field.name;
  return false;
}

template <typename Record>
Record* find_mutable(std::vector<Record>& records, const std::string& key) {
  for (auto& record : records) {
    if (record.id == key) {
      return &record;
    }
  }
  return nullptr;
}

template <typename Record>
bool remove_record(std::vector<Record>& records, const std::string& key) {
  const auto it = std::find_if(records.begin(), records.end(), [&](const Record& record) {
    return record.id == key;
  });
  if (it == records.end()) {
    return false;
  }
  records.erase(it);
  return true;
}

template <typename Record>
void insert_sorted(std::vector<Record>& records, Record record) {
  const auto it = std::lower_bound(
    records.begin(),
    records.end(),
    record,
    [](const Record& lhs, const Record& rhs) { return lhs.id < rhs.id; });
  records.insert(it, std::move(record));
}

bool decode_node(const JsonValue& object, SnapshotNode& node, std::string& error) {
  node.id = json_detail::require_string(object, "id", error);
  node.kind = json_detail::require_string(object, "kind", error);
  node.role = json_detail::require_string(object, "role", error);
  node.anchors = json_detail::require_string_array(object, "anchors", error);
  node.evidence = json_detail::require_string(object, "evidence", error);
  return error.empty();
}

bool decode_edge(const JsonValue& object, SnapshotEdge& edge, std::string& error) {
  edge.id = json_detail::require_string(object, "id", error);
  edge.kind = json_detail::require_string(object, "kind", error);
  edge.from = json_detail::require_string(object, "from", error);
  edge.to = json_detail::require_string(object, "to", error);
  edge.evidence = json_detail::require_string(object, "evidence", error);
  return error.empty();
}

bool decode_fact(const JsonValue& object, SnapshotFact& fact, std::string& error) {
  fact.id = json_detail::require_string(object, "id", error);
  fact.subject = json_detail::require_string(object, "subject", error);
  fact.predicate = json_detail::require_string(object, "predicate", error);
  const JsonValue* value = object.field("value");
  if (value == nullptr) {
    error = "fact value is missing";
    return false;
  }
  if (value->kind == JsonValue::Kind::Array) {
    fact.value_is_array = true;
    if (!decode_string_array(*value, fact.array_value)) {
      error = "fact array value must contain strings";
      return false;
    }
  } else if (value->kind == JsonValue::Kind::String) {
    fact.string_value = value->string_value;
  } else {
    error = "fact value must be a string or string array";
    return false;
  }
  fact.evidence = json_detail::require_string(object, "evidence", error);
  return error.empty();
}

bool decode_diagnostic(const JsonValue& object, SnapshotDiagnostic& diagnostic, std::string& error) {
  diagnostic.id = json_detail::require_string(object, "id", error);
  diagnostic.code = json_detail::require_string(object, "code", error);
  diagnostic.severity = json_detail::require_string(object, "severity", error);
  diagnostic.subject = json_detail::require_string(object, "subject", error);
  diagnostic.evidence = json_detail::require_string(object, "evidence", error);
  return error.empty();
}

bool decode_anchor(const JsonValue& object, SnapshotAnchor& anchor, std::string& error) {
  anchor.id = json_detail::require_string(object, "ref", error);
  anchor.path = json_detail::require_string(object, "path", error);
  anchor.precision = json_detail::require_string(object, "precision", error);
  return error.empty();
}

bool decode_evidence(const JsonValue& object, SnapshotEvidence& evidence, std::string& error) {
  evidence.id = json_detail::require_string(object, "ref", error);
  evidence.producer_rule = json_detail::require_string(object, "producer_rule", error);
  evidence.rule_version = json_detail::require_string(object, "rule_version", error);
  evidence.subjects = json_detail::require_string_array(object, "subjects", error);
  evidence.prerequisites = json_detail::require_string_array(object, "prerequisites", error);
  evidence.anchors = json_detail::require_string_array(object, "anchors", error);
  return error.empty();
}

bool decode_lineage(const JsonValue& object, LineageRecord& lineage, std::string& error) {
  lineage.id = json_detail::require_string(object, "id", error);
  lineage.kind = json_detail::require_string(object, "kind", error);
  lineage.prior = json_detail::require_string_array(object, "prior", error);
  lineage.target = json_detail::require_string_array(object, "target", error);
  lineage.producer_rule = json_detail::require_string(object, "producer_rule", error);
  lineage.rule_version = json_detail::require_string(object, "rule_version", error);
  lineage.evidence = json_detail::require_string_array(object, "evidence", error);
  lineage.completeness = json_detail::require_string(object, "completeness", error);
  return error.empty();
}

// Identity fields participate in the producer-owned identity tuple of a
// record category. They never appear in a `replace_fields` operation: changing
// one is a remove-plus-add. Everything not listed here is rejectable identity
// content for that category.
bool field_is_replaceable(RecordCategory category, std::string_view name) {
  switch (category) {
    case RecordCategory::Metadata:
      return name == "completeness" || name == "producer_name" || name == "producer_version"
        || name == "root" || name == "capabilities" || name == "package_name"
        || name == "manifest_path" || name == "entry_path" || name == "parent_snapshot_id";
    case RecordCategory::Nodes:
      return name == "kind" || name == "role" || name == "anchors" || name == "evidence";
    case RecordCategory::Edges:
      return name == "evidence";
    case RecordCategory::Facts:
      return name == "value" || name == "evidence";
    case RecordCategory::Diagnostics:
      return name == "code" || name == "severity" || name == "evidence";
    case RecordCategory::Anchors:
      return name == "precision";
    case RecordCategory::Evidence:
      return false;
    case RecordCategory::Lineage:
      return name == "completeness";
  }
  return false;
}

bool apply_named_field(
  std::vector<NamedField>& current,
  const FieldReplacement& field,
  std::string& error
) {
  for (auto& entry : current) {
    if (entry.name != field.name) {
      continue;
    }
    if (entry.json != field.before) {
      error = "field before-value mismatch: " + field.name;
      return false;
    }
    entry.json = field.after;
    return true;
  }
  error = "unknown field: " + field.name;
  return false;
}

bool apply_node_fields(SnapshotNode& node, const std::vector<FieldReplacement>& fields, std::string& error) {
  auto current = node_fields(node);
  for (const auto& field : fields) {
    if (!apply_named_field(current, field, error)) {
      return false;
    }
  }
  JsonValue parsed;
  for (const auto& field : current) {
    if (!json_detail::parse_json(field.json, parsed, error)) {
      return false;
    }
    if (field.name == "kind") {
      node.kind = parsed.string_value;
    } else if (field.name == "role") {
      node.role = parsed.string_value;
    } else if (field.name == "anchors") {
      decode_string_array(parsed, node.anchors);
    } else if (field.name == "evidence") {
      node.evidence = parsed.string_value;
    }
  }
  return true;
}

bool apply_edge_fields(SnapshotEdge& edge, const std::vector<FieldReplacement>& fields, std::string& error) {
  auto current = edge_fields(edge);
  for (const auto& field : fields) {
    if (!apply_named_field(current, field, error)) {
      return false;
    }
  }
  JsonValue parsed;
  for (const auto& field : current) {
    if (!json_detail::parse_json(field.json, parsed, error)) {
      return false;
    }
    if (field.name == "kind") {
      edge.kind = parsed.string_value;
    } else if (field.name == "from") {
      edge.from = parsed.string_value;
    } else if (field.name == "to") {
      edge.to = parsed.string_value;
    } else if (field.name == "evidence") {
      edge.evidence = parsed.string_value;
    }
  }
  return true;
}

bool apply_fact_fields(SnapshotFact& fact, const std::vector<FieldReplacement>& fields, std::string& error) {
  auto current = fact_fields(fact);
  for (const auto& field : fields) {
    if (!apply_named_field(current, field, error)) {
      return false;
    }
  }
  JsonValue parsed;
  for (const auto& field : current) {
    if (!json_detail::parse_json(field.json, parsed, error)) {
      return false;
    }
    if (field.name == "subject") {
      fact.subject = parsed.string_value;
    } else if (field.name == "predicate") {
      fact.predicate = parsed.string_value;
    } else if (field.name == "value") {
      if (parsed.kind == JsonValue::Kind::Array) {
        fact.value_is_array = true;
        decode_string_array(parsed, fact.array_value);
        fact.string_value.clear();
      } else {
        fact.value_is_array = false;
        fact.array_value.clear();
        fact.string_value = parsed.string_value;
      }
    } else if (field.name == "evidence") {
      fact.evidence = parsed.string_value;
    }
  }
  return true;
}

bool apply_diagnostic_fields(
  SnapshotDiagnostic& diagnostic,
  const std::vector<FieldReplacement>& fields,
  std::string& error
) {
  auto current = diagnostic_fields(diagnostic);
  for (const auto& field : fields) {
    if (!apply_named_field(current, field, error)) {
      return false;
    }
  }
  JsonValue parsed;
  for (const auto& field : current) {
    if (!json_detail::parse_json(field.json, parsed, error)) {
      return false;
    }
    if (field.name == "code") {
      diagnostic.code = parsed.string_value;
    } else if (field.name == "severity") {
      diagnostic.severity = parsed.string_value;
    } else if (field.name == "subject") {
      diagnostic.subject = parsed.string_value;
    } else if (field.name == "evidence") {
      diagnostic.evidence = parsed.string_value;
    }
  }
  return true;
}

bool apply_anchor_fields(SnapshotAnchor& anchor, const std::vector<FieldReplacement>& fields, std::string& error) {
  auto current = anchor_fields(anchor);
  for (const auto& field : fields) {
    if (!apply_named_field(current, field, error)) {
      return false;
    }
  }
  JsonValue parsed;
  for (const auto& field : current) {
    if (!json_detail::parse_json(field.json, parsed, error)) {
      return false;
    }
    if (field.name == "path") {
      anchor.path = parsed.string_value;
    } else if (field.name == "precision") {
      anchor.precision = parsed.string_value;
    }
  }
  return true;
}

bool apply_evidence_fields(
  SnapshotEvidence& evidence,
  const std::vector<FieldReplacement>& fields,
  std::string& error
) {
  auto current = evidence_fields(evidence);
  for (const auto& field : fields) {
    if (!apply_named_field(current, field, error)) {
      return false;
    }
  }
  JsonValue parsed;
  for (const auto& field : current) {
    if (!json_detail::parse_json(field.json, parsed, error)) {
      return false;
    }
    if (field.name == "producer_rule") {
      evidence.producer_rule = parsed.string_value;
    } else if (field.name == "rule_version") {
      evidence.rule_version = parsed.string_value;
    } else if (field.name == "subjects") {
      decode_string_array(parsed, evidence.subjects);
    } else if (field.name == "prerequisites") {
      decode_string_array(parsed, evidence.prerequisites);
    } else if (field.name == "anchors") {
      decode_string_array(parsed, evidence.anchors);
    }
  }
  return true;
}

bool apply_lineage_fields(LineageRecord& lineage, const std::vector<FieldReplacement>& fields, std::string& error) {
  auto current = lineage_fields(lineage);
  for (const auto& field : fields) {
    if (!apply_named_field(current, field, error)) {
      return false;
    }
  }
  JsonValue parsed;
  for (const auto& field : current) {
    if (!json_detail::parse_json(field.json, parsed, error)) {
      return false;
    }
    if (field.name == "kind") {
      lineage.kind = parsed.string_value;
    } else if (field.name == "prior") {
      decode_string_array(parsed, lineage.prior);
    } else if (field.name == "target") {
      decode_string_array(parsed, lineage.target);
    } else if (field.name == "producer_rule") {
      lineage.producer_rule = parsed.string_value;
    } else if (field.name == "rule_version") {
      lineage.rule_version = parsed.string_value;
    } else if (field.name == "evidence") {
      decode_string_array(parsed, lineage.evidence);
    } else if (field.name == "completeness") {
      lineage.completeness = parsed.string_value;
    }
  }
  return true;
}

bool lineage_cardinality_ok(const LineageDraft& draft) {
  if (draft.kind == "rename" || draft.kind == "move") {
    return draft.prior.size() == 1 && draft.target.size() == 1;
  }
  if (draft.kind == "split") {
    return draft.prior.size() == 1 && draft.target.size() >= 2;
  }
  if (draft.kind == "merge") {
    return draft.prior.size() >= 2 && draft.target.size() == 1;
  }
  return false;
}

} // namespace

std::string_view category_name(RecordCategory category) noexcept {
  switch (category) {
    case RecordCategory::Metadata: return "metadata";
    case RecordCategory::Nodes: return "nodes";
    case RecordCategory::Edges: return "edges";
    case RecordCategory::Facts: return "facts";
    case RecordCategory::Diagnostics: return "diagnostics";
    case RecordCategory::Anchors: return "anchors";
    case RecordCategory::Evidence: return "evidence";
    case RecordCategory::Lineage: return "lineage";
  }
  return "unknown";
}

std::optional<RecordCategory> parse_category(std::string_view name) {
  if (name == "metadata") return RecordCategory::Metadata;
  if (name == "nodes") return RecordCategory::Nodes;
  if (name == "edges") return RecordCategory::Edges;
  if (name == "facts") return RecordCategory::Facts;
  if (name == "diagnostics") return RecordCategory::Diagnostics;
  if (name == "anchors") return RecordCategory::Anchors;
  if (name == "evidence") return RecordCategory::Evidence;
  if (name == "lineage") return RecordCategory::Lineage;
  return std::nullopt;
}

std::string_view op_name(DeltaOpKind kind) noexcept {
  switch (kind) {
    case DeltaOpKind::Add: return "add";
    case DeltaOpKind::Remove: return "remove";
    case DeltaOpKind::ReplaceFields: return "replace_fields";
  }
  return "unknown";
}

std::string serialize_delta(const TopologyDelta& delta) {
  CompactJson json;
  json.begin_object();
  json.key("contract");
  json.string_value(delta.contract);
  json.key("schema_version");
  json.begin_object();
  json.key("major");
  json.integer_value(delta.schema_major);
  json.key("minor");
  json.integer_value(delta.schema_minor);
  json.end_object();
  json.key("stability");
  json.string_value(delta.stability);
  json.key("parent_snapshot_id");
  json.string_value(delta.parent_snapshot_id);
  json.key("target_snapshot_id");
  json.string_value(delta.target_snapshot_id);
  json.key("required_capabilities");
  emit_string_array(json, delta.required_capabilities);
  json.key("optional_capabilities");
  emit_string_array(json, delta.optional_capabilities);
  json.key("operations");
  json.begin_array();
  for (const auto& op : delta.operations) {
    json.begin_object();
    json.key("op");
    json.string_value(op_name(op.kind));
    json.key("category");
    json.string_value(category_name(op.category));
    json.key("key");
    json.string_value(op.key);
    if (op.kind == DeltaOpKind::Add) {
      json.key("record");
      json.raw_value(op.record_json);
    } else if (op.kind == DeltaOpKind::ReplaceFields) {
      json.key("fields");
      json.begin_array();
      for (const auto& field : op.fields) {
        json.begin_object();
        json.key("name");
        json.string_value(field.name);
        json.key("before");
        json.raw_value(field.before);
        json.key("after");
        json.raw_value(field.after);
        json.end_object();
      }
      json.end_array();
    }
    json.end_object();
  }
  json.end_array();
  json.end_object();
  json.buf.push_back('\n');
  return json.buf;
}

DeltaIssue parse_delta(std::string_view json) {
  DeltaIssue result;
  JsonValue root;
  std::string error;
  if (!json_detail::parse_json(json, root, error) || !root.is_object()) {
    result.error = error.empty() ? "delta JSON must be an object" : error;
    return result;
  }
  TopologyDelta delta;
  delta.contract = json_detail::require_string(root, "contract", error);
  const JsonValue* schema = root.field("schema_version");
  if (schema == nullptr || !schema->is_object()) {
    result.error = "delta schema_version must be an object";
    return result;
  }
  const JsonValue* major = schema->field("major");
  const JsonValue* minor = schema->field("minor");
  if (major == nullptr || minor == nullptr || major->kind != JsonValue::Kind::Int
      || minor->kind != JsonValue::Kind::Int) {
    result.error = "delta schema_version major and minor must be integers";
    return result;
  }
  delta.schema_major = static_cast<int>(major->int_value);
  delta.schema_minor = static_cast<int>(minor->int_value);
  delta.stability = json_detail::require_string(root, "stability", error);
  delta.parent_snapshot_id = json_detail::require_string(root, "parent_snapshot_id", error);
  delta.target_snapshot_id = json_detail::require_string(root, "target_snapshot_id", error);
  delta.required_capabilities = json_detail::require_string_array(root, "required_capabilities", error);
  delta.optional_capabilities = json_detail::require_string_array(root, "optional_capabilities", error);
  const JsonValue* operations = root.field("operations");
  if (operations == nullptr || !operations->is_array()) {
    result.error = "delta operations must be an array";
    return result;
  }
  for (const auto& item : operations->array_value) {
    if (!item.is_object()) {
      result.error = "delta operation must be an object";
      result.reason = std::string(kReasonMalformedOperation);
      return result;
    }
    DeltaOperation op;
    const std::string kind = json_detail::require_string(item, "op", error);
    if (kind == "add") {
      op.kind = DeltaOpKind::Add;
    } else if (kind == "remove") {
      op.kind = DeltaOpKind::Remove;
    } else if (kind == "replace_fields") {
      op.kind = DeltaOpKind::ReplaceFields;
    } else {
      result.error = "unsupported delta operation";
      result.reason = std::string(kReasonMalformedOperation);
      return result;
    }
    const auto category = parse_category(json_detail::require_string(item, "category", error));
    if (!category.has_value()) {
      result.error = "unknown delta category";
      result.reason = std::string(kReasonMalformedOperation);
      return result;
    }
    op.category = *category;
    op.key = json_detail::require_string(item, "key", error);
    if (op.kind == DeltaOpKind::Add) {
      const JsonValue* record = item.field("record");
      if (record == nullptr) {
        result.error = "add operation is missing record";
        result.reason = std::string(kReasonMalformedOperation);
        return result;
      }
      op.record_json = field_as_json(*record);
    } else if (op.kind == DeltaOpKind::ReplaceFields) {
      const JsonValue* fields = item.field("fields");
      if (fields == nullptr || !fields->is_array()) {
        result.error = "replace_fields is missing fields";
        result.reason = std::string(kReasonMalformedOperation);
        return result;
      }
      for (const auto& field : fields->array_value) {
        if (!field.is_object()) {
          result.error = "field replacement must be an object";
          result.reason = std::string(kReasonMalformedOperation);
          return result;
        }
        FieldReplacement replacement;
        replacement.name = json_detail::require_string(field, "name", error);
        const JsonValue* before = field.field("before");
        const JsonValue* after = field.field("after");
        if (before == nullptr || after == nullptr) {
          result.error = "field replacement is missing before or after";
          result.reason = std::string(kReasonMalformedOperation);
          return result;
        }
        replacement.before = field_as_json(*before);
        replacement.after = field_as_json(*after);
        op.fields.push_back(std::move(replacement));
      }
    }
    delta.operations.push_back(std::move(op));
  }
  if (!error.empty()) {
    result.error = error;
    return result;
  }
  result.ok = true;
  result.delta = std::move(delta);
  result.json = serialize_delta(result.delta);
  return result;
}

DeltaIssue generate_delta(const Snapshot& parent, const Snapshot& child) {
  DeltaIssue result;
  const auto parent_final = finalize_snapshot(parent);
  const auto child_final = finalize_snapshot(child);
  if (!parent_final.ok) {
    result.error = parent_final.error;
    return result;
  }
  if (!child_final.ok) {
    result.error = child_final.error;
    return result;
  }

  TopologyDelta delta;
  delta.parent_snapshot_id = parent_final.snapshot_id;
  delta.target_snapshot_id = child_final.snapshot_id;
  delta.required_capabilities = child_final.snapshot.capabilities;
  delta.optional_capabilities = {std::string(kDeltaCapability)};
  if (!child_final.snapshot.lineage.empty()) {
    delta.optional_capabilities.emplace_back(kLineageCapability);
  }

  append_replacements(
    delta.operations,
    RecordCategory::Metadata,
    metadata_key(),
    metadata_fields(parent_final.snapshot),
    metadata_fields(child_final.snapshot));
  merge_category(
    delta.operations,
    RecordCategory::Nodes,
    parent_final.snapshot.nodes,
    child_final.snapshot.nodes,
    node_fields,
    node_json);
  merge_category(
    delta.operations,
    RecordCategory::Edges,
    parent_final.snapshot.edges,
    child_final.snapshot.edges,
    edge_fields,
    edge_json);
  merge_category(
    delta.operations,
    RecordCategory::Facts,
    parent_final.snapshot.facts,
    child_final.snapshot.facts,
    fact_fields,
    fact_json);
  merge_category(
    delta.operations,
    RecordCategory::Diagnostics,
    parent_final.snapshot.diagnostics,
    child_final.snapshot.diagnostics,
    diagnostic_fields,
    diagnostic_json);
  merge_category(
    delta.operations,
    RecordCategory::Anchors,
    parent_final.snapshot.anchors,
    child_final.snapshot.anchors,
    anchor_fields,
    anchor_json);
  merge_category(
    delta.operations,
    RecordCategory::Evidence,
    parent_final.snapshot.evidence,
    child_final.snapshot.evidence,
    evidence_fields,
    evidence_json);
  merge_category(
    delta.operations,
    RecordCategory::Lineage,
    parent_final.snapshot.lineage,
    child_final.snapshot.lineage,
    lineage_fields,
    lineage_json);

  result.ok = true;
  result.delta = std::move(delta);
  result.json = serialize_delta(result.delta);
  result.snapshot = child_final.snapshot;
  result.snapshot_id = child_final.snapshot_id;
  return result;
}

DeltaIssue apply_delta(const Snapshot& parent, const TopologyDelta& delta) {
  DeltaIssue result;
  const auto parent_final = finalize_snapshot(parent);
  if (!parent_final.ok) {
    result.error = parent_final.error;
    return result;
  }
  if (parent_final.snapshot_id != delta.parent_snapshot_id) {
    result.error = "delta parent_snapshot_id does not match the supplied snapshot";
    result.reason = std::string(kReasonWrongBase);
    return result;
  }
  if (delta.schema_major != kDeltaSchemaMajor) {
    result.error = "unsupported delta schema major version";
    result.reason = std::string(kReasonUnsupported);
    return result;
  }
  for (const auto& capability : delta.required_capabilities) {
    if (!observable_capability_is_supported(capability)) {
      result.error = "unknown required capability: " + capability;
      result.reason = std::string(kReasonUnknownRequiredCapability);
      return result;
    }
  }
  for (std::size_t i = 0; i < delta.operations.size(); ++i) {
    const auto& op = delta.operations[i];
    if (i > 0) {
      const auto& previous = delta.operations[i - 1];
      const bool ordered = static_cast<int>(previous.category) < static_cast<int>(op.category)
        || (previous.category == op.category && previous.key < op.key);
      if (!ordered) {
        result.error = "delta operations are not in canonical sorted unique order";
        result.reason = std::string(kReasonMalformedOperation);
        return result;
      }
    }
    for (const auto& field : op.fields) {
      if (op.kind == DeltaOpKind::ReplaceFields && !field_is_replaceable(op.category, field.name)) {
        result.error = "identity field must change via remove plus add: " + field.name;
        result.reason = std::string(kReasonMalformedOperation);
        return result;
      }
    }
  }

  Snapshot working = parent_final.snapshot;
  std::string error;
  for (const auto& op : delta.operations) {
    if (op.category == RecordCategory::Metadata) {
      if (op.kind != DeltaOpKind::ReplaceFields || op.key != metadata_key()) {
        result.error = "metadata operations must replace fields on the snapshot key";
        result.reason = std::string(kReasonMalformedOperation);
        return result;
      }
      for (const auto& field : op.fields) {
        auto current = metadata_fields(working);
        if (!apply_named_field(current, field, error) || !apply_metadata_field(working, field, error)) {
          result.error = error.empty() ? "metadata field application failed" : error;
          result.reason = std::string(kReasonMalformedOperation);
          return result;
        }
      }
      continue;
    }

    if (op.kind == DeltaOpKind::Remove) {
      bool removed = false;
      switch (op.category) {
        case RecordCategory::Nodes: removed = remove_record(working.nodes, op.key); break;
        case RecordCategory::Edges: removed = remove_record(working.edges, op.key); break;
        case RecordCategory::Facts: removed = remove_record(working.facts, op.key); break;
        case RecordCategory::Diagnostics: removed = remove_record(working.diagnostics, op.key); break;
        case RecordCategory::Anchors: removed = remove_record(working.anchors, op.key); break;
        case RecordCategory::Evidence: removed = remove_record(working.evidence, op.key); break;
        case RecordCategory::Lineage: removed = remove_record(working.lineage, op.key); break;
        case RecordCategory::Metadata: break;
      }
      if (!removed) {
        result.error = "remove operation key does not exist: " + op.key;
        result.reason = std::string(kReasonMalformedOperation);
        return result;
      }
      continue;
    }

    if (op.kind == DeltaOpKind::Add) {
      JsonValue record = parse_required(op.record_json, error);
      if (!record.is_object()) {
        result.error = error.empty() ? "add record is not an object" : error;
        result.reason = std::string(kReasonMalformedOperation);
        return result;
      }
      switch (op.category) {
        case RecordCategory::Nodes: {
          SnapshotNode node;
          if (!decode_node(record, node, error) || node.id != op.key) {
            result.error = error.empty() ? "add node key mismatch" : error;
            result.reason = std::string(kReasonMalformedOperation);
            return result;
          }
          insert_sorted(working.nodes, std::move(node));
          break;
        }
        case RecordCategory::Edges: {
          SnapshotEdge edge;
          if (!decode_edge(record, edge, error) || edge.id != op.key) {
            result.error = error.empty() ? "add edge key mismatch" : error;
            result.reason = std::string(kReasonMalformedOperation);
            return result;
          }
          insert_sorted(working.edges, std::move(edge));
          break;
        }
        case RecordCategory::Facts: {
          SnapshotFact fact;
          if (!decode_fact(record, fact, error) || fact.id != op.key) {
            result.error = error.empty() ? "add fact key mismatch" : error;
            result.reason = std::string(kReasonMalformedOperation);
            return result;
          }
          insert_sorted(working.facts, std::move(fact));
          break;
        }
        case RecordCategory::Diagnostics: {
          SnapshotDiagnostic diagnostic;
          if (!decode_diagnostic(record, diagnostic, error) || diagnostic.id != op.key) {
            result.error = error.empty() ? "add diagnostic key mismatch" : error;
            result.reason = std::string(kReasonMalformedOperation);
            return result;
          }
          insert_sorted(working.diagnostics, std::move(diagnostic));
          break;
        }
        case RecordCategory::Anchors: {
          SnapshotAnchor anchor;
          if (!decode_anchor(record, anchor, error) || anchor.id != op.key) {
            result.error = error.empty() ? "add anchor key mismatch" : error;
            result.reason = std::string(kReasonMalformedOperation);
            return result;
          }
          insert_sorted(working.anchors, std::move(anchor));
          break;
        }
        case RecordCategory::Evidence: {
          SnapshotEvidence evidence;
          if (!decode_evidence(record, evidence, error) || evidence.id != op.key) {
            result.error = error.empty() ? "add evidence key mismatch" : error;
            result.reason = std::string(kReasonMalformedOperation);
            return result;
          }
          insert_sorted(working.evidence, std::move(evidence));
          break;
        }
        case RecordCategory::Lineage: {
          LineageRecord lineage;
          if (!decode_lineage(record, lineage, error) || lineage.id != op.key) {
            result.error = error.empty() ? "add lineage key mismatch" : error;
            result.reason = std::string(kReasonMalformedOperation);
            return result;
          }
          insert_sorted(working.lineage, std::move(lineage));
          break;
        }
        case RecordCategory::Metadata:
          break;
      }
      continue;
    }

    bool applied = false;
    switch (op.category) {
      case RecordCategory::Nodes: {
        auto* node = find_mutable(working.nodes, op.key);
        applied = node != nullptr && apply_node_fields(*node, op.fields, error);
        break;
      }
      case RecordCategory::Edges: {
        auto* edge = find_mutable(working.edges, op.key);
        applied = edge != nullptr && apply_edge_fields(*edge, op.fields, error);
        break;
      }
      case RecordCategory::Facts: {
        auto* fact = find_mutable(working.facts, op.key);
        applied = fact != nullptr && apply_fact_fields(*fact, op.fields, error);
        break;
      }
      case RecordCategory::Diagnostics: {
        auto* diagnostic = find_mutable(working.diagnostics, op.key);
        applied = diagnostic != nullptr && apply_diagnostic_fields(*diagnostic, op.fields, error);
        break;
      }
      case RecordCategory::Anchors: {
        auto* anchor = find_mutable(working.anchors, op.key);
        applied = anchor != nullptr && apply_anchor_fields(*anchor, op.fields, error);
        break;
      }
      case RecordCategory::Evidence: {
        auto* evidence = find_mutable(working.evidence, op.key);
        applied = evidence != nullptr && apply_evidence_fields(*evidence, op.fields, error);
        break;
      }
      case RecordCategory::Lineage: {
        auto* lineage = find_mutable(working.lineage, op.key);
        applied = lineage != nullptr && apply_lineage_fields(*lineage, op.fields, error);
        break;
      }
      case RecordCategory::Metadata:
        break;
    }
    if (!applied) {
      result.error = error.empty() ? "replace_fields could not be applied" : error;
      result.reason = std::string(kReasonMalformedOperation);
      return result;
    }
  }

  auto finished = finalize_snapshot(std::move(working));
  if (!finished.ok) {
    result.error = finished.error;
    return result;
  }
  if (finished.snapshot_id != delta.target_snapshot_id) {
    result.error = "applied delta does not reconstruct the declared target snapshot";
    result.reason = std::string(kReasonInvalid);
    return result;
  }
  result.ok = true;
  result.delta = delta;
  result.json = serialize_delta(delta);
  result.snapshot = std::move(finished.snapshot);
  result.snapshot_id = finished.snapshot_id;
  return result;
}

std::vector<std::string> delta_seed_keys(const TopologyDelta& delta) {
  std::vector<std::string> keys;
  keys.reserve(delta.operations.size());
  for (const auto& op : delta.operations) {
    if (op.category == RecordCategory::Metadata) {
      keys.push_back(metadata_key());
    } else {
      keys.push_back(op.key);
    }
  }
  std::sort(keys.begin(), keys.end());
  keys.erase(std::unique(keys.begin(), keys.end()), keys.end());
  return keys;
}

bool producer_rule_is_canonical(std::string_view rule) noexcept {
  return rule.find("styio.sema.") == 0 || rule.find("styio.observable.") == 0;
}

LineageIssue construct_lineage(
  const Snapshot& parent,
  const Snapshot& child,
  const std::vector<LineageDraft>& drafts
) {
  LineageIssue result;
  const auto parent_final = finalize_snapshot(parent);
  const auto child_final = finalize_snapshot(child);
  if (!parent_final.ok) {
    result.error = parent_final.error;
    return result;
  }
  if (!child_final.ok) {
    result.error = child_final.error;
    return result;
  }

  for (const auto& draft : drafts) {
    if (!producer_rule_is_canonical(draft.producer_rule)) {
      result.error = "lineage requires a compiler producer rule";
      return result;
    }
    if (draft.rule_version.empty()) {
      result.error = "lineage rule_version is required";
      return result;
    }
    if (!lineage_cardinality_ok(draft)) {
      result.error = "lineage relation cardinality is invalid for kind " + draft.kind;
      return result;
    }
    for (const auto& prior : draft.prior) {
      if (!record_exists(parent_final.snapshot, prior)) {
        result.error = "lineage prior subject is not in the parent snapshot";
        return result;
      }
    }
    for (const auto& target : draft.target) {
      if (!record_exists(child_final.snapshot, target)) {
        result.error = "lineage target subject is not in the child snapshot";
        return result;
      }
    }
    if (draft.evidence.empty()) {
      result.error = "lineage evidence is required";
      return result;
    }
    for (const auto& evidence : draft.evidence) {
      if (find_evidence(child_final.snapshot, evidence) == nullptr
          && find_evidence(parent_final.snapshot, evidence) == nullptr) {
        result.error = "lineage evidence does not resolve";
        return result;
      }
    }

    std::vector<std::string> identity{
      draft.kind,
      draft.producer_rule,
      draft.rule_version,
    };
    identity.insert(identity.end(), draft.prior.begin(), draft.prior.end());
    identity.insert(identity.end(), draft.target.begin(), draft.target.end());
    identity.insert(identity.end(), draft.evidence.begin(), draft.evidence.end());
    std::string preimage;
    for (const auto& field : identity) {
      preimage.push_back(static_cast<char>(field.size() >> 8));
      preimage.push_back(static_cast<char>(field.size() & 0xff));
      preimage += field;
    }
    LineageRecord record;
    record.id = std::string(kLineageIdPrefix) + json_detail::sha256_hex16(preimage);
    record.kind = draft.kind;
    record.prior = draft.prior;
    record.target = draft.target;
    record.producer_rule = draft.producer_rule;
    record.rule_version = draft.rule_version;
    record.evidence = draft.evidence;
    record.completeness = draft.completeness.empty() ? "complete" : draft.completeness;
    result.records.push_back(std::move(record));
  }

  std::sort(result.records.begin(), result.records.end(), [](const auto& lhs, const auto& rhs) {
    return lhs.id < rhs.id;
  });
  result.ok = true;
  return result;
}

} // namespace styio::observable
