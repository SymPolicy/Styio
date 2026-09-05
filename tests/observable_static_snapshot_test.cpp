#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#ifndef _WIN32
#include <sys/wait.h>
#endif

#include "StyioAST/AST.hpp"
#include "StyioException/Exception.hpp"
#include "StyioLowering/AstToStyioIRLowerer.hpp"
#include "StyioParser/Parser.hpp"
#include "StyioParser/Tokenizer.hpp"
#include "StyioResourceTopology/ResourceTopology.hpp"
#include "StyioServices/StyioConfig/CompilePlanContract.hpp"
#include "StyioServices/StyioObservableProducer/StaticSnapshotContract.hpp"

#include "llvm/Support/JSON.h"

#ifndef STYIO_SOURCE_DIR
#define STYIO_SOURCE_DIR "."
#endif
#ifndef STYIO_COMPILER_EXE
#define STYIO_COMPILER_EXE ""
#endif
#ifndef STYIO_NANO_COMPILER_EXE
#define STYIO_NANO_COMPILER_EXE ""
#endif

namespace fs = std::filesystem;
namespace obs = styio::observable;
namespace cfg = styio::config;
namespace rt = styio::resource_topology;

namespace {

std::string read_text(const fs::path& path) {
  std::ifstream in(path, std::ios::binary);
  std::ostringstream out;
  out << in.rdbuf();
  return out.str();
}

void write_text(const fs::path& path, std::string_view text) {
  fs::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out << text;
}

fs::path fixture_root() {
  return fs::path(STYIO_SOURCE_DIR) / "tests/fixtures/observable_static_snapshot/v1";
}

std::vector<std::pair<size_t, size_t>> line_seps(const std::string& src) {
  std::vector<std::pair<size_t, size_t>> seps;
  size_t begin = 0;
  for (size_t i = 0; i < src.size(); ++i) {
    if (src[i] == '\n') {
      seps.emplace_back(begin, i);
      begin = i + 1;
    }
  }
  seps.emplace_back(begin, src.size());
  return seps;
}

void free_tokens(std::vector<StyioToken*>& tokens) {
  for (auto* token : tokens) {
    delete token;
  }
}

cfg::CompilationUnit example_unit(
  std::string package = "example.app",
  std::string manifest = "Styio.toml",
  std::string entry = "src/main.styio"
) {
  return cfg::CompilationUnit{
    std::move(package), std::move(manifest), std::move(entry)};
}

obs::SnapshotPublishResult publish_source(
  const std::string& src,
  const cfg::CompilationUnit& unit,
  obs::SnapshotFault fault = obs::SnapshotFault::None
) {
  auto tokens = StyioTokenizer::tokenize(src);
  StyioContext* ctx = StyioContext::Create("snapshot.styio", src, line_seps(src), tokens, false);
  MainBlockAST* ast = nullptr;
  try {
    ast = parse_main_block_with_engine_latest(*ctx, StyioParserEngine::Nightly);
    AstToStyioIRLowerer analyzer(styio::semantic_identity::Scope::qualified(
      unit.package_name, unit.manifest_relative_path, unit.entry_relative_path));
    ast->typeInfer(&analyzer);
    obs::SnapshotProducer producer{"styio", "0.0.1"};
    obs::SnapshotPublishResult result;
    if (analyzer.resource_topology_lifecycle()
        == StyioSemaContext::ResourceTopologyLifecycle::ScalarNoop) {
      result = obs::publish_proven_scalar_noop(unit, producer);
    } else {
      const auto* artifact = analyzer.resource_topology_artifact_for(ast);
      if (artifact == nullptr) {
        result.error = "missing topology artifact";
      } else {
        result = obs::publish_validated_topology(*artifact, unit, producer, fault);
      }
    }
    delete ast;
    delete ctx;
    free_tokens(tokens);
    StyioAST::destroy_all_tracked_nodes();
    return result;
  } catch (...) {
    delete ast;
    delete ctx;
    free_tokens(tokens);
    StyioAST::destroy_all_tracked_nodes();
    throw;
  }
}

std::string json_escape(std::string_view value) {
  std::string out;
  out.reserve(value.size());
  for (char ch : value) {
    if (ch == '\\' || ch == '"') {
      out.push_back('\\');
    }
    out.push_back(ch);
  }
  return out;
}

std::string compile_plan_json(
  const std::string& tool,
  const fs::path& workspace,
  const std::string& package_id,
  const std::string& package_name,
  const fs::path& package_root,
  const fs::path& manifest,
  const fs::path& entry,
  const fs::path& build_root,
  const fs::path& artifact_dir,
  const fs::path& diag_dir,
  std::string_view snapshot_object,
  const std::string& extra_packages = ""
) {
  std::ostringstream plan;
  plan
    << "{\n"
    << "  \"plan_version\": 1,\n"
    << "  \"generated_by\": {\"tool\": \"" << tool << "\", \"version\": \"0.1.0\"},\n"
    << "  \"intent\": \"check\",\n"
    << "  \"workspace_root\": \"" << json_escape(workspace.string()) << "\",\n"
    << "  \"entry\": {\n"
    << "    \"package_id\": \"" << package_id << "\",\n"
    << "    \"target_kind\": \"bin\",\n"
    << "    \"target_name\": \"demo\",\n"
    << "    \"file\": \"" << json_escape(entry.string()) << "\"\n"
    << "  },\n"
    << "  \"toolchain\": {\"channel\": \"stable\", \"edition\": \"2026\", \"implicit_std\": true, \"std_package_id\": \"styio/std@2026\"},\n"
    << "  \"profile\": {\"name\": \"dev\", \"opt_level\": 0, \"debug\": true, \"lto\": false},\n"
    << "  \"packages\": [{\"id\": \"" << package_id << "\",\"name\": \"" << package_name
    << "\",\"root\": \"" << json_escape(package_root.string())
    << "\",\"manifest\": \"" << json_escape(manifest.string()) << "\"}"
    << extra_packages << "],\n"
    << "  \"resolution\": {\"resolver\": \"single-package\", \"package_order\": [\"" << package_id << "\"]},\n"
    << "  \"outputs\": {\"build_root\": \"" << json_escape(build_root.string())
    << "\", \"artifact_dir\": \"" << json_escape(artifact_dir.string())
    << "\", \"diag_dir\": \"" << json_escape(diag_dir.string()) << "\"},\n"
    << "  \"emit\": {\"error_format\": \"jsonl\", \"ast\": false, \"styio_ir\": false, \"llvm_ir\": false";
  if (!snapshot_object.empty()) {
    plan << ", \"observable_static_snapshot\": " << snapshot_object;
  }
  plan << "}\n}\n";
  return plan.str();
}

struct CommandResult {
  int exit_code = -1;
  std::string output;
};

CommandResult run_command(const std::string& command) {
  CommandResult result;
  FILE* pipe = popen(command.c_str(), "r");
  if (pipe == nullptr) {
    result.output = "popen failed";
    return result;
  }
  std::array<char, 4096> buffer{};
  while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
    result.output.append(buffer.data());
  }
  const int status = pclose(pipe);
#ifdef _WIN32
  result.exit_code = status;
#else
  result.exit_code = WEXITSTATUS(status);
#endif
  return result;
}

