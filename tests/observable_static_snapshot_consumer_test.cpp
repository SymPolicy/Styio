#include <gtest/gtest.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "llvm/Support/JSON.h"

#ifndef STYIO_SOURCE_DIR
#define STYIO_SOURCE_DIR "."
#endif

namespace fs = std::filesystem;

namespace {

constexpr std::array<std::string_view, 5> kRequiredCapabilities{
  "file-source-anchors",
  "producer-evidence",
  "static-topology-edges",
  "static-topology-facts",
  "static-topology-nodes",
};

std::string read_text(const fs::path& path) {
  std::ifstream in(path, std::ios::binary);
  std::ostringstream out;
  out << in.rdbuf();
  return out.str();
}

fs::path fixture_root() {
  return fs::path(STYIO_SOURCE_DIR) / "tests/fixtures/observable_static_snapshot/v1";
}

struct ConsumerError {
  bool ok = false;
  std::string message;
};

bool evidence_acyclic(const llvm::json::Array& evidence, std::string& error) {
  std::unordered_map<std::string, std::size_t> index;
  for (std::size_t i = 0; i < evidence.size(); ++i) {
    const auto* object = evidence[i].getAsObject();
    if (object == nullptr) {
      error = "evidence record must be an object";
      return false;
    }
    const auto ref = object->getString("ref");
    if (!ref.has_value()) {
      error = "evidence record is missing ref";
      return false;
    }
    index.emplace(std::string(*ref), i);
  }
  std::vector<int> indegree(evidence.size(), 0);
  std::vector<std::vector<std::size_t>> adj(evidence.size());
  for (std::size_t i = 0; i < evidence.size(); ++i) {
    const auto* object = evidence[i].getAsObject();
    const auto* prerequisites = object->getArray("prerequisites");
    if (prerequisites == nullptr) {
      continue;
    }
    for (const auto& value : *prerequisites) {
      const auto raw = value.getAsString();
      if (!raw.has_value()) {
        error = "evidence prerequisite must be a string";
        return false;
      }
      const auto it = index.find(std::string(*raw));
      if (it == index.end()) {
        error = "evidence prerequisite does not resolve";
        return false;
      }
      adj[it->second].push_back(i);
      indegree[i] += 1;
    }
  }
  std::vector<std::size_t> ready;
  for (std::size_t i = 0; i < evidence.size(); ++i) {
    if (indegree[i] == 0) {
      ready.push_back(i);
    }
  }
  std::size_t seen = 0;
  for (std::size_t cursor = 0; cursor < ready.size(); ++cursor) {
    ++seen;
    for (const std::size_t next : adj[ready[cursor]]) {
      indegree[next] -= 1;
      if (indegree[next] == 0) {
        ready.push_back(next);
      }
    }
  }
  if (seen != evidence.size()) {
    error = "evidence graph contains a cycle";
    return false;
  }
  return true;
}

ConsumerError consume(const std::string& json) {
  ConsumerError result;
  llvm::Expected<llvm::json::Value> parsed = llvm::json::parse(json);
  if (!parsed) {
    result.message = "snapshot is not valid JSON";
    return result;
  }
  const auto* root = parsed->getAsObject();
  if (root == nullptr) {
    result.message = "snapshot must be an object";
    return result;
  }
  const auto schema = root->getInteger("schema_version");
  if (!schema.has_value() || *schema != 1) {
    result.message = "unsupported snapshot schema_version";
    return result;
  }
  const auto completeness = root->getString("completeness");
  if (!completeness.has_value()
      || (*completeness != "complete/validated-topology"
          && *completeness != "complete/proven-scalar-noop")) {
    result.message = "unsupported snapshot completeness";
    return result;
  }
  const auto* capabilities = root->getArray("capabilities");
  if (capabilities == nullptr) {
    result.message = "snapshot capabilities must be an array";
    return result;
  }
  std::unordered_set<std::string> advertised;
  for (const auto& value : *capabilities) {
    const auto raw = value.getAsString();
    if (raw.has_value()) {
      advertised.insert(std::string(*raw));
    }
  }
  for (const std::string_view required : kRequiredCapabilities) {
    if (advertised.count(std::string(required)) == 0) {
      result.message = "snapshot is missing required capability";
      return result;
    }
  }

  const auto* nodes = root->getArray("nodes");
  const auto* edges = root->getArray("edges");
  const auto* facts = root->getArray("facts");
  const auto* anchors = root->getArray("anchors");
  const auto* evidence = root->getArray("evidence");
  if (nodes == nullptr || edges == nullptr || facts == nullptr
      || anchors == nullptr || evidence == nullptr) {
    result.message = "snapshot collections must be arrays";
    return result;
  }

  std::unordered_set<std::string> node_ids;
  std::unordered_set<std::string> edge_ids;
  std::unordered_set<std::string> fact_ids;
  std::unordered_set<std::string> anchor_ids;
  for (const auto& value : *nodes) {
    const auto* object = value.getAsObject();
    if (object == nullptr || !object->getString("id").has_value()) {
      result.message = "node record is invalid";
      return result;
    }
    node_ids.insert(std::string(*object->getString("id")));
  }
  for (const auto& value : *edges) {
    const auto* object = value.getAsObject();
    if (object == nullptr) {
      result.message = "edge record is invalid";
      return result;
    }
    const auto from = object->getString("from");
    const auto to = object->getString("to");
    const auto id = object->getString("id");
    if (!from.has_value() || !to.has_value() || !id.has_value()) {
      result.message = "edge record is missing endpoints";
      return result;
    }
    if (!node_ids.count(std::string(*from)) || !node_ids.count(std::string(*to))) {
      result.message = "edge endpoint does not resolve";
      return result;
    }
    edge_ids.insert(std::string(*id));
  }
  if (edge_ids.size() != edges->size()) {
    result.message = "edge identities are not unique";
    return result;
  }
  for (const auto& value : *facts) {
    const auto* object = value.getAsObject();
    if (object == nullptr || !object->getString("id").has_value()
        || !object->getString("subject").has_value()) {
      result.message = "fact record is invalid";
      return result;
    }
    if (!node_ids.count(std::string(*object->getString("subject")))) {
      result.message = "fact subject does not resolve";
      return result;
    }
    fact_ids.insert(std::string(*object->getString("id")));
  }
  if (fact_ids.size() != facts->size()) {
    result.message = "fact identities are not unique";
    return result;
  }
  for (const auto& value : *anchors) {
    const auto* object = value.getAsObject();
    if (object == nullptr || !object->getString("ref").has_value()) {
      result.message = "anchor record is invalid";
      return result;
    }
    anchor_ids.insert(std::string(*object->getString("ref")));
  }
  if (anchor_ids.size() != anchors->size()) {
    result.message = "anchor identities are not unique";
    return result;
  }
  const auto* root_value = root->get("root");
  if (root_value == nullptr) {
    result.message = "snapshot root is missing";
    return result;
  }
  if (root_value->kind() != llvm::json::Value::Null) {
    const auto root_id = root_value->getAsString();
    if (!root_id.has_value() || !node_ids.count(std::string(*root_id))) {
      result.message = "snapshot root does not resolve";
      return result;
    }
  }
  if (!evidence_acyclic(*evidence, result.message)) {
    return result;
  }
  result.ok = true;
  return result;
}

} // namespace

