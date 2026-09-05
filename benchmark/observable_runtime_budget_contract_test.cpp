#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

#ifndef STYIO_BUILD_DIR
#define STYIO_BUILD_DIR "."
#endif

namespace {

std::string read_text(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return {};
  }
  std::ostringstream out;
  out << in.rdbuf();
  return out.str();
}

bool extract_string(const std::string& json, const std::string& key, std::string& out) {
  const std::string needle = "\"" + key + "\":\"";
  const auto pos = json.find(needle);
  if (pos == std::string::npos) {
    return false;
  }
  const auto start = pos + needle.size();
  const auto end = json.find('"', start);
  if (end == std::string::npos) {
    return false;
  }
  out = json.substr(start, end - start);
  return true;
}

const char* required_fields[] = {
  "wall_ns",
  "cpu_ns",
  "allocations",
  "peak_rss_bytes",
  "binary_size",
  "artifact_size",
  "produced_bytes",
  "produced_records",
  "aggregated_records",
  "emitted_bytes",
  "emitted_records",
  "occupancy_high",
  "sampled_out",
  "buffer_dropped",
  "exporter_dropped",
};

int fail(const std::string& reason) {
  std::cerr << "styio_observable_runtime_budget_contract: " << reason << "\n";
  std::cerr << "Numeric ceilings are owned by the benchmark authority. "
               "This seam rejects missing, stale, pending, or incomparable "
               "results and does not invent an approval.\n";
  return 1;
}

} // namespace

int
main() {
  const char* env_path = std::getenv("STYIO_OBSERVABLE_RUNTIME_BUDGET_RESULT");
  std::string path = env_path != nullptr ? env_path : "";
  if (path.empty()) {
    path = std::string(STYIO_BUILD_DIR)
      + "/benchmark/observable-runtime/approved-result.json";
  }
  const std::string json = read_text(path);
  if (json.empty()) {
    return fail("missing approved baseline result at " + path);
  }
  std::string status;
  if (!extract_string(json, "status", status) && !extract_string(json, "approval", status)) {
    return fail("result contract is missing status");
  }
  if (status == "pending") {
    return fail("budget result is pending");
  }
  if (status == "stale") {
    return fail("budget result is stale");
  }
  if (status == "incomparable") {
    return fail("budget result is incomparable");
  }
  if (status == "failed" || status == "unapproved") {
    return fail("budget result is " + status);
  }
  if (status != "approved") {
    return fail("budget result status is not approved: " + status);
  }
  for (const char* mode : {"disabled", "static-only", "aggregate", "sampled", "detailed"}) {
    if (json.find(std::string("\"") + mode + "\"") == std::string::npos) {
      return fail(std::string("missing mode object: ") + mode);
    }
  }
  for (const char* field : required_fields) {
    if (json.find(std::string("\"") + field + "\"") == std::string::npos) {
      return fail(std::string("missing required field: ") + field);
    }
  }
  return 0;
}