const char* compiler_exe() {
  const char* env = std::getenv("STYIO_COMPILER_EXE");
  if (env != nullptr && env[0] != '\0') {
    return env;
  }
  return STYIO_COMPILER_EXE;
}

bool json_has_sorted_ids(const llvm::json::Array* records, const char* id_key) {
  if (records == nullptr) {
    return false;
  }
  std::string previous;
  for (const auto& value : *records) {
    const auto* object = value.getAsObject();
    if (object == nullptr) {
      return false;
    }
    const auto id = object->getString(id_key);
    if (!id.has_value()) {
      return false;
    }
    const std::string current = std::string(*id);
    if (!previous.empty() && current < previous) {
      return false;
    }
    previous = current;
  }
  return true;
}

std::set<std::string> collect_ids(const llvm::json::Array* records, const char* id_key) {
  std::set<std::string> ids;
  if (records == nullptr) {
    return ids;
  }
  for (const auto& value : *records) {
    const auto* object = value.getAsObject();
    if (object == nullptr) {
      continue;
    }
    const auto id = object->getString(id_key);
    if (id.has_value()) {
      ids.insert(std::string(*id));
    }
  }
  return ids;
}

} // namespace

TEST(StyioObservableStaticSnapshot, CanonicalProducerIsByteIdenticalAndComplete) {
  const std::string source = read_text(fixture_root() / "package/src/main.styio");
  const auto unit = example_unit();
  const auto first = publish_source(source, unit);
  const auto second = publish_source(source, unit);
  ASSERT_TRUE(first.ok) << first.error;
  ASSERT_TRUE(second.ok) << second.error;
  EXPECT_EQ(first.json, second.json);

  llvm::Expected<llvm::json::Value> parsed = llvm::json::parse(first.json);
  ASSERT_TRUE((bool)parsed);
  const auto* root = parsed->getAsObject();
  ASSERT_NE(root, nullptr);
  EXPECT_EQ(
    std::string(root->getString("contract").value_or("")),
    std::string(obs::kStaticSnapshotContractName));
  EXPECT_EQ(root->getInteger("schema_version").value_or(0), 1);
  EXPECT_EQ(
    std::string(root->getString("stability").value_or("")),
    std::string(obs::kStaticSnapshotStability));
  EXPECT_EQ(
    std::string(root->getString("completeness").value_or("")),
    std::string(obs::kCompletenessValidatedTopology));
  EXPECT_TRUE(root->getString("root").has_value());
  ASSERT_NE(root->getArray("nodes"), nullptr);
  ASSERT_NE(root->getArray("edges"), nullptr);
  ASSERT_NE(root->getArray("facts"), nullptr);
  ASSERT_NE(root->getArray("anchors"), nullptr);
  ASSERT_NE(root->getArray("evidence"), nullptr);
  EXPECT_FALSE(root->getArray("nodes")->empty());
  EXPECT_FALSE(root->getArray("edges")->empty());
  EXPECT_TRUE(json_has_sorted_ids(root->getArray("nodes"), "id"));
  EXPECT_TRUE(json_has_sorted_ids(root->getArray("edges"), "id"));
  EXPECT_TRUE(json_has_sorted_ids(root->getArray("facts"), "id"));
  EXPECT_TRUE(json_has_sorted_ids(root->getArray("anchors"), "ref"));
  EXPECT_TRUE(json_has_sorted_ids(root->getArray("evidence"), "ref"));

  std::set<std::string> kinds;
  std::set<std::string> roles;
  std::set<std::string> edge_kinds;
  for (const auto& value : *root->getArray("nodes")) {
    const auto* node = value.getAsObject();
    ASSERT_NE(node, nullptr);
    kinds.insert(std::string(*node->getString("kind")));
    roles.insert(std::string(*node->getString("role")));
  }
  for (const auto& value : *root->getArray("edges")) {
    const auto* edge = value.getAsObject();
    ASSERT_NE(edge, nullptr);
    edge_kinds.insert(std::string(*edge->getString("kind")));
  }
  EXPECT_TRUE(kinds.count("Program"));
  EXPECT_TRUE(kinds.count("Sink") || kinds.count("Handle") || kinds.count("StateSlot"));
  EXPECT_TRUE(roles.count("Program"));
  EXPECT_FALSE(edge_kinds.empty());

  const fs::path golden = fixture_root() / "canonical.json";
  const fs::path additive = fixture_root() / "additive-field.json";
  auto write_additive = [&](const std::string& json) {
    std::string extra = json;
    if (!extra.empty() && extra.back() == '\n') {
      extra.pop_back();
    }
    if (!extra.empty() && extra.back() == '}') {
      extra.pop_back();
      extra += ",\"consumer_note\":\"ignored-additive-field\"}\n";
      write_text(additive, extra);
    }
  };
  if (fs::exists(golden)) {
    EXPECT_EQ(first.json, read_text(golden));
    if (!fs::exists(additive)) {
      write_additive(first.json);
    }
  } else {
    write_text(golden, first.json);
    write_additive(first.json);
    FAIL() << "wrote missing canonical.json; re-run to lock the golden";
  }
}

