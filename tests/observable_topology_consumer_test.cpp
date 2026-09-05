#include <gtest/gtest.h>

#include "StyioServices/StyioObservable/Delta.hpp"
#include "StyioServices/StyioObservable/Query.hpp"
#include "StyioServices/StyioObservable/Service.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#ifndef STYIO_SOURCE_DIR
#define STYIO_SOURCE_DIR "."
#endif
#ifndef STYIO_OBSERVABLE_CONSUMER_LINK_MAP
#define STYIO_OBSERVABLE_CONSUMER_LINK_MAP ""
#endif

namespace fs = std::filesystem;
namespace obs = styio::observable;

namespace {

std::string read_text(const fs::path& path) {
  std::ifstream in(path, std::ios::binary);
  std::ostringstream out;
  out << in.rdbuf();
  return out.str();
}

fs::path fixture_root() {
  return fs::path(STYIO_SOURCE_DIR) / "tests/fixtures/observable-topology";
}

} // namespace

TEST(StyioObservableConsumer, NegotiatesIndependentContracts) {
  obs::NegotiationRequest compatible;
  compatible.required_capabilities = {"static-topology-nodes", "snapshot-delta"};
  compatible.optional_capabilities = {"bounded-query", "producer-lineage", "not-a-capability"};
  const auto ok = obs::negotiate_observable_contracts(compatible);
  ASSERT_TRUE(ok.ok) << ok.reason;
  EXPECT_EQ(ok.query.minor, obs::kQuerySchemaMinor);
  EXPECT_EQ(ok.delta.minor, obs::kDeltaSchemaMinor);
  bool saw_query = false;
  bool saw_unknown = false;
  for (const auto& capability : ok.capabilities) {
    if (capability == "bounded-query") {
      saw_query = true;
    }
    if (capability == "not-a-capability") {
      saw_unknown = true;
    }
  }
  EXPECT_TRUE(saw_query);
  EXPECT_FALSE(saw_unknown);

  obs::NegotiationRequest major;
  major.delta.major = 1;
  const auto incompatible = obs::negotiate_observable_contracts(major);
  EXPECT_FALSE(incompatible.ok);
  EXPECT_EQ(incompatible.reason, obs::kReasonMajorIncompatible);
}

TEST(StyioObservableConsumer, AppliesAndQueriesCheckedFixtures) {
  const auto parent = obs::parse_snapshot(read_text(fixture_root() / "parent" / "complete.json"));
  const auto child = obs::parse_snapshot(read_text(fixture_root() / "child" / "field-change.json"));
  ASSERT_TRUE(parent.ok) << parent.error;
  ASSERT_TRUE(child.ok) << child.error;
  const auto delta = obs::parse_delta(read_text(fixture_root() / "delta" / "field-change.json"));
  ASSERT_TRUE(delta.ok) << delta.error;
  const auto applied = obs::apply_delta(parent.snapshot, delta.delta);
  ASSERT_TRUE(applied.ok) << applied.error;
  EXPECT_EQ(applied.snapshot_id, child.snapshot_id);

  obs::ObservableTopologyService service(obs::qualified_scope_key(parent.snapshot));
  ASSERT_TRUE(service.publish(parent.snapshot).ok);
  ASSERT_TRUE(service.publish(child.snapshot).ok);
  obs::QueryRequest request;
  request.kind = obs::QueryKind::Lookup;
  request.subject = parent.snapshot.root;
  const auto response = service.query(request);
  EXPECT_EQ(response.status, obs::QueryStatus::Complete);
  ASSERT_FALSE(response.results.empty());

  std::string query_error;
  const auto path_request = obs::parse_query_request(
    read_text(fixture_root() / "query" / "canonical-path.json"), query_error);
  ASSERT_TRUE(query_error.empty()) << query_error;
  EXPECT_EQ(path_request.kind, obs::QueryKind::CanonicalPath);
  const auto path = service.query(path_request);
  EXPECT_EQ(path.status, obs::QueryStatus::Complete);
  ASSERT_EQ(path.results.size(), 3u);
  EXPECT_EQ(path.results.front().id, parent.snapshot.root);
  const auto wire = obs::serialize_query_response(path);
  EXPECT_NE(wire.find("\"status\":\"complete\""), std::string::npos);
  EXPECT_NE(wire.find(parent.snapshot.root), std::string::npos);
}

TEST(StyioObservableConsumer, RejectsUnknownRequiredCapabilities) {
  obs::NegotiationRequest request;
  request.required_capabilities = {"totally-unknown-required"};
  const auto rejected = obs::negotiate_observable_contracts(request);
  EXPECT_FALSE(rejected.ok);
  EXPECT_EQ(rejected.reason, obs::kReasonUnknownRequiredCapability);

  const auto map = read_text(STYIO_OBSERVABLE_CONSUMER_LINK_MAP);
  EXPECT_EQ(map.find("styio_frontend_core"), std::string::npos) << map;
  EXPECT_EQ(map.find("styio_ide_core"), std::string::npos) << map;
  EXPECT_EQ(map.find("styio_runtime_core"), std::string::npos) << map;
  EXPECT_EQ(map.find("LLVM"), std::string::npos) << map;
  EXPECT_EQ(map.find("llvm"), std::string::npos) << map;
}