TEST(StyioObservableStaticSnapshotConsumer, CanonicalGoldenIsAccepted) {
  const fs::path golden = fixture_root() / "canonical.json";
  if (!fs::exists(golden)) {
    GTEST_SKIP() << "canonical.json is produced by the producer suite";
  }
  const ConsumerError result = consume(read_text(golden));
  EXPECT_TRUE(result.ok) << result.message;
}

TEST(StyioObservableStaticSnapshotConsumer, ScalarNoopFixtureIsAccepted) {
  const ConsumerError result = consume(read_text(fixture_root() / "proven-scalar-noop.json"));
  EXPECT_TRUE(result.ok) << result.message;
}

TEST(StyioObservableStaticSnapshotConsumer, AdditiveFieldsAreIgnored) {
  const fs::path additive = fixture_root() / "additive-field.json";
  ASSERT_TRUE(fs::exists(additive));
  const ConsumerError result = consume(read_text(additive));
  EXPECT_TRUE(result.ok) << result.message;
}

TEST(StyioObservableStaticSnapshotConsumer, UnsupportedSchemaFailsClosed) {
  const ConsumerError result = consume(read_text(fixture_root() / "unsupported-schema.json"));
  EXPECT_FALSE(result.ok);
  EXPECT_NE(result.message.find("schema_version"), std::string::npos);
}

TEST(StyioObservableStaticSnapshotConsumer, UnsupportedCompletenessFailsClosed) {
  const ConsumerError result = consume(read_text(fixture_root() / "unsupported-completeness.json"));
  EXPECT_FALSE(result.ok);
  EXPECT_NE(result.message.find("completeness"), std::string::npos);
}

TEST(StyioObservableStaticSnapshotConsumer, MissingCapabilityFailsClosed) {
  const ConsumerError result = consume(read_text(fixture_root() / "missing-capability.json"));
  EXPECT_FALSE(result.ok);
  EXPECT_NE(result.message.find("capability"), std::string::npos);
}

TEST(StyioObservableStaticSnapshotConsumer, DanglingReferenceFailsClosed) {
  const ConsumerError result = consume(read_text(fixture_root() / "dangling-reference.json"));
  EXPECT_FALSE(result.ok);
  EXPECT_NE(result.message.find("resolve"), std::string::npos);
}

TEST(StyioObservableStaticSnapshotConsumer, CyclicEvidenceFailsClosed) {
  const ConsumerError result = consume(read_text(fixture_root() / "cyclic-evidence.json"));
  EXPECT_FALSE(result.ok);
  EXPECT_NE(result.message.find("cycle"), std::string::npos);
}