TEST(StyioObservableStaticSnapshot, IdentityDomainsChangeWithLogicalComponents) {
  const std::string source = read_text(fixture_root() / "package/src/main.styio");
  const auto baseline = publish_source(source, example_unit());
  const auto package_changed = publish_source(source, example_unit("other.app"));
  const auto manifest_changed = publish_source(source, example_unit("example.app", "Alt.toml"));
  const auto entry_changed = publish_source(
    source, example_unit("example.app", "Styio.toml", "src/alt.styio"));
  ASSERT_TRUE(baseline.ok) << baseline.error;
  ASSERT_TRUE(package_changed.ok);
  ASSERT_TRUE(manifest_changed.ok);
  ASSERT_TRUE(entry_changed.ok);
  EXPECT_NE(baseline.json, package_changed.json);
  EXPECT_NE(baseline.json, manifest_changed.json);
  EXPECT_NE(baseline.json, entry_changed.json);

  auto parse_ids = [](const std::string& json) {
    llvm::Expected<llvm::json::Value> parsed = llvm::json::parse(json);
    std::set<std::string> ids;
    const auto* root = parsed->getAsObject();
    for (const char* key : {"nodes", "edges", "facts"}) {
      const auto found = collect_ids(root->getArray(key), "id");
      ids.insert(found.begin(), found.end());
    }
    const auto anchors = collect_ids(root->getArray("anchors"), "ref");
    ids.insert(anchors.begin(), anchors.end());
    const auto evidence = collect_ids(root->getArray("evidence"), "ref");
    ids.insert(evidence.begin(), evidence.end());
    return ids;
  };
  EXPECT_TRUE(parse_ids(baseline.json).empty() == false);
  std::vector<std::string> intersection;
  const auto left = parse_ids(baseline.json);
  const auto right = parse_ids(package_changed.json);
  std::set_intersection(
    left.begin(), left.end(), right.begin(), right.end(), std::back_inserter(intersection));
  EXPECT_TRUE(intersection.empty());
}

TEST(StyioObservableStaticSnapshot, ScalarNoopIsCompleteWithoutGraph) {
  const std::string source = read_text(fixture_root() / "scalar/src/main.styio");
  const auto published = publish_source(source, example_unit("example.scalar"));
  ASSERT_TRUE(published.ok) << published.error;
  llvm::Expected<llvm::json::Value> parsed = llvm::json::parse(published.json);
  const auto* root = parsed->getAsObject();
  ASSERT_NE(root, nullptr);
  EXPECT_EQ(
    std::string(root->getString("completeness").value_or("")),
    std::string(obs::kCompletenessProvenScalarNoop));
  EXPECT_EQ(root->get("root")->kind(), llvm::json::Value::Null);
  EXPECT_TRUE(root->getArray("nodes")->empty());
  EXPECT_TRUE(root->getArray("edges")->empty());
  EXPECT_TRUE(root->getArray("facts")->empty());
  EXPECT_TRUE(root->getArray("anchors")->empty());
  EXPECT_TRUE(root->getArray("evidence")->empty());
  EXPECT_EQ(published.counts.nodes, 0u);
}

TEST(StyioObservableStaticSnapshot, FailClosedPublicationFaultsEmitNoJson) {
  const std::string source = read_text(fixture_root() / "package/src/main.styio");
  for (auto fault : {
         obs::SnapshotFault::MissingDescriptor,
         obs::SnapshotFault::DanglingEndpoint,
         obs::SnapshotFault::DuplicateIdentity,
         obs::SnapshotFault::EvidenceCycle}) {
    const auto published = publish_source(source, example_unit(), fault);
    EXPECT_FALSE(published.ok);
    EXPECT_TRUE(published.json.empty());
    EXPECT_FALSE(published.error.empty());
  }
}

TEST(StyioObservableStaticSnapshot, PrivacyScanRejectsForbiddenTokens) {
  const std::string source = read_text(fixture_root() / "package/src/main.styio");
  const auto published = publish_source(source, example_unit());
  ASSERT_TRUE(published.ok) << published.error;
  EXPECT_EQ(published.json.find("/Users/"), std::string::npos);
  EXPECT_EQ(published.json.find("/home/"), std::string::npos);
  EXPECT_EQ(published.json.find("/private/"), std::string::npos);
  EXPECT_EQ(published.json.find("/tmp/"), std::string::npos);
  EXPECT_EQ(published.json.find("content_hash"), std::string::npos);
  EXPECT_EQ(published.json.find("raw_source"), std::string::npos);
  EXPECT_EQ(published.json.find("0x"), std::string::npos);
  EXPECT_NE(published.json.find("example.app"), std::string::npos);
  EXPECT_NE(published.json.find("src/main.styio"), std::string::npos);
  EXPECT_EQ(published.json.find("data/lines.txt"), std::string::npos);
}

TEST(StyioObservableStaticSnapshot, UserMethodNamesNeverAppearInPublishedBytes) {
  const std::string source =
    "@file::secretmark = () => { <| 1 }\n"
    "log <- @file(\"data/lines.txt\")\n"
    ">_(log.secretmark())\n";
  const auto published = publish_source(source, example_unit());
  ASSERT_TRUE(published.ok) << published.error;
  EXPECT_EQ(published.json.find("secretmark"), std::string::npos);
  EXPECT_EQ(published.json.find("resource-method:"), std::string::npos);
  EXPECT_NE(
    published.json.find("styio.sema.topology.relation.Borrow.resource-method"),
    std::string::npos);
}

TEST(StyioObservableStaticSnapshot, AnonymousArtifactCarriesNoPublicationMaterial) {
  const std::string source = read_text(fixture_root() / "package/src/main.styio");
  auto tokens = StyioTokenizer::tokenize(source);
  StyioContext* ctx = StyioContext::Create("snapshot.styio", source, line_seps(source), tokens, false);
  MainBlockAST* ast = nullptr;
  try {
    ast = parse_main_block_with_engine_latest(*ctx, StyioParserEngine::Nightly);
    AstToStyioIRLowerer analyzer;
    ast->typeInfer(&analyzer);
    const auto* artifact = analyzer.resource_topology_artifact_for(ast);
    ASSERT_NE(artifact, nullptr);
    EXPECT_FALSE(artifact->identity_scope().is_globally_comparable());
    EXPECT_FALSE(artifact->publication_complete());
    EXPECT_TRUE(artifact->node_publications().empty());
    EXPECT_TRUE(artifact->relation_publications().empty());
    const auto rejected = obs::publish_validated_topology(
      *artifact, example_unit(), obs::SnapshotProducer{"styio", "0.0.1"});
    EXPECT_FALSE(rejected.ok);
    EXPECT_NE(rejected.error.find("qualified"), std::string::npos);
    EXPECT_TRUE(rejected.json.empty());
    delete ast;
    delete ctx;
    free_tokens(tokens);
    StyioAST::destroy_all_tracked_nodes();
  } catch (...) {
    delete ast;
    delete ctx;
    free_tokens(tokens);
    StyioAST::destroy_all_tracked_nodes();
    throw;
  }
}

TEST(StyioObservableStaticSnapshotAdmission, ValidPafioPackageIsAdmitted) {
  const fs::path tmp = fs::temp_directory_path() / "styio-snapshot-admit";
  fs::create_directories(tmp / "pkg/src");
  write_text(tmp / "pkg/Styio.toml", "name = \"example.app\"\n");
  write_text(tmp / "pkg/src/main.styio", "x = 1\n>_(x)\n");
  const fs::path plan = tmp / "plan.json";
  write_text(
    plan,
    compile_plan_json(
      "pafio",
      tmp,
      "example.app@1",
      "example.app",
      tmp / "pkg",
      tmp / "pkg/Styio.toml",
      tmp / "pkg/src/main.styio",
      tmp / "build",
      tmp / "artifacts",
      tmp / "diag",
      "{\"schema_version\":1,\"required_capabilities\":[\"static-topology-nodes\"]}"));
  cfg::CompilePlanRequest request;
  std::string error;
  std::string subcode;
  ASSERT_TRUE(cfg::parse_compile_plan(plan, request, error, subcode)) << error;
  EXPECT_TRUE(request.emit_observable_static_snapshot);
  ASSERT_TRUE(request.compilation_unit.has_value());
  EXPECT_EQ(request.compilation_unit->package_name, "example.app");
  EXPECT_EQ(request.compilation_unit->manifest_relative_path, "Styio.toml");
  EXPECT_EQ(request.compilation_unit->entry_relative_path, "src/main.styio");
}

TEST(StyioObservableStaticSnapshotAdmission, RequestMatrixFailsBeforeSema) {
  const fs::path tmp = fs::temp_directory_path() / "styio-snapshot-admit-fail";
  fs::create_directories(tmp / "pkg/src");
  write_text(tmp / "pkg/Styio.toml", "name = \"example.app\"\n");
  write_text(tmp / "pkg/src/main.styio", "x = 1\n>_(x)\n");

  auto expect_fail = [&](
    const std::string& name,
    const std::string& tool,
    const std::string& package_id,
    const std::string& package_name,
    const fs::path& root,
    const fs::path& manifest,
    const fs::path& entry,
    std::string_view snapshot,
    const std::string& extra,
    const std::string& expected_subcode
  ) {
    const fs::path plan = tmp / (name + ".json");
    write_text(
      plan,
      compile_plan_json(
        tool, tmp, package_id, package_name, root, manifest, entry,
        tmp / "build", tmp / "artifacts", tmp / "diag", snapshot, extra));
    cfg::CompilePlanRequest request;
    std::string error;
    std::string subcode;
    EXPECT_FALSE(cfg::parse_compile_plan(plan, request, error, subcode)) << name << ": " << error;
    EXPECT_EQ(subcode, expected_subcode) << name << " error=" << error;
    EXPECT_FALSE(request.emit_observable_static_snapshot && request.compilation_unit.has_value());
  };

  const std::string snapshot =
    "{\"schema_version\":1,\"required_capabilities\":[\"static-topology-nodes\"]}";
  expect_fail(
    "direct-file", "styio", "example.app@1", "example.app",
    tmp / "pkg", tmp / "pkg/Styio.toml", tmp / "pkg/src/main.styio",
    snapshot, "", "observable_static_snapshot_direct_file");
  expect_fail(
    "unsupported-version", "pafio", "example.app@1", "example.app",
    tmp / "pkg", tmp / "pkg/Styio.toml", tmp / "pkg/src/main.styio",
    "{\"schema_version\":2,\"required_capabilities\":[]}",
    "", "observable_static_snapshot_unsupported_version");
  expect_fail(
    "unsupported-capability", "pafio", "example.app@1", "example.app",
    tmp / "pkg", tmp / "pkg/Styio.toml", tmp / "pkg/src/main.styio",
    "{\"schema_version\":1,\"required_capabilities\":[\"delta-query\"]}",
    "", "observable_static_snapshot_unsupported_capability");
  expect_fail(
    "malformed", "pafio", "example.app@1", "example.app",
    tmp / "pkg", tmp / "pkg/Styio.toml", tmp / "pkg/src/main.styio",
    "true", "", "observable_static_snapshot_malformed");
  expect_fail(
    "anonymous", "pafio", "example.app@1", "",
    tmp / "pkg", tmp / "pkg/Styio.toml", tmp / "pkg/src/main.styio",
    snapshot, "", "observable_static_snapshot_anonymous");
  expect_fail(
    "escaping", "pafio", "example.app@1", "example.app",
    tmp / "pkg", tmp / "pkg/Styio.toml", tmp / "outside.styio",
    snapshot, "", "observable_static_snapshot_escaping_path");
  expect_fail(
    "duplicate", "pafio", "example.app@1", "example.app",
    tmp / "pkg", tmp / "pkg/Styio.toml", tmp / "pkg/src/main.styio",
    snapshot,
    ",{\"id\":\"example.app@1\",\"name\":\"example.app\",\"root\":\""
      + json_escape((tmp / "pkg").string()) + "\",\"manifest\":\""
      + json_escape((tmp / "pkg/Styio.toml").string()) + "\"}",
    "observable_static_snapshot_duplicate");
  expect_fail(
    "ambiguous", "pafio", "example.app@1", "example.app",
    tmp / "pkg", tmp / "pkg/Styio.toml", tmp / "pkg/src/main.styio",
    snapshot,
    ",{\"id\":\"example.app@1\",\"name\":\"example.app\",\"root\":\""
      + json_escape((tmp / "pkg").string()) + "\",\"manifest\":\""
      + json_escape((tmp / "pkg/Styio.toml").string()) + "\"}"
      + ",{\"id\":\"example.app@1\",\"name\":\"example.app\",\"root\":\""
      + json_escape((tmp / "pkg").string()) + "\",\"manifest\":\""
      + json_escape((tmp / "pkg/Styio.toml").string()) + "\"}",
    "observable_static_snapshot_ambiguous");
  expect_fail(
    "styio-produced", "other-builder", "example.app@1", "example.app",
    tmp / "pkg", tmp / "pkg/Styio.toml", tmp / "pkg/src/main.styio",
    snapshot, "", "observable_static_snapshot_styio_produced");
}

TEST(StyioObservableStaticSnapshotAdmission, UnmatchedPackageFailsClosed) {
  const fs::path tmp = fs::temp_directory_path() / "styio-snapshot-unmatched";
  fs::create_directories(tmp / "pkg/src");
  write_text(tmp / "pkg/Styio.toml", "name = \"example.app\"\n");
  write_text(tmp / "pkg/src/main.styio", "x = 1\n>_(x)\n");
  std::ostringstream plan;
  plan
    << "{\n"
    << "  \"plan_version\": 1,\n"
    << "  \"generated_by\": {\"tool\": \"pafio\", \"version\": \"0.1.0\"},\n"
    << "  \"intent\": \"check\",\n"
    << "  \"workspace_root\": \"" << json_escape(tmp.string()) << "\",\n"
    << "  \"entry\": {\"package_id\": \"missing.app@1\", \"target_kind\": \"bin\", \"target_name\": \"demo\", \"file\": \""
    << json_escape((tmp / "pkg/src/main.styio").string()) << "\"},\n"
    << "  \"toolchain\": {\"channel\": \"stable\", \"edition\": \"2026\", \"implicit_std\": true, \"std_package_id\": \"styio/std@2026\"},\n"
    << "  \"profile\": {\"name\": \"dev\", \"opt_level\": 0, \"debug\": true, \"lto\": false},\n"
    << "  \"packages\": [{\"id\": \"example.app@1\",\"name\":\"example.app\",\"root\":\""
    << json_escape((tmp / "pkg").string()) << "\",\"manifest\":\""
    << json_escape((tmp / "pkg/Styio.toml").string()) << "\"}],\n"
    << "  \"resolution\": {\"resolver\": \"single-package\", \"package_order\": [\"example.app@1\"]},\n"
    << "  \"outputs\": {\"build_root\": \"" << json_escape((tmp / "build").string())
    << "\", \"artifact_dir\": \"" << json_escape((tmp / "artifacts").string())
    << "\", \"diag_dir\": \"" << json_escape((tmp / "diag").string()) << "\"},\n"
    << "  \"emit\": {\"error_format\": \"jsonl\", \"ast\": false, \"styio_ir\": false, \"llvm_ir\": false, \"observable_static_snapshot\": {\"schema_version\":1,\"required_capabilities\":[]}}\n"
    << "}\n";
  const fs::path plan_path = tmp / "plan.json";
  write_text(plan_path, plan.str());
  cfg::CompilePlanRequest request;
  std::string error;
  std::string subcode;
  EXPECT_FALSE(cfg::parse_compile_plan(plan_path, request, error, subcode));
  EXPECT_EQ(subcode, "observable_static_snapshot_unmatched") << error;
}

TEST(StyioObservableStaticSnapshotCli, RelocatedRootsProduceIdenticalBytes) {
  const char* runner = compiler_exe();
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');
  const std::string source = read_text(fixture_root() / "package/src/main.styio");
  auto compile_once = [&](const fs::path& root) {
    fs::create_directories(root / "pkg/src");
    fs::create_directories(root / "pkg/data");
    write_text(root / "pkg/Styio.toml", read_text(fixture_root() / "package/Styio.toml"));
    write_text(root / "pkg/src/main.styio", source);
    write_text(root / "pkg/data/lines.txt", read_text(fixture_root() / "package/data/lines.txt"));
    const fs::path plan = root / "plan.json";
    write_text(
      plan,
      compile_plan_json(
        "pafio",
        root,
        "example.app@1",
        "example.app",
        root / "pkg",
        root / "pkg/Styio.toml",
        root / "pkg/src/main.styio",
        root / "build",
        root / "artifacts",
        root / "diag",
        "{\"schema_version\":1,\"required_capabilities\":[]}"));
    const CommandResult result = run_command(
      std::string("\"") + runner + "\" --compile-plan \"" + plan.string() + "\" 2>&1");
    EXPECT_EQ(result.exit_code, 0) << result.output;
    return read_text(root / "artifacts/demo.observable-static-snapshot.json");
  };
  const auto first = compile_once(fs::temp_directory_path() / "styio-snap-a");
  const auto second = compile_once(fs::temp_directory_path() / "styio-snap-b");
  EXPECT_FALSE(first.empty());
  EXPECT_EQ(first, second);
}

TEST(StyioObservableStaticSnapshotCli, AbsentRequestKeepsDisabledPath) {
  const char* runner = compiler_exe();
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');
  const fs::path tmp = fs::temp_directory_path() / "styio-snap-disabled";
  fs::create_directories(tmp / "pkg/src");
  write_text(tmp / "pkg/Styio.toml", "name = \"example.app\"\n");
  write_text(tmp / "pkg/src/main.styio", "x = 1\n>_(x)\n");
  const fs::path plan = tmp / "plan.json";
  write_text(
    plan,
    compile_plan_json(
      "pafio", tmp, "example.app@1", "example.app", tmp / "pkg",
      tmp / "pkg/Styio.toml", tmp / "pkg/src/main.styio",
      tmp / "build", tmp / "artifacts", tmp / "diag", ""));
  const CommandResult result = run_command(
    std::string("\"") + runner + "\" --compile-plan \"" + plan.string() + "\" 2>&1");
  EXPECT_EQ(result.exit_code, 0) << result.output;
  EXPECT_FALSE(fs::exists(tmp / "artifacts/demo.observable-static-snapshot.json"));
}

TEST(StyioObservableStaticSnapshotCli, MachineInfoAdvertisesSchemaV1) {
  const char* runner = compiler_exe();
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');
  const CommandResult result = run_command(std::string("\"") + runner + "\" --machine-info=json");
  ASSERT_EQ(result.exit_code, 0) << result.output;
  EXPECT_NE(result.output.find("\"supported_contracts\":{\"machine_info\":[1]"), std::string::npos);
  EXPECT_NE(result.output.find("\"observable_static_snapshot\":{\"schema_versions\":[1]"), std::string::npos);
  EXPECT_NE(result.output.find("static-topology-nodes"), std::string::npos);
}

TEST(StyioObservableStaticSnapshotCli, NanoMachineInfoAdvertisesNone) {
  const char* nano = std::getenv("STYIO_NANO_COMPILER_EXE");
  if (nano == nullptr || nano[0] == '\0') {
    nano = STYIO_NANO_COMPILER_EXE;
  }
  if (nano == nullptr || nano[0] == '\0') {
    GTEST_SKIP() << "styio-nano is not built";
  }
  const CommandResult result = run_command(std::string("\"") + nano + "\" --machine-info=json");
  ASSERT_EQ(result.exit_code, 0) << result.output;
  EXPECT_NE(
    result.output.find("\"observable_static_snapshot\":{\"schema_versions\":[],\"capabilities\":[]}"),
    std::string::npos)
    << result.output;
}

TEST(StyioObservableStaticSnapshot, AdapterDoesNotRebuildTopology) {
  const std::string adapter = read_text(
    fs::path(STYIO_SOURCE_DIR) / "src/StyioServices/StyioObservableProducer/StaticSnapshotContract.cpp");
  EXPECT_EQ(adapter.find("validate_or_throw"), std::string::npos);
  EXPECT_EQ(adapter.find("resource_topology::build"), std::string::npos);
  EXPECT_EQ(adapter.find("Graph graph"), std::string::npos);
}

TEST(StyioObservableStaticSnapshotCli, ProfilerCountsMatchSerializedBytes) {
  const char* runner = compiler_exe();
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');
  const fs::path tmp = fs::temp_directory_path() / "styio-snap-profile";
  fs::create_directories(tmp / "pkg/src");
  fs::create_directories(tmp / "pkg/data");
  write_text(tmp / "pkg/Styio.toml", read_text(fixture_root() / "package/Styio.toml"));
  write_text(tmp / "pkg/src/main.styio", read_text(fixture_root() / "package/src/main.styio"));
  write_text(tmp / "pkg/data/lines.txt", read_text(fixture_root() / "package/data/lines.txt"));
  const fs::path plan = tmp / "plan.json";
  write_text(
    plan,
    compile_plan_json(
      "pafio", tmp, "example.app@1", "example.app", tmp / "pkg",
      tmp / "pkg/Styio.toml", tmp / "pkg/src/main.styio",
      tmp / "build", tmp / "artifacts", tmp / "diag",
      "{\"schema_version\":1,\"required_capabilities\":[]}"));
  const fs::path profile = tmp / "profile.json";
  const CommandResult result = run_command(
    std::string("\"") + runner
    + "\" --profile-frontend --profile-out \"" + profile.string()
    + "\" --compile-plan \"" + plan.string() + "\" 2>&1");
  EXPECT_EQ(result.exit_code, 0) << result.output;
  const std::string snapshot = read_text(tmp / "artifacts/demo.observable-static-snapshot.json");
  const std::string profile_json = read_text(profile);
  EXPECT_NE(profile_json.find("observable_static_snapshot"), std::string::npos);
  EXPECT_NE(profile_json.find("snapshot_serialized_bytes"), std::string::npos);
  EXPECT_NE(profile_json.find(std::to_string(snapshot.size())), std::string::npos);
}

TEST(StyioObservableStaticSnapshot, DenseIdsRemainInternal) {
  const std::string source = read_text(fixture_root() / "package/src/main.styio");
  const auto published = publish_source(source, example_unit());
  ASSERT_TRUE(published.ok) << published.error;
  EXPECT_EQ(published.json.find("\"id\":0"), std::string::npos);
  EXPECT_EQ(published.json.find("\"from\":0"), std::string::npos);
}

TEST(StyioObservableStaticSnapshotCli, AdmissionFailsBeforeSourceParse) {
  const char* runner = compiler_exe();
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');
  const fs::path tmp = fs::temp_directory_path() / "styio-snap-admit-cli";
  fs::create_directories(tmp / "pkg/src");
  write_text(tmp / "pkg/Styio.toml", "name = \"example.app\"\n");
  write_text(tmp / "pkg/src/main.styio", "this is not valid styio source !!!\n");
  const fs::path plan = tmp / "plan.json";
  write_text(
    plan,
    compile_plan_json(
      "pafio", tmp, "example.app@1", "example.app", tmp / "pkg",
      tmp / "pkg/Styio.toml", tmp / "pkg/src/main.styio",
      tmp / "build", tmp / "artifacts", tmp / "diag",
      "{\"schema_version\":2,\"required_capabilities\":[]}"));
  const CommandResult result = run_command(
    std::string("\"") + runner + "\" --compile-plan \"" + plan.string() + "\" 2>&1");
  EXPECT_NE(result.exit_code, 0);
  EXPECT_NE(result.output.find("observable_static_snapshot_unsupported_version"), std::string::npos)
    << result.output;
  EXPECT_FALSE(fs::exists(tmp / "artifacts/demo.observable-static-snapshot.json"));
}

TEST(StyioObservableStaticSnapshotCli, WriteFailureEmitsNoPartialArtifact) {
  const char* runner = compiler_exe();
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');
  const fs::path tmp = fs::temp_directory_path() / "styio-snap-write-fail";
  fs::create_directories(tmp / "pkg/src");
  write_text(tmp / "pkg/Styio.toml", "name = \"example.app\"\n");
  write_text(tmp / "pkg/src/main.styio", "x = 1\n>_(x)\n");
  const fs::path artifact_dir = tmp / "artifacts";
  fs::create_directories(artifact_dir);
  fs::create_directories(artifact_dir / "demo.observable-static-snapshot.json");
  const fs::path plan = tmp / "plan.json";
  write_text(
    plan,
    compile_plan_json(
      "pafio", tmp, "example.app@1", "example.app", tmp / "pkg",
      tmp / "pkg/Styio.toml", tmp / "pkg/src/main.styio",
      tmp / "build", artifact_dir, tmp / "diag",
      "{\"schema_version\":1,\"required_capabilities\":[]}"));
  const CommandResult result = run_command(
    std::string("\"") + runner + "\" --compile-plan \"" + plan.string() + "\" 2>&1");
  EXPECT_NE(result.exit_code, 0) << result.output;
  EXPECT_FALSE(fs::is_regular_file(artifact_dir / "demo.observable-static-snapshot.json"));
}

TEST(StyioObservableStaticSnapshotCli, ScalarNoopCompleteness) {
  const char* runner = compiler_exe();
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');
  const fs::path tmp = fs::temp_directory_path() / "styio-snap-scalar";
  fs::create_directories(tmp / "pkg/src");
  write_text(tmp / "pkg/Styio.toml", read_text(fixture_root() / "scalar/Styio.toml"));
  write_text(tmp / "pkg/src/main.styio", read_text(fixture_root() / "scalar/src/main.styio"));
  const fs::path plan = tmp / "plan.json";
  write_text(
    plan,
    compile_plan_json(
      "pafio", tmp, "example.scalar@1", "example.scalar", tmp / "pkg",
      tmp / "pkg/Styio.toml", tmp / "pkg/src/main.styio",
      tmp / "build", tmp / "artifacts", tmp / "diag",
      "{\"schema_version\":1,\"required_capabilities\":[]}"));
  const CommandResult result = run_command(
    std::string("\"") + runner + "\" --compile-plan \"" + plan.string() + "\" 2>&1");
  EXPECT_EQ(result.exit_code, 0) << result.output;
  const std::string snapshot = read_text(tmp / "artifacts/demo.observable-static-snapshot.json");
  EXPECT_NE(snapshot.find("complete/proven-scalar-noop"), std::string::npos);
  EXPECT_NE(snapshot.find("\"root\":null"), std::string::npos);
}
