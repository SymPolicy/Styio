#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <initializer_list>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "EnvTestUtil.hpp"

#ifndef _WIN32
#include <sys/wait.h>
#endif

#define main styio_main_entry_for_unit_tests
#include "../src/main.cpp"
#undef main

namespace fs = std::filesystem;

namespace {

class TempDir {
 public:
  explicit TempDir(const std::string& name)
    : path_(fs::temp_directory_path() / ("styio-main-contract-" + name + "-" + styio_now_token_latest()))
  {
    std::error_code ec;
    fs::create_directories(path_, ec);
    EXPECT_FALSE(ec) << ec.message();
  }

  ~TempDir() {
    std::error_code ec;
    fs::remove_all(path_, ec);
  }

  const fs::path& path() const {
    return path_;
  }

 private:
  fs::path path_;
};

class EnvVarGuard {
 public:
  explicit EnvVarGuard(std::string name)
    : name_(std::move(name))
  {
    if (const char* value = std::getenv(name_.c_str())) {
      original_ = std::string(value);
    }
  }

  ~EnvVarGuard() {
    if (original_.has_value()) {
      styio_test_setenv(name_.c_str(), original_->c_str(), 1);
    }
    else {
      styio_test_unsetenv(name_.c_str());
    }
  }

  void set(const std::string& value) {
    styio_test_setenv(name_.c_str(), value.c_str(), 1);
  }

  void unset() {
    styio_test_unsetenv(name_.c_str());
  }

 private:
  std::string name_;
  std::optional<std::string> original_;
};

class CurrentPathGuard {
 public:
  CurrentPathGuard()
    : original_(fs::current_path())
  {
  }

  ~CurrentPathGuard() {
    std::error_code ec;
    fs::current_path(original_, ec);
  }

 private:
  fs::path original_;
};

void WriteText(const fs::path& path, const std::string& text) {
  std::error_code ec;
  fs::create_directories(path.parent_path(), ec);
  ASSERT_FALSE(ec) << ec.message();
  std::string error;
  ASSERT_TRUE(styio_write_text_file_latest(path, text, error)) << error;
}

TEST(StyioMainContractSourceBuildInfo, BranchMappingDefaultsToStable) {
  EXPECT_STREQ(styio::config::default_source_origin(), "https://github.com/eBioRing/Styio.git");
  EXPECT_STREQ(styio::config::source_branch_for_channel("nightly"), "nightly");
  EXPECT_STREQ(styio::config::source_branch_for_channel("stable"), "stable");
  EXPECT_STREQ(styio::config::source_branch_for_channel("dev"), "stable");
}

std::string ReadText(const fs::path& path) {
  std::string text;
  std::string error;
  if (!styio_read_text_file_latest(path, text, error)) {
    ADD_FAILURE() << error;
    return "";
  }
  return text;
}

void MakeExecutable(const fs::path& path) {
  styio_make_file_executable_latest(path);
  ASSERT_TRUE(styio_native_build_is_executable_file_latest(path)) << path.string();
}

void RunShellOrFail(const std::string& command, const std::string& purpose) {
  const int status = std::system(command.c_str());
  ASSERT_EQ(status, 0) << purpose << ": " << command;
}

void RunProcessOrFail(const std::vector<std::string>& argv, const std::string& purpose) {
  std::string error;
  ASSERT_TRUE(styio_run_process_latest(argv, purpose, error)) << error;
}

bool ParseNativeBuildArgs(
  std::initializer_list<std::string> raw_args,
  StyioNativeBuildArgsLatest& out_args,
  std::string& error_message
) {
  std::vector<std::string> args(raw_args);
  std::vector<char*> argv;
  argv.reserve(args.size());
  for (auto& arg : args) {
    argv.push_back(arg.data());
  }
  return styio_parse_native_build_args_latest(
    static_cast<int>(argv.size()),
    argv.data(),
    out_args,
    error_message);
}

struct MainRunResult {
  int exit_code = -1;
  std::string stdout_text;
  std::string stderr_text;
};

MainRunResult RunMain(std::initializer_list<std::string> raw_args) {
  std::vector<std::string> args(raw_args);
  std::vector<char*> argv;
  argv.reserve(args.size());
  for (auto& arg : args) {
    argv.push_back(arg.data());
  }

  testing::internal::CaptureStdout();
  testing::internal::CaptureStderr();
  const int exit_code = styio_main_entry_for_unit_tests(
    static_cast<int>(argv.size()),
    argv.data());
  MainRunResult result;
  result.exit_code = exit_code;
  result.stdout_text = testing::internal::GetCapturedStdout();
  result.stderr_text = testing::internal::GetCapturedStderr();
  return result;
}

fs::path CurrentTestBinaryDir() {
  const fs::path self = styio::platform::current_executable_path();
  if (self.empty()) {
    return {};
  }
  return self.parent_path();
}

fs::path CurrentTestBinaryPath(const std::string& name) {
  return CurrentTestBinaryDir() / (name + std::string(styio::platform::executable_suffix()));
}

MainRunResult RunExternalTool(
  const fs::path& binary,
  std::initializer_list<std::string> raw_args
) {
  TempDir temp("external-tool");
  const fs::path stdout_path = temp.path() / "stdout.txt";
  const fs::path stderr_path = temp.path() / "stderr.txt";

  std::vector<std::string> argv = {binary.string()};
  for (const std::string& arg : raw_args) {
    argv.push_back(arg);
  }

  const auto status = styio::native::run_native_command_to_logs(argv, stdout_path, stderr_path);
  MainRunResult result;
  result.exit_code = status.exit_code;
  if (!status.launch_error.empty()) {
    result.stderr_text = status.launch_error;
  }
  result.stdout_text = ReadText(stdout_path);
  result.stderr_text += ReadText(stderr_path);
  return result;
}

cxxopts::ParseResult ParseMainOptions(std::initializer_list<std::string> raw_args) {
  std::vector<std::string> args(raw_args);
  std::vector<char*> argv;
  argv.reserve(args.size());
  for (auto& arg : args) {
    argv.push_back(arg.data());
  }

  cxxopts::Options options("styio", "Styio Compiler");
  options.allow_unrecognised_options();
  options.add_options()
    ("config", "Read project configuration.", cxxopts::value<std::string>())
    ("dict-impl", "Dictionary backend selector.", cxxopts::value<std::string>())
    ("nano-create", "Materialize a styio-nano package.", cxxopts::value<bool>()->default_value("false"))
    ("nano-publish", "Publish a styio-nano package.", cxxopts::value<bool>()->default_value("false"))
    ("nano-package-config", "Read styio-nano package configuration.", cxxopts::value<std::string>())
    ("nano-publish-config", "Read styio-nano publish configuration.", cxxopts::value<std::string>())
    ("nano-mode", "styio-nano package creation mode.", cxxopts::value<std::string>())
    ("nano-output", "Output directory.", cxxopts::value<std::string>())
    ("nano-name", "Package name.", cxxopts::value<std::string>())
    ("nano-profile", "Local-subset profile.", cxxopts::value<std::string>())
    ("nano-binary", "Existing styio-nano executable.", cxxopts::value<std::string>())
    ("nano-source-root", "Source root.", cxxopts::value<std::string>())
    ("nano-package-dir", "Materialized package directory.", cxxopts::value<std::string>())
    ("nano-channel", "Publish channel.", cxxopts::value<std::string>())
    ("nano-manifest", "Cloud package manifest.", cxxopts::value<std::string>())
    ("nano-registry", "Static repository root.", cxxopts::value<std::string>())
    ("nano-package", "Static repository package id.", cxxopts::value<std::string>())
    ("nano-version", "Static repository package version state.", cxxopts::value<std::string>());
  return options.parse(static_cast<int>(argv.size()), argv.data());
}

std::string GoodSha(char ch = 'a') {
  return std::string(64, ch);
}

void PopulateMinimalNanoSourceRoot(const fs::path& root) {
  WriteText(root / "CMakeLists.txt", "cmake_minimum_required(VERSION 3.20)\n");
  for (const std::string& relpath : styio_nano_source_roots_latest(false)) {
    WriteText(root / relpath, "// " + relpath + "\n");
  }
}

void WriteNanoProfileGenerator(const fs::path& root, bool write_output) {
  std::string script =
    "#!/usr/bin/env python3\n"
    "import argparse\n"
    "parser = argparse.ArgumentParser()\n"
    "parser.add_argument('--input')\n"
    "parser.add_argument('--cmake-out', dest='cmake_out')\n"
    "args = parser.parse_args()\n";
  if (write_output) {
    script +=
      "with open(args.cmake_out, 'w', encoding='utf-8') as out:\n"
      "    out.write('set(STYIO_NANO_INCLUDE_PIPELINE_CHECK OFF)\\n')\n";
  }
  WriteText(root / "scripts" / "gen-styio-nano-profile.py", script);
}

StyioNanoCreateSelectionLatest MakeLocalNanoSelection(
  const fs::path& output_dir,
  const fs::path& profile,
  const fs::path& source_root
) {
  StyioNanoCreateSelectionLatest selection;
  selection.mode = "local-subset";
  selection.output_dir = output_dir.string();
  selection.profile_path = profile.string();
  selection.source_root = source_root.string();
  selection.package_name = "local";
  return selection;
}

void WriteNanoRepositoryBlobEntry(
  const fs::path& repo,
  const fs::path& blob,
  const std::string& package,
  const std::string& version,
  StyioNanoRepositoryEntryLatest& entry,
  std::string& error
) {
  std::string sha256;
  ASSERT_TRUE(styio_compute_file_sha256_latest(blob, sha256, error)) << error;
  std::error_code ec;
  const uint64_t size_bytes = fs::file_size(blob, ec);
  ASSERT_FALSE(ec) << ec.message();

  entry = StyioNanoRepositoryEntryLatest{};
  entry.package_name = package;
  entry.version = version;
  entry.channel = "nano";
  entry.sha256 = sha256;
  entry.blob_path = styio_nano_repository_blob_relpath_latest(sha256);
  entry.size_bytes = size_bytes;
  ASSERT_TRUE(styio_copy_file_with_exec_latest(blob, repo / fs::path(entry.blob_path), false, error)) << error;
  ASSERT_TRUE(styio_write_nano_repository_entry_latest(repo, entry, error)) << error;
}

} // namespace

TEST(StyioMainContract, MainEntryDispatchAndEarlyCliExitsStayStable) {
  {
    const MainRunResult result = RunMain({"styio", "--help"});
    EXPECT_EQ(result.exit_code, static_cast<int>(StyioExitCode::Success));
    EXPECT_NE(result.stdout_text.find("Styio Compiler"), std::string::npos);
    EXPECT_NE(result.stdout_text.find("--nano-create"), std::string::npos);
  }
  {
    const MainRunResult result = RunMain({"styio", "--version"});
    EXPECT_EQ(result.exit_code, static_cast<int>(StyioExitCode::Success));
    EXPECT_NE(result.stdout_text.find("styio "), std::string::npos);
  }
  {
    const MainRunResult result = RunMain({"styio", "check", "--help"});
    EXPECT_EQ(result.exit_code, static_cast<int>(StyioExitCode::Success));
    EXPECT_NE(result.stdout_text.find("Usage: styio check"), std::string::npos);
  }
  {
    const MainRunResult result = RunMain({"styio", "build", "--help"});
    EXPECT_EQ(result.exit_code, static_cast<int>(StyioExitCode::Success));
    EXPECT_NE(result.stdout_text.find("Usage: styio build"), std::string::npos);
  }
  {
    const MainRunResult result = RunMain({"styio", "build", "input.styio"});
    EXPECT_EQ(result.exit_code, static_cast<int>(StyioExitCode::CliError));
    EXPECT_NE(result.stderr_text.find("requires -o"), std::string::npos);
  }

  {
    const MainRunResult result = RunMain({"styio", "--machine-info=json"});
    EXPECT_EQ(result.exit_code, static_cast<int>(StyioExitCode::Success));
    EXPECT_NE(result.stdout_text.find("\"tool\":\"styio\""), std::string::npos);
  }
  {
    const MainRunResult result = RunMain({"styio", "--machine-info=toml"});
    EXPECT_EQ(result.exit_code, static_cast<int>(StyioExitCode::CliError));
    EXPECT_NE(result.stderr_text.find("unsupported --machine-info format"), std::string::npos);
  }
  {
    const MainRunResult result = RunMain({"styio", "--source-build-info=json"});
    EXPECT_EQ(result.exit_code, static_cast<int>(StyioExitCode::Success));
    EXPECT_NE(result.stdout_text.find("\"contract\": \"source-build-info\""), std::string::npos);
  }
  {
    const MainRunResult result = RunMain({"styio", "--source-build-info=toml"});
    EXPECT_EQ(result.exit_code, static_cast<int>(StyioExitCode::CliError));
    EXPECT_NE(result.stderr_text.find("unsupported --source-build-info format"), std::string::npos);
  }
}

TEST(StyioMainContract, NativeBuildAndNanoBinaryCliGuardsStayFailClosed) {
  TempDir temp("native-build-cli-guards");
  std::string error;

  {
    const fs::path tmp_blocker = temp.path() / "tmp-blocker";
    WriteText(tmp_blocker, "not a directory\n");
    EnvVarGuard tmpdir_guard("TMPDIR");
    tmpdir_guard.set(tmp_blocker.string());

    const fs::path temp_root = styio_create_native_build_temp_root_latest(error);
    EXPECT_TRUE(temp_root.empty());
    EXPECT_NE(error.find("cannot resolve temporary directory"), std::string::npos) << error;
  }

  const fs::path source = temp.path() / "main.styio";
  WriteText(source, "print(1)\n");
  const fs::path output_parent_blocker = temp.path() / "out-parent";
  WriteText(output_parent_blocker, "not a directory\n");
  const MainRunResult blocked_output = RunMain({
    "styio",
    "build",
    source.string(),
    "-o",
    (output_parent_blocker / "app").string()});
  EXPECT_EQ(blocked_output.exit_code, static_cast<int>(StyioExitCode::CliError));
  EXPECT_NE(blocked_output.stderr_text.find("cannot create output directory"), std::string::npos)
    << blocked_output.stderr_text;

  const fs::path frontend_error_source = temp.path() / "frontend-error.styio";
  WriteText(frontend_error_source, "x = missing(1)\n");
  const fs::path styio_binary = CurrentTestBinaryPath("styio");
  ASSERT_TRUE(fs::exists(styio_binary)) << styio_binary.string();
  const MainRunResult frontend_error = RunExternalTool(
    styio_binary,
    {
      "build",
      frontend_error_source.string(),
      "-o",
      (temp.path() / "frontend-error-out").string()});
  EXPECT_EQ(frontend_error.exit_code, static_cast<int>(StyioExitCode::RuntimeError));
  EXPECT_NE(frontend_error.stderr_text.find("styio build frontend compilation failed"), std::string::npos)
    << frontend_error.stderr_text;

  const fs::path styio_nano = CurrentTestBinaryPath("styio-nano");
  ASSERT_TRUE(fs::exists(styio_nano)) << styio_nano.string();
  const MainRunResult nano_result = RunExternalTool(styio_nano, {"--nano-create"});
  EXPECT_EQ(nano_result.exit_code, static_cast<int>(StyioExitCode::CliError));
  EXPECT_NE(
    nano_result.stderr_text.find("styio-nano packaging commands are only available in the full styio compiler"),
    std::string::npos) << nano_result.stderr_text;
}

TEST(StyioMainContract, PathHelpersFallBackWhenCurrentDirectoryIsUnavailable) {
  TempDir temp("deleted-cwd");
  CurrentPathGuard cwd_guard;
  std::error_code ec;
  const fs::path deleted_cwd = temp.path() / "gone";
  fs::create_directories(deleted_cwd, ec);
  ASSERT_FALSE(ec) << ec.message();
  fs::current_path(deleted_cwd, ec);
  ASSERT_FALSE(ec) << ec.message();
  fs::remove_all(deleted_cwd, ec);
  ASSERT_FALSE(ec) << ec.message();

  EXPECT_EQ(styio_absolute_path_latest(fs::path("rel") / ".." / "file.styio"), fs::path("file.styio"));
  fs::path discovered_config;
  EXPECT_FALSE(styio_find_project_config_latest("rel/main.styio", discovered_config));
}

TEST(StyioMainContract, MainEntryCliGuardsStayFailClosed) {
  {
    const MainRunResult result = RunMain({"styio", "--bad-option"});
    EXPECT_EQ(result.exit_code, static_cast<int>(StyioExitCode::Success));
    EXPECT_TRUE(result.stderr_text.empty());
  }
  {
    const MainRunResult result = RunMain({"styio", "--machine-info"});
    EXPECT_EQ(result.exit_code, static_cast<int>(StyioExitCode::CliError));
    EXPECT_NE(result.stderr_text.find("[CliError]"), std::string::npos);
  }
  {
    const MainRunResult result = RunMain({"styio", "--dict-impl=bad"});
    EXPECT_EQ(result.exit_code, static_cast<int>(StyioExitCode::CliError));
    EXPECT_NE(result.stderr_text.find("unsupported --dict-impl"), std::string::npos);
  }
  {
    const MainRunResult result = RunMain({"styio", "--error-format=yaml"});
    EXPECT_EQ(result.exit_code, static_cast<int>(StyioExitCode::CliError));
    EXPECT_NE(result.stderr_text.find("unsupported --error-format"), std::string::npos);
  }
  {
    const MainRunResult result = RunMain({"styio", "--parser-engine=bad"});
    EXPECT_EQ(result.exit_code, static_cast<int>(StyioExitCode::CliError));
    EXPECT_NE(result.stderr_text.find("unsupported --parser-engine"), std::string::npos);
  }
  {
    const MainRunResult result = RunMain({"styio", "--profile-out=out.json"});
    EXPECT_EQ(result.exit_code, static_cast<int>(StyioExitCode::CliError));
    EXPECT_NE(result.stderr_text.find("--profile-out requires --profile-frontend"), std::string::npos);
  }
  {
    const MainRunResult result = RunMain({"styio", "--profile-frontend"});
    EXPECT_EQ(result.exit_code, static_cast<int>(StyioExitCode::CliError));
    EXPECT_NE(result.stderr_text.find("--profile-frontend requires --file or --compile-plan"), std::string::npos);
  }
  {
    const MainRunResult result = RunMain({"styio", "--parser-shadow-artifact-dir=shadow"});
    EXPECT_EQ(result.exit_code, static_cast<int>(StyioExitCode::CliError));
    EXPECT_NE(result.stderr_text.find("--parser-shadow-artifact-dir requires --parser-shadow-compare"), std::string::npos);
  }
  {
    const MainRunResult result = RunMain({"styio", "--file", "/tmp/styio-definitely-missing.styio", "--error-format=jsonl"});
    EXPECT_EQ(result.exit_code, static_cast<int>(StyioExitCode::RuntimeError));
    EXPECT_NE(result.stderr_text.find("\"category\":\"RuntimeError\""), std::string::npos);
    EXPECT_NE(result.stderr_text.find("file not found"), std::string::npos);
  }
}

TEST(StyioMainContract, MainEntryNanoAndCompilePlanGuardsStayFailClosed) {
  {
    const MainRunResult result = RunMain({"styio", "--nano-create", "--nano-publish"});
    EXPECT_EQ(result.exit_code, static_cast<int>(StyioExitCode::CliError));
    EXPECT_NE(result.stderr_text.find("mutually exclusive"), std::string::npos);
  }
  {
    const MainRunResult result = RunMain({"styio", "--nano-mode=cloud"});
    EXPECT_EQ(result.exit_code, static_cast<int>(StyioExitCode::CliError));
    EXPECT_NE(result.stderr_text.find("packaging arguments require --nano-create or --nano-publish"), std::string::npos);
  }
  {
    const MainRunResult result = RunMain({"styio", "--nano-create", "--nano-publish-config=publish.toml"});
    EXPECT_EQ(result.exit_code, static_cast<int>(StyioExitCode::CliError));
    EXPECT_NE(result.stderr_text.find("--nano-create does not accept publish-only options"), std::string::npos);
  }
  {
    const MainRunResult result = RunMain({"styio", "--nano-publish", "--nano-mode=cloud"});
    EXPECT_EQ(result.exit_code, static_cast<int>(StyioExitCode::CliError));
    EXPECT_NE(result.stderr_text.find("--nano-publish does not accept create-only options"), std::string::npos);
  }
  {
    const MainRunResult result = RunMain({"styio", "--nano-create"});
    EXPECT_EQ(result.exit_code, static_cast<int>(StyioExitCode::CliError));
    EXPECT_NE(result.stderr_text.find("styio-nano creation requires --nano-mode"), std::string::npos);
  }
  {
    const MainRunResult result = RunMain({"styio", "--nano-publish"});
    EXPECT_EQ(result.exit_code, static_cast<int>(StyioExitCode::CliError));
    EXPECT_NE(result.stderr_text.find("styio-nano publish requires --nano-package-dir"), std::string::npos);
  }
  {
    const MainRunResult result = RunMain({"styio", "--nano-create", "--nano-package-config="});
    EXPECT_EQ(result.exit_code, static_cast<int>(StyioExitCode::CliError));
    EXPECT_NE(result.stderr_text.find("--nano-package-config requires a non-empty path"), std::string::npos);
  }
  {
    const MainRunResult result = RunMain({"styio", "--nano-publish", "--nano-publish-config="});
    EXPECT_EQ(result.exit_code, static_cast<int>(StyioExitCode::CliError));
    EXPECT_NE(result.stderr_text.find("--nano-publish-config requires a non-empty path"), std::string::npos);
  }
  {
    TempDir temp("nano-cli-guards");
    const fs::path conflict = temp.path() / "conflict.toml";
    WriteText(
      conflict,
      "mode = \"cloud\"\n"
      "output = \"out\"\n"
      "[nano.cloud]\n"
      "manifest = \"manifest.toml\"\n"
      "registry = \"repo\"\n"
      "package = \"org/pkg\"\n"
      "version = \"1.0.0\"\n");
    const MainRunResult result = RunMain({
      "styio",
      "--nano-create",
      "--nano-package-config=" + conflict.string()});
    EXPECT_EQ(result.exit_code, static_cast<int>(StyioExitCode::CliError));
    EXPECT_NE(result.stderr_text.find("accepts either nano.cloud.manifest"), std::string::npos);
  }
  {
    TempDir temp("nano-publish-cli-guards");
    const fs::path package = temp.path() / "package";
    fs::create_directories(package);
    const fs::path publish_config = temp.path() / "publish.toml";
    WriteText(
      publish_config,
      "[publish]\n"
      "package_dir = \"package\"\n"
      "registry = \"repo\"\n"
      "package = \"org/pkg\"\n"
      "version = \"1.0.0\"\n"
      "channel = \"edge\"\n");
    const MainRunResult result = RunMain({
      "styio",
      "--nano-publish",
      "--nano-publish-config=" + publish_config.string()});
    EXPECT_EQ(result.exit_code, static_cast<int>(StyioExitCode::CliError));
    EXPECT_NE(result.stderr_text.find("missing bin/" + styio_nano_binary_filename_latest()), std::string::npos);
  }
  {
    TempDir temp("nano-publish-http-guard");
    const fs::path package = temp.path() / "package";
    WriteText(package / "bin" / styio_nano_binary_filename_latest(), "#!/usr/bin/env sh\nexit 0\n");
    const MainRunResult result = RunMain({
      "styio",
      "--nano-publish",
      "--nano-package-dir=" + package.string(),
      "--nano-registry=https://example.test/repo",
      "--nano-package=org/pkg",
      "--nano-version=1.0.0"});
    EXPECT_EQ(result.exit_code, static_cast<int>(StyioExitCode::CliError));
    EXPECT_NE(result.stderr_text.find("only supports local repository roots"), std::string::npos);
  }
  {
    TempDir temp("nano-publish-main-failure");
    const fs::path package = temp.path() / "package";
    WriteText(package / "bin" / styio_nano_binary_filename_latest(), "#!/usr/bin/env sh\nexit 0\n");
    const fs::path repo_blocker = temp.path() / "repo-blocker";
    WriteText(repo_blocker, "not a directory\n");
    const MainRunResult result = RunMain({
      "styio",
      "--nano-publish",
      "--nano-package-dir=" + package.string(),
      "--nano-registry=" + repo_blocker.string(),
      "--nano-package=org/pkg",
      "--nano-version=1.0.0"});
    EXPECT_EQ(result.exit_code, static_cast<int>(StyioExitCode::CliError));
    EXPECT_NE(result.stderr_text.find("failed to create nano repository root"), std::string::npos);
  }
  {
    const MainRunResult result = RunMain({"styio", "--compile-plan=plan.json", "--file", "main.styio"});
    EXPECT_EQ(result.exit_code, static_cast<int>(StyioExitCode::CliError));
    EXPECT_NE(result.stderr_text.find("--compile-plan and --file are mutually exclusive"), std::string::npos);
  }
  {
    const MainRunResult result = RunMain({"styio", "--compile-plan=missing-plan.json"});
    EXPECT_EQ(result.exit_code, static_cast<int>(StyioExitCode::CliError));
    EXPECT_NE(result.stderr_text.find("missing-plan.json"), std::string::npos);
  }
}

TEST(StyioMainContract, MainEntryFrontendDiagnosticsStayFailClosed) {
  TempDir temp("frontend-diagnostics");

  const fs::path lex_source = temp.path() / "lex.styio";
  WriteText(lex_source, "/* unterminated");
  const MainRunResult lex_result =
    RunMain({"styio", "--file", lex_source.string(), "--error-format=jsonl"});
  EXPECT_EQ(lex_result.exit_code, static_cast<int>(StyioExitCode::LexError));
  EXPECT_NE(lex_result.stderr_text.find("\"category\":\"LexError\""), std::string::npos)
    << lex_result.stderr_text;

  const fs::path parse_source = temp.path() / "parse.styio";
  WriteText(parse_source, "foo.bar(1).baz(2)\n");
  const MainRunResult parse_result =
    RunMain({"styio", "--file", parse_source.string(), "--error-format=jsonl"});
  EXPECT_EQ(parse_result.exit_code, static_cast<int>(StyioExitCode::ParseError));
  EXPECT_NE(parse_result.stderr_text.find("\"category\":\"ParseError\""), std::string::npos)
    << parse_result.stderr_text;

  const fs::path type_source = temp.path() / "type.styio";
  WriteText(type_source, "x = missing(1)\n");
  const MainRunResult type_result =
    RunMain({"styio", "--file", type_source.string(), "--error-format=jsonl"});
  EXPECT_EQ(type_result.exit_code, static_cast<int>(StyioExitCode::TypeError));
  EXPECT_NE(type_result.stderr_text.find("\"category\":\"TypeError\""), std::string::npos)
    << type_result.stderr_text;
}

TEST(StyioMainContract, MainEntryAstAndIrTTYOutputUsesAnsiHeaders) {
  std::string script_path;
  if (!styio_native_build_find_executable_latest("script", script_path)) {
    GTEST_SKIP() << "script command is required for pseudo-terminal coverage";
  }

  const fs::path styio_binary = CurrentTestBinaryPath("styio");
  if (!fs::exists(styio_binary)) {
    GTEST_SKIP() << "styio binary is not available next to the test binary";
  }

  TempDir temp("tty-output");
  const fs::path source = temp.path() / "main.styio";
  WriteText(source, "x = 1\n");

  const fs::path transcript = temp.path() / "transcript.txt";
  const fs::path stdout_path = temp.path() / "script.stdout";
  const fs::path stderr_path = temp.path() / "script.stderr";
  const std::string child_command =
    styio_shell_quote_latest(styio_binary.string())
    + " --file " + styio_shell_quote_latest(source.string())
    + " --styio-ast --styio-ir --llvm-ir";
  const std::string command =
    styio_shell_quote_latest(script_path)
    + " -q -e -c " + styio_shell_quote_latest(child_command)
    + " " + styio_shell_quote_latest(transcript.string())
    + " > " + styio_shell_quote_latest(stdout_path.string())
    + " 2> " + styio_shell_quote_latest(stderr_path.string());
  RunShellOrFail(command, "run styio AST/IR dump under pseudo-terminal");

  const std::string tty_output = ReadText(transcript);
  EXPECT_NE(tty_output.find("\033[1;32mAST\033[0m \033[31m-Original\033[0m"), std::string::npos)
    << tty_output;
  EXPECT_NE(tty_output.find("\033[1;32mAST\033[0m \033[1;33m-Type-Checking\033[0m"), std::string::npos)
    << tty_output;
  EXPECT_NE(tty_output.find("\033[1;32mStyio IR\033[0m"), std::string::npos)
    << tty_output;
  EXPECT_NE(tty_output.find("\033[1;32mLLVM IR\033[0m"), std::string::npos)
    << tty_output;
}

TEST(StyioMainContract, EscapingDiagnosticsAndOptionMatchingStayStable) {
  EXPECT_EQ(styio_json_escape("a\\b\"c\n\r\t"), "a\\\\b\\\"c\\n\\r\\t");
  EXPECT_EQ(styio_toml_escape_string_latest("a\\b\"c\n\r\t"), "a\\\\b\\\"c\\n\\r\\t");
  EXPECT_TRUE(styio_error_jsonl_enabled("jsonl"));
  EXPECT_FALSE(styio_error_jsonl_enabled("text"));

  EXPECT_TRUE(styio_arg_matches_latest("--nano-mode=cloud", "--nano-mode"));
  EXPECT_TRUE(styio_arg_matches_latest("--nano-mode", "--nano-mode"));
  EXPECT_TRUE(styio_arg_matches_latest("-h", "--help", "-h"));
  EXPECT_FALSE(styio_arg_matches_latest(nullptr, "--help"));
  EXPECT_FALSE(styio_arg_matches_latest("--help", nullptr));
  EXPECT_FALSE(styio_arg_matches_latest("-x", "--help", "-h"));

  EXPECT_EQ(std::string(styio_category_name(StyioErrorCategory::CliError)), "CliError");
  EXPECT_EQ(std::string(styio_category_name(StyioErrorCategory::LexError)), "LexError");
  EXPECT_EQ(std::string(styio_category_name(StyioErrorCategory::ParseError)), "ParseError");
  EXPECT_EQ(std::string(styio_category_name(StyioErrorCategory::TypeError)), "TypeError");
  EXPECT_EQ(std::string(styio_category_name(StyioErrorCategory::RuntimeError)), "RuntimeError");
  EXPECT_EQ(
    styio_diagnostic_code(StyioErrorCategory::CliError, "ignored", "STYIO_CUSTOM_CODE"),
    "STYIO_CUSTOM_CODE");
  EXPECT_EQ(styio_category_phase(StyioErrorCategory::CliError), "service");
  EXPECT_EQ(styio_category_phase(StyioErrorCategory::LexError), "lex");
  EXPECT_EQ(styio_category_phase(StyioErrorCategory::ParseError), "parse");
  EXPECT_EQ(styio_category_phase(StyioErrorCategory::TypeError), "type");
  EXPECT_EQ(styio_category_phase(StyioErrorCategory::RuntimeError), "runtime");
  EXPECT_EQ(styio_exit_code(StyioErrorCategory::CliError), 6);
  EXPECT_EQ(styio_exit_code(StyioErrorCategory::LexError), 2);
  EXPECT_EQ(styio_exit_code(StyioErrorCategory::ParseError), 3);
  EXPECT_EQ(styio_exit_code(StyioErrorCategory::TypeError), 4);
  EXPECT_EQ(styio_exit_code(StyioErrorCategory::RuntimeError), 5);
  EXPECT_STREQ(styio_runtime_phase_name_latest(CompilationPhase::Tokenized), "tokenized");
  EXPECT_STREQ(styio_runtime_phase_name_latest(CompilationPhase::Lowered), "lowered");
  EXPECT_STREQ(styio_runtime_phase_name_latest(CompilationPhase::CodegenReady), "codegen_ready");
  EXPECT_STREQ(styio_runtime_phase_name_latest(CompilationPhase::Executed), "executed");

  const auto category = styio_parse_nano_option_category_latest("--nano-mode=cloud");
  ASSERT_TRUE(category.has_value());
  EXPECT_EQ(*category, StyioNanoOptionCategoryLatest::NanoPackaging);
  EXPECT_FALSE(styio_parse_nano_option_category_latest("--unknown").has_value());
}

TEST(StyioMainContract, ConfigPathFetchAndNativeLookupHelpersCoverEdges) {
  TempDir temp("config-path-fetch-native");
  std::string parsed;
  std::string error;

  EXPECT_EQ(styio_strip_inline_comment_latest("value # comment"), "value ");
  EXPECT_EQ(styio_strip_inline_comment_latest("\"#\" # comment"), "\"#\" ");
  EXPECT_EQ(styio_strip_inline_comment_latest("'#' # comment"), "'#' ");
  EXPECT_EQ(styio_strip_inline_comment_latest("\"\\\"#\" # comment"), "\"\\\"#\" ");

  EXPECT_FALSE(styio_parse_config_scalar_latest("", parsed, error));
  EXPECT_FALSE(styio_parse_config_scalar_latest("\"", parsed, error));
  ASSERT_TRUE(styio_parse_config_scalar_latest(" \"ok\" ", parsed, error)) << error;
  EXPECT_EQ(parsed, "ok");
  ASSERT_TRUE(styio_parse_config_scalar_latest("'single'", parsed, error)) << error;
  EXPECT_EQ(parsed, "single");
  ASSERT_TRUE(styio_parse_config_scalar_latest("bare", parsed, error)) << error;
  EXPECT_EQ(parsed, "bare");
  EXPECT_FALSE(styio_parse_config_scalar_latest("two words", parsed, error));

  EXPECT_EQ(
    styio_parse_project_config_section_latest("runtime"),
    StyioProjectConfigSectionLatest::RootOrRuntime);
  EXPECT_EQ(
    styio_parse_project_config_section_latest("dictionary"),
    StyioProjectConfigSectionLatest::Dict);
  EXPECT_EQ(
    styio_parse_project_config_section_latest("unknown"),
    StyioProjectConfigSectionLatest::Other);
  EXPECT_EQ(
    styio_parse_project_config_field_latest(StyioProjectConfigSectionLatest::RootOrRuntime, "dictionary_impl"),
    StyioProjectConfigFieldLatest::DictImpl);
  EXPECT_EQ(
    styio_parse_project_config_field_latest(StyioProjectConfigSectionLatest::Dict, "impl"),
    StyioProjectConfigFieldLatest::DictImpl);
  EXPECT_EQ(
    styio_parse_project_config_field_latest(StyioProjectConfigSectionLatest::Other, "impl"),
    StyioProjectConfigFieldLatest::None);
  EXPECT_EQ(
    styio_parse_project_config_field_latest(StyioProjectConfigSectionLatest::RootOrRuntime, "unknown"),
    StyioProjectConfigFieldLatest::None);
  EXPECT_EQ(
    styio_parse_project_config_field_latest(StyioProjectConfigSectionLatest::Dict, "unknown"),
    StyioProjectConfigFieldLatest::None);

  EXPECT_EQ(
    styio_parse_nano_package_config_section_latest("nano.local"),
    StyioNanoPackageConfigSectionLatest::NanoLocal);
  EXPECT_EQ(
    styio_parse_nano_package_config_section_latest("nano.cloud"),
    StyioNanoPackageConfigSectionLatest::NanoCloud);
  EXPECT_EQ(
    styio_parse_nano_package_config_field_latest(StyioNanoPackageConfigSectionLatest::RootOrNano, "manifest"),
    StyioNanoPackageConfigFieldLatest::Manifest);
  EXPECT_EQ(
    styio_parse_nano_package_config_field_latest(StyioNanoPackageConfigSectionLatest::NanoLocal, "source_root"),
    StyioNanoPackageConfigFieldLatest::SourceRoot);
  EXPECT_EQ(
    styio_parse_nano_package_config_field_latest(StyioNanoPackageConfigSectionLatest::NanoCloud, "version"),
    StyioNanoPackageConfigFieldLatest::RegistryVersion);
  EXPECT_EQ(
    styio_parse_nano_package_config_field_latest(StyioNanoPackageConfigSectionLatest::Other, "mode"),
    StyioNanoPackageConfigFieldLatest::None);
  EXPECT_EQ(
    styio_parse_nano_package_config_field_latest(StyioNanoPackageConfigSectionLatest::RootOrNano, "unknown"),
    StyioNanoPackageConfigFieldLatest::None);
  EXPECT_EQ(
    styio_parse_nano_package_config_field_latest(StyioNanoPackageConfigSectionLatest::NanoLocal, "unknown"),
    StyioNanoPackageConfigFieldLatest::None);
  EXPECT_EQ(
    styio_parse_nano_package_config_field_latest(StyioNanoPackageConfigSectionLatest::NanoCloud, "unknown"),
    StyioNanoPackageConfigFieldLatest::None);

  EXPECT_EQ(
    styio_parse_nano_publish_config_section_latest("publish"),
    StyioNanoPublishConfigSectionLatest::Publish);
  EXPECT_EQ(
    styio_parse_nano_publish_config_section_latest("elsewhere"),
    StyioNanoPublishConfigSectionLatest::Other);
  EXPECT_EQ(
    styio_parse_nano_publish_field_latest(StyioNanoPublishConfigSectionLatest::Publish, "package_root"),
    StyioNanoPublishFieldLatest::PackageDir);
  EXPECT_EQ(
    styio_parse_nano_publish_field_latest(StyioNanoPublishConfigSectionLatest::Other, "registry"),
    StyioNanoPublishFieldLatest::None);
  EXPECT_EQ(
    styio_parse_nano_publish_field_latest(StyioNanoPublishConfigSectionLatest::Publish, "unknown"),
    StyioNanoPublishFieldLatest::None);

  EXPECT_EQ(
    styio_parse_nano_manifest_section_latest("artifact"),
    StyioNanoManifestSectionLatest::Artifact);
  EXPECT_EQ(
    styio_parse_nano_manifest_section_latest("other"),
    StyioNanoManifestSectionLatest::Other);
  EXPECT_EQ(
    styio_parse_nano_manifest_field_latest(StyioNanoManifestSectionLatest::PackageRoot, "binary_ref"),
    StyioNanoManifestFieldLatest::Binary);
  EXPECT_EQ(
    styio_parse_nano_manifest_field_latest(StyioNanoManifestSectionLatest::Artifact, "profile_url"),
    StyioNanoManifestFieldLatest::Profile);
  EXPECT_EQ(
    styio_parse_nano_manifest_field_latest(StyioNanoManifestSectionLatest::Other, "binary"),
    StyioNanoManifestFieldLatest::None);
  EXPECT_EQ(
    styio_parse_nano_manifest_field_latest(StyioNanoManifestSectionLatest::PackageRoot, "unknown"),
    StyioNanoManifestFieldLatest::None);
  EXPECT_EQ(
    styio_parse_nano_manifest_field_latest(StyioNanoManifestSectionLatest::Artifact, "unknown"),
    StyioNanoManifestFieldLatest::None);

  const fs::path config_path = temp.path() / "cfg" / "styio.toml";
  WriteText(config_path, "dict_impl = \"linear\"\n");
  fs::path empty_discovered_config;
  EXPECT_FALSE(styio_find_project_config_latest("", empty_discovered_config));
  const fs::path nested_config = temp.path() / "nested-project" / ".styio.toml";
  const fs::path nested_source = temp.path() / "nested-project" / "src" / "main.styio";
  WriteText(nested_config, "[dict]\nimpl = \"linear\"\n");
  WriteText(nested_source, "print(1)\n");
  fs::path discovered_config;
  ASSERT_TRUE(styio_find_project_config_latest(nested_source.string(), discovered_config));
  EXPECT_EQ(discovered_config, nested_config);
  EXPECT_EQ(styio_file_uri_to_path_latest("file:///tmp/styio"), "/tmp/styio");
  EXPECT_EQ(styio_file_uri_to_path_latest("file://tmp/styio"), "/tmp/styio");
  EXPECT_EQ(styio_file_uri_to_path_latest("plain"), "plain");
  EXPECT_EQ(styio_shell_quote_latest("a'b"), "'a'\\''b'");
  EXPECT_EQ(styio_resolve_path_from_config_latest(config_path, ""), "");
  EXPECT_EQ(styio_resolve_path_from_config_latest(config_path, "https://example.test/a"), "https://example.test/a");
  EXPECT_EQ(styio_resolve_path_from_config_latest(config_path, "file:///tmp/a"), "file:///tmp/a");
  EXPECT_EQ(
    styio_resolve_path_from_config_latest(config_path, "rel/file.txt"),
    styio_absolute_path_latest(config_path.parent_path() / "rel" / "file.txt").string());
  EXPECT_EQ(
    styio_resolve_cli_path_latest("rel/file.txt"),
    styio_absolute_path_latest(fs::path("rel/file.txt")).string());

  const fs::path artifact = temp.path() / "base" / "artifact.bin";
  WriteText(artifact, "payload\n");
  EXPECT_FALSE(styio_fetch_ref_to_file_latest("", temp.path(), temp.path() / "out" / "empty.bin", false, error));
  EXPECT_FALSE(styio_fetch_ref_to_file_latest("missing.bin", {}, temp.path() / "out" / "missing.bin", false, error));
  ASSERT_TRUE(styio_fetch_ref_to_file_latest(
    "artifact.bin",
    artifact.parent_path(),
    temp.path() / "out" / "relative.bin",
    false,
    error)) << error;
  EXPECT_EQ(ReadText(temp.path() / "out" / "relative.bin"), "payload\n");
  ASSERT_TRUE(styio_fetch_ref_to_file_latest(
    "file://" + artifact.string(),
    {},
    temp.path() / "out" / "file-uri.bin",
    true,
    error)) << error;
  EXPECT_TRUE(styio_native_build_is_executable_file_latest(temp.path() / "out" / "file-uri.bin"));

  {
    const fs::path fake_bin = temp.path() / "fake-curl-bin";
    const fs::path fake_curl = fake_bin / "curl";
    WriteText(
      fake_curl,
      "#!/bin/sh\n"
      "out=\"\"\n"
      "while [ \"$#\" -gt 0 ]; do\n"
      "  if [ \"$1\" = \"-o\" ]; then shift; out=\"$1\"; fi\n"
      "  shift || exit 2\n"
      "done\n"
      "test -n \"$out\" || exit 3\n"
      "printf 'remote payload\\n' > \"$out\"\n");
    MakeExecutable(fake_curl);

    EnvVarGuard curl_path_guard("PATH");
    curl_path_guard.set(fake_bin.string());
    std::string registry_text;
    ASSERT_TRUE(styio_fetch_registry_text_latest(
      "https://example.test/repo",
      "styio-nano-repository.json",
      registry_text,
      error)) << error;
    EXPECT_EQ(registry_text, "remote payload\n");

    const fs::path downloaded = temp.path() / "out" / "remote.bin";
    ASSERT_TRUE(styio_fetch_ref_to_file_latest(
      "https://example.test/blob",
      {},
      downloaded,
      true,
      error)) << error;
    EXPECT_EQ(ReadText(downloaded), "remote payload\n");
    EXPECT_TRUE(styio_native_build_is_executable_file_latest(downloaded));

    WriteText(fake_curl, "#!/bin/sh\nexit 9\n");
    MakeExecutable(fake_curl);
    EXPECT_FALSE(styio_fetch_ref_to_file_latest(
      "https://example.test/fail",
      {},
      temp.path() / "out" / "remote-fail.bin",
      false,
      error));
    EXPECT_NE(error.find("failed to download artifact via curl"), std::string::npos);
  }

  const fs::path profile = temp.path() / "profile.toml";
  WriteText(profile, "[profile]\nignored-without-equals\nname = \"demo\"\n");
  std::string profile_name;
  ASSERT_TRUE(styio_parse_nano_profile_name_latest(profile, profile_name, error)) << error;
  EXPECT_EQ(profile_name, "demo");
  WriteText(temp.path() / "bad-profile.toml", "[profile\nname = demo\n");
  EXPECT_FALSE(styio_parse_nano_profile_name_latest(temp.path() / "bad-profile.toml", profile_name, error));

  const fs::path package_config = temp.path() / "nano-package.toml";
  WriteText(
    package_config,
    "[nano]\n"
    "mode = \"cloud\"\n"
    "output = \"out\"\n"
    "name = \"pkg\"\n"
    "ignored = \"skip\"\n"
    "[nano.local]\n"
    "profile = \"profile.toml\"\n"
    "binary = \"bin/styio-nano\"\n"
    "source_root = \"src\"\n"
    "[nano.cloud]\n"
    "manifest = \"manifest.toml\"\n"
    "registry = \"repo\"\n"
    "package = \"org/pkg\"\n"
    "version = \"1.0.0\"\n");
  StyioNanoPackageConfigLatest package_values;
  ASSERT_TRUE(styio_parse_nano_package_config_latest(package_config, package_values, error)) << error;
  EXPECT_TRUE(package_values.has_mode);
  EXPECT_TRUE(package_values.has_output_dir);
  EXPECT_TRUE(package_values.has_profile);
  EXPECT_TRUE(package_values.has_registry_version);
  EXPECT_EQ(package_values.package_name, "pkg");
  EXPECT_EQ(package_values.loaded_from, package_config.string());
  WriteText(temp.path() / "bad-package.toml", "[nano]\nmode = two words\n");
  EXPECT_FALSE(styio_parse_nano_package_config_latest(temp.path() / "bad-package.toml", package_values, error));

  const fs::path publish_config = temp.path() / "publish.toml";
  WriteText(
    publish_config,
    "[publish]\n"
    "package_root = \"pkg-dir\"\n"
    "registry = \"repo\"\n"
    "package = \"org/pkg\"\n"
    "version = \"2.0.0\"\n"
    "channel = \"beta\"\n"
    "ignored = \"skip\"\n");
  StyioNanoPublishConfigLatest publish_values;
  ASSERT_TRUE(styio_parse_nano_publish_config_latest(publish_config, publish_values, error)) << error;
  EXPECT_TRUE(publish_values.has_package_dir);
  EXPECT_TRUE(publish_values.has_registry);
  EXPECT_TRUE(publish_values.has_channel);
  EXPECT_EQ(publish_values.channel_raw, "beta");
  WriteText(temp.path() / "bad-publish.toml", "[publish]\nversion = two words\n");
  EXPECT_FALSE(styio_parse_nano_publish_config_latest(temp.path() / "bad-publish.toml", publish_values, error));

  const fs::path manifest = temp.path() / "manifest.toml";
  WriteText(
    manifest,
    "[package]\n"
    "name = \"pkg\"\n"
    "version = \"3.0.0\"\n"
    "channel = \"stable\"\n"
    "ignored = \"skip\"\n"
    "binary_ref = \"bin/styio-nano\"\n"
    "profile_ref = \"profile.toml\"\n"
    "[artifact]\n"
    "binary_url = \"bin/override\"\n"
    "profile_url = \"profile-override.toml\"\n");
  StyioNanoPackageManifestLatest manifest_values;
  ASSERT_TRUE(styio_parse_nano_package_manifest_latest(manifest, manifest_values, error)) << error;
  EXPECT_EQ(manifest_values.package_name, "pkg");
  EXPECT_EQ(manifest_values.version, "3.0.0");
  EXPECT_EQ(manifest_values.channel, "stable");
  EXPECT_EQ(manifest_values.binary_ref, "bin/override");
  EXPECT_EQ(manifest_values.profile_ref, "profile-override.toml");
  const fs::path defaulted_manifest = temp.path() / "defaulted.toml";
  WriteText(defaulted_manifest, "[artifact]\nbinary = \"bin/styio-nano\"\n");
  StyioNanoPackageManifestLatest defaulted_manifest_values;
  ASSERT_TRUE(styio_parse_nano_package_manifest_latest(
    defaulted_manifest,
    defaulted_manifest_values,
    error)) << error;
  EXPECT_EQ(defaulted_manifest_values.package_name, "defaulted");
  EXPECT_EQ(defaulted_manifest_values.channel, "nano");
  WriteText(temp.path() / "bad-manifest-scalar.toml", "[artifact]\nbinary = two words\n");
  EXPECT_FALSE(styio_parse_nano_package_manifest_latest(
    temp.path() / "bad-manifest-scalar.toml",
    defaulted_manifest_values,
    error));
  WriteText(temp.path() / "manifest-missing-binary.toml", "[package]\nname = \"pkg\"\n");
  StyioNanoPackageManifestLatest missing_manifest_values;
  EXPECT_FALSE(styio_parse_nano_package_manifest_latest(
    temp.path() / "manifest-missing-binary.toml",
    missing_manifest_values,
    error));

  std::string resolved;
  EXPECT_FALSE(styio_native_build_find_executable_latest("", resolved));
  const fs::path tool = temp.path() / "bin" / "tool";
  WriteText(tool, "#!/usr/bin/env sh\nexit 0\n");
  EXPECT_FALSE(styio_native_build_find_executable_latest(tool.string(), resolved));
  MakeExecutable(tool);
  ASSERT_TRUE(styio_native_build_find_executable_latest(tool.string(), resolved));
  EXPECT_EQ(resolved, tool.string());
  EnvVarGuard path_guard("PATH");
  path_guard.set(tool.parent_path().string());
  ASSERT_TRUE(styio_native_build_find_executable_latest("tool", resolved));
  EXPECT_EQ(resolved, tool.string());
  path_guard.unset();
  EXPECT_FALSE(styio_native_build_find_executable_latest("tool", resolved));

  EXPECT_FALSE(styio_native_build_find_clang_in_root_latest(temp.path() / "missing-root", resolved));
  const fs::path clang = temp.path() / "toolchain" / "bin" / "clang++-18";
  WriteText(clang, "#!/usr/bin/env sh\nexit 0\n");
  MakeExecutable(clang);
  ASSERT_TRUE(styio_native_build_find_clang_in_root_latest(temp.path() / "toolchain", resolved));
  EXPECT_EQ(resolved, clang.string());
  EXPECT_EQ(styio_native_build_read_log_latest(temp.path() / "missing.log"), "");
}

TEST(StyioMainContract, LowLevelMainHelpersCoverFailureBoundaries) {
  TempDir temp("low-level-failures");
  std::string error;

  const fs::path unreadable = temp.path() / "unreadable.styio";
  WriteText(unreadable, "print(1)\n");
  std::error_code perm_ec;
  fs::permissions(
    unreadable,
    fs::perms::owner_read | fs::perms::group_read | fs::perms::others_read,
    fs::perm_options::remove,
    perm_ec);
  ASSERT_FALSE(perm_ec) << perm_ec.message();
  const tmp_code_wrap unreadable_read = read_styio_file(unreadable.string());
  EXPECT_FALSE(unreadable_read.ok);
  EXPECT_NE(unreadable_read.error_message.find("cannot open file"), std::string::npos);
  fs::permissions(unreadable, fs::perms::owner_read, fs::perm_options::add, perm_ec);

  tmp_code_wrap printable_code;
  printable_code.code_text = "a\nb\n";
  printable_code.line_seps = {{0, 2}, {2, 2}};
  testing::internal::CaptureStdout();
  show_code_with_linenum(printable_code);
  const std::string code_dump = testing::internal::GetCapturedStdout();
  EXPECT_NE(code_dump.find("|0|-[0:2]"), std::string::npos);

  StyioToken* lf = StyioToken::Create(StyioTokenType::TOK_LF, "\n");
  StyioToken* space = StyioToken::Create(StyioTokenType::TOK_SPACE, " ");
  StyioToken* name = StyioToken::Create(StyioTokenType::NAME, "alpha");
  StyioToken* string = StyioToken::Create(StyioTokenType::STRING, "\"beta\"");
  StyioToken* integer = StyioToken::Create(StyioTokenType::INTEGER, "42");
  StyioToken* plus = StyioToken::Create(StyioTokenType::TOK_PLUS, "+");
  testing::internal::CaptureStdout();
  show_tokens({lf, space, name, string, integer, plus});
  const std::string token_dump = testing::internal::GetCapturedStdout();
  EXPECT_NE(token_dump.find("\\n"), std::string::npos);
  EXPECT_NE(token_dump.find("alpha"), std::string::npos);
  EXPECT_NE(token_dump.find("\"beta\""), std::string::npos);
  EXPECT_NE(token_dump.find("42"), std::string::npos);
  EXPECT_NE(token_dump.find("+"), std::string::npos);
  delete lf;
  delete space;
  delete name;
  delete string;
  delete integer;
  delete plus;

  const auto invalid_category = static_cast<StyioErrorCategory>(999);
  EXPECT_STREQ(styio_category_name(invalid_category), "RuntimeError");
  EXPECT_EQ(styio_diagnostic_code(invalid_category, "fallback runtime", ""), "STYIO_RUNTIME_ERROR");
  EXPECT_EQ(styio_category_phase(invalid_category), "runtime");
  EXPECT_EQ(styio_exit_code(invalid_category), static_cast<int>(StyioExitCode::RuntimeError));

  if (fs::exists("/dev/full")) {
    EXPECT_FALSE(styio_write_text_file_latest("/dev/full", "x", error));
    EXPECT_NE(error.find("failed to write file"), std::string::npos);
  }

  const fs::path file_repo_root = temp.path() / "repo-as-file";
  WriteText(file_repo_root, "not a directory\n");
  EXPECT_FALSE(styio_ensure_writable_nano_repository_latest(file_repo_root, error));
  EXPECT_NE(error.find("failed to create nano repository root"), std::string::npos);

  const fs::path bad_index_repo = temp.path() / "bad-index-repo";
  ASSERT_TRUE(styio_ensure_writable_nano_repository_latest(bad_index_repo, error)) << error;
  WriteText(bad_index_repo / "index" / "org", "blocks nested package dir\n");
  StyioNanoRepositoryEntryLatest entry;
  entry.package_name = "org/pkg";
  entry.version = "1.0.0";
  entry.channel = "nano";
  entry.sha256 = GoodSha('a');
  entry.size_bytes = 1;
  entry.blob_path = "blob.tar";
  EXPECT_FALSE(styio_write_nano_repository_entry_latest(bad_index_repo, entry, error));
  EXPECT_NE(error.find("failed to create nano repository index directory"), std::string::npos);

  bool include_pipeline_check = false;
  EXPECT_FALSE(styio_profile_cmake_includes_pipeline_check_latest(
    temp.path() / "missing-profile.cmake",
    include_pipeline_check,
    error));
  EXPECT_NE(error.find("cannot open file"), std::string::npos);
  const fs::path profile_without_pipeline = temp.path() / "profile-no-pipeline.cmake";
  WriteText(profile_without_pipeline, "set(STYIO_NANO_INCLUDE_PIPELINE_CHECK OFF)\n");
  ASSERT_TRUE(styio_profile_cmake_includes_pipeline_check_latest(
    profile_without_pipeline,
    include_pipeline_check,
    error)) << error;
  EXPECT_FALSE(include_pipeline_check);
  const fs::path profile_with_pipeline = temp.path() / "profile-with-pipeline.cmake";
  WriteText(profile_with_pipeline, "set(STYIO_NANO_INCLUDE_PIPELINE_CHECK ON)\n");
  ASSERT_TRUE(styio_profile_cmake_includes_pipeline_check_latest(
    profile_with_pipeline,
    include_pipeline_check,
    error)) << error;
  EXPECT_TRUE(include_pipeline_check);

  const fs::path source_root = temp.path() / "closure-source";
  for (const std::string& relpath : styio_nano_source_roots_latest(false)) {
    WriteText(source_root / relpath, "// " + relpath + "\n");
  }
  std::string include_relpath;
  WriteText(source_root / "src" / "local.hpp", "// local\n");
  EXPECT_TRUE(styio_resolve_local_include_relpath_latest(
    source_root,
    "src/main.cpp",
    "local.hpp",
    include_relpath));
  EXPECT_EQ(include_relpath, "src/local.hpp");
  WriteText(source_root / "src" / "StyioParser" / "Shared.hpp", "// shared\n");
  EXPECT_TRUE(styio_resolve_local_include_relpath_latest(
    source_root,
    "src/main.cpp",
    "StyioParser/Shared.hpp",
    include_relpath));
  EXPECT_EQ(include_relpath, "src/StyioParser/Shared.hpp");
  WriteText(source_root / "RootOnly.hpp", "// root\n");
  EXPECT_TRUE(styio_resolve_local_include_relpath_latest(
    source_root,
    "src/main.cpp",
    "RootOnly.hpp",
    include_relpath));
  EXPECT_EQ(include_relpath, "RootOnly.hpp");
  WriteText(temp.path() / "outside.hpp", "// outside\n");
  EXPECT_FALSE(styio_resolve_local_include_relpath_latest(
    source_root,
    "src/main.cpp",
    "../../outside.hpp",
    include_relpath));
  WriteText(source_root / "src" / "main.cpp", "#include \"missing-local.hpp\"\n");
  std::set<std::string> closure_files;
  EXPECT_FALSE(styio_collect_nano_closure_files_latest(source_root, false, closure_files, error));
  EXPECT_NE(error.find("failed to resolve local include 'missing-local.hpp'"), std::string::npos);

  EXPECT_FALSE(styio_copy_closure_files_latest(
    source_root,
    temp.path() / "closure-out",
    {"src/missing.cpp"},
    error));
  EXPECT_NE(error.find("failed to copy"), std::string::npos);

  EXPECT_FALSE(styio_generate_profile_cmake_latest(
    source_root,
    temp.path() / "profile.toml",
    temp.path() / "profile.cmake",
    error));
  EXPECT_NE(error.find("styio-nano profile generator not found"), std::string::npos);

  const fs::path copy_dest_as_file = temp.path() / "copy-dest-file";
  WriteText(copy_dest_as_file, "blocks directory creation\n");
  EXPECT_FALSE(styio_copy_directory_contents_latest(source_root, copy_dest_as_file, error));
  EXPECT_NE(error.find("failed to create directory"), std::string::npos);

  const fs::path blocked_local_output = temp.path() / "blocked-local-output";
  WriteText(blocked_local_output, "blocks local nano output directory\n");
  StyioNanoCreateSelectionLatest blocked_local_materialize;
  blocked_local_materialize.mode = "local-subset";
  blocked_local_materialize.output_dir = blocked_local_output.string();
  blocked_local_materialize.profile_path = (temp.path() / "unused.profile.toml").string();
  blocked_local_materialize.source_root = source_root.string();
  EXPECT_FALSE(styio_materialize_local_nano_package_latest(blocked_local_materialize, nullptr, error));
  EXPECT_NE(error.find("failed to create output directory"), std::string::npos);

  StyioNanoCreateSelectionLatest missing_profile;
  missing_profile.mode = "local-subset";
  missing_profile.output_dir = (temp.path() / "local-out").string();
  missing_profile.profile_path = (temp.path() / "missing.profile.toml").string();
  missing_profile.source_root = source_root.string();
  EXPECT_FALSE(styio_materialize_local_nano_package_latest(missing_profile, nullptr, error));
  EXPECT_NE(error.find("styio-nano profile not found"), std::string::npos);

  const fs::path profile = temp.path() / "profile.toml";
  WriteText(profile, "[profile]\nname = \"local\"\n");
  StyioNanoCreateSelectionLatest bad_source_root = missing_profile;
  bad_source_root.profile_path = profile.string();
  bad_source_root.source_root = (temp.path() / "not-a-repo").string();
  EXPECT_FALSE(styio_materialize_local_nano_package_latest(bad_source_root, nullptr, error));
  EXPECT_NE(error.find("does not look like a styio repository"), std::string::npos);

  std::string token;
  EXPECT_TRUE(styio_read_first_process_token_latest({"printf", "first second\n"}, token));
  EXPECT_EQ(token, "first");
  EXPECT_FALSE(styio_read_first_process_token_latest({"/bin/false"}, token));

  EnvVarGuard jobs_guard("STYIO_NANO_BUILD_JOBS");
  jobs_guard.unset();
  EXPECT_EQ(styio_nano_build_jobs_latest(), "2");
  jobs_guard.set("4");
  EXPECT_EQ(styio_nano_build_jobs_latest(), "4");
  jobs_guard.set("0");
  EXPECT_EQ(styio_nano_build_jobs_latest(), "2");
  jobs_guard.set("abc");
  EXPECT_EQ(styio_nano_build_jobs_latest(), "2");

  const fs::path fake_cmake_bin = temp.path() / "fake-cmake-bin";
  const fs::path fake_cmake = fake_cmake_bin / "cmake";
  EnvVarGuard fake_cmake_path_guard("PATH");
  fake_cmake_path_guard.set(fake_cmake_bin.string());
  const fs::path fake_nano_out = temp.path() / "fake-nano-out";
  WriteText(fake_nano_out / "CMakeLists.txt", "cmake_minimum_required(VERSION 3.16)\n");

  WriteText(fake_cmake, "#!/bin/sh\nexit 9\n");
  MakeExecutable(fake_cmake);
  EXPECT_FALSE(styio_build_nano_package_latest(fake_nano_out, error));
  EXPECT_NE(error.find("styio-nano package configure failed"), std::string::npos);

  WriteText(
    fake_cmake,
    "#!/bin/sh\n"
    "if [ \"$1\" = \"--build\" ]; then exit 8; fi\n"
    "exit 0\n");
  MakeExecutable(fake_cmake);
  EXPECT_FALSE(styio_build_nano_package_latest(fake_nano_out, error));
  EXPECT_NE(error.find("styio-nano package build failed"), std::string::npos);

  WriteText(fake_cmake, "#!/bin/sh\nexit 0\n");
  MakeExecutable(fake_cmake);
  EXPECT_FALSE(styio_build_nano_package_latest(fake_nano_out, error));
  EXPECT_NE(error.find("styio-nano package build did not produce"), std::string::npos);
}

TEST(StyioMainContract, LocalNanoMaterializationCoversStagedFailures) {
  TempDir temp("local-nano-materialize");
  std::string error;
  const std::string binary_name = styio_nano_binary_filename_latest();

  const fs::path source_root = temp.path() / "source";
  PopulateMinimalNanoSourceRoot(source_root);
  const fs::path profile = temp.path() / "profile.toml";
  WriteText(profile, "[profile]\nname = \"local\"\n");

  {
    const fs::path output = temp.path() / "copy-profile-failure";
    fs::create_directories(output / "styio-nano.profile.toml");
    StyioNanoCreateSelectionLatest selection = MakeLocalNanoSelection(output, profile, source_root);
    error.clear();
    EXPECT_FALSE(styio_materialize_local_nano_package_latest(selection, nullptr, error));
    EXPECT_NE(error.find("failed to copy"), std::string::npos) << error;
  }

  {
    StyioNanoCreateSelectionLatest selection =
      MakeLocalNanoSelection(temp.path() / "missing-generator", profile, source_root);
    error.clear();
    EXPECT_FALSE(styio_materialize_local_nano_package_latest(selection, nullptr, error));
    EXPECT_NE(error.find("profile generator not found"), std::string::npos) << error;
  }

  WriteNanoProfileGenerator(source_root, false);
  {
    StyioNanoCreateSelectionLatest selection =
      MakeLocalNanoSelection(temp.path() / "missing-profile-cmake", profile, source_root);
    error.clear();
    EXPECT_FALSE(styio_materialize_local_nano_package_latest(selection, nullptr, error));
    EXPECT_NE(error.find("cannot open file"), std::string::npos) << error;
  }

  WriteNanoProfileGenerator(source_root, true);
  {
    const fs::path missing_closure_root = temp.path() / "missing-closure-source";
    WriteText(missing_closure_root / "CMakeLists.txt", "cmake_minimum_required(VERSION 3.20)\n");
    WriteNanoProfileGenerator(missing_closure_root, true);
    StyioNanoCreateSelectionLatest selection =
      MakeLocalNanoSelection(temp.path() / "missing-closure", profile, missing_closure_root);
    error.clear();
    EXPECT_FALSE(styio_materialize_local_nano_package_latest(selection, nullptr, error));
    EXPECT_NE(error.find("nano source closure is missing required file"), std::string::npos) << error;
  }

  {
    const fs::path output = temp.path() / "closure-copy-failure";
    WriteText(output / "styio-nano.profile.toml", "");
    WriteText(output / "styio_nano_profile.cmake", "");
    std::error_code create_ec;
    fs::create_directories(output / "bin", create_ec);
    ASSERT_FALSE(create_ec) << create_ec.message();
    std::error_code perm_ec;
    fs::permissions(
      output,
      fs::perms::owner_write | fs::perms::group_write | fs::perms::others_write,
      fs::perm_options::remove,
      perm_ec);
    ASSERT_FALSE(perm_ec) << perm_ec.message();
    StyioNanoCreateSelectionLatest selection = MakeLocalNanoSelection(output, profile, source_root);
    error.clear();
    EXPECT_FALSE(styio_materialize_local_nano_package_latest(selection, nullptr, error));
    EXPECT_NE(error.find("failed to copy"), std::string::npos) << error;
    fs::permissions(output, fs::perms::owner_write, fs::perm_options::add, perm_ec);
  }

  {
    const fs::path output = temp.path() / "manifest-write-failure";
    fs::create_directories(output / "source-closure-manifest.txt");
    StyioNanoCreateSelectionLatest selection = MakeLocalNanoSelection(output, profile, source_root);
    error.clear();
    EXPECT_FALSE(styio_materialize_local_nano_package_latest(selection, nullptr, error));
    EXPECT_NE(error.find("cannot open file for writing"), std::string::npos) << error;
  }

  {
    const fs::path output = temp.path() / "cmakelists-write-failure";
    fs::create_directories(output / "CMakeLists.txt");
    StyioNanoCreateSelectionLatest selection = MakeLocalNanoSelection(output, profile, source_root);
    error.clear();
    EXPECT_FALSE(styio_materialize_local_nano_package_latest(selection, nullptr, error));
    EXPECT_NE(error.find("cannot open file for writing"), std::string::npos) << error;
  }

  {
    const fs::path output = temp.path() / "helper-write-failure";
    fs::create_directories(output / "build-styio-nano.sh");
    StyioNanoCreateSelectionLatest selection = MakeLocalNanoSelection(output, profile, source_root);
    error.clear();
    EXPECT_FALSE(styio_materialize_local_nano_package_latest(selection, nullptr, error));
    EXPECT_NE(error.find("cannot open file for writing"), std::string::npos) << error;
  }

  const fs::path fake_cmake_bin = temp.path() / "fake-cmake-bin-local";
  const fs::path fake_cmake = fake_cmake_bin / "cmake";
  EnvVarGuard path_guard("PATH");
  const char* original_path_raw = std::getenv("PATH");
  const std::string original_path = original_path_raw == nullptr ? "" : original_path_raw;
  path_guard.set(fake_cmake_bin.string() + (original_path.empty() ? "" : ":" + original_path));

  WriteText(
    fake_cmake,
    "#!/bin/sh\n"
    "if [ \"$1\" = \"--build\" ]; then exit 8; fi\n"
    "exit 0\n");
  MakeExecutable(fake_cmake);
  {
    StyioNanoCreateSelectionLatest selection =
      MakeLocalNanoSelection(temp.path() / "build-failure", profile, source_root);
    error.clear();
    EXPECT_FALSE(styio_materialize_local_nano_package_latest(selection, nullptr, error));
    EXPECT_NE(error.find("styio-nano package build failed"), std::string::npos) << error;
  }

  WriteText(
    fake_cmake,
    "#!/bin/sh\n"
    "if [ \"$1\" = \"--build\" ]; then\n"
    "  build_dir=\"$2\"\n"
    "  mkdir -p \"$build_dir/bin\"\n"
    "  printf '#!/bin/sh\\nexit 0\\n' > \"$build_dir/bin/" + binary_name + "\"\n"
    "  chmod +x \"$build_dir/bin/" + binary_name + "\"\n"
    "fi\n"
    "exit 0\n");
  MakeExecutable(fake_cmake);
  {
    const fs::path output = temp.path() / "receipt-write-failure";
    fs::create_directories(output / "styio-nano-package.toml");
    StyioNanoCreateSelectionLatest selection = MakeLocalNanoSelection(output, profile, source_root);
    error.clear();
    EXPECT_FALSE(styio_materialize_local_nano_package_latest(selection, "styio", error));
    EXPECT_NE(error.find("cannot open file for writing"), std::string::npos) << error;
  }

  {
    const fs::path output = temp.path() / "success";
    StyioNanoCreateSelectionLatest selection = MakeLocalNanoSelection(output, profile, source_root);
    error.clear();
    ASSERT_TRUE(styio_materialize_local_nano_package_latest(selection, nullptr, error)) << error;
    EXPECT_TRUE(styio_native_build_is_executable_file_latest(output / "bin" / binary_name));
    const std::string receipt = ReadText(output / "styio-nano-package.toml");
    EXPECT_NE(receipt.find("name = \"local\""), std::string::npos);
    EXPECT_NE(receipt.find("compiler = \"\""), std::string::npos);
  }
}

TEST(StyioMainContract, NanoSelectionAndCompilePlanHelpersCoverDirectBranches) {
  TempDir temp("nano-selection");
  std::string error;

  auto no_nano_args = ParseMainOptions({"styio"});
  EXPECT_FALSE(styio_cli_mentions_any_nano_packaging_args_latest(no_nano_args));
  auto nano_create_args = ParseMainOptions({"styio", "--nano-create"});
  EXPECT_TRUE(styio_cli_mentions_any_nano_packaging_args_latest(nano_create_args));

  StyioNanoCreateSelectionLatest create_selection;
  auto unsupported_mode = ParseMainOptions({
    "styio",
    "--nano-mode=embedded",
    "--nano-output=" + (temp.path() / "out").string()});
  EXPECT_FALSE(styio_resolve_nano_create_selection_latest(
    unsupported_mode,
    nullptr,
    create_selection,
    error));
  EXPECT_NE(error.find("unsupported --nano-mode"), std::string::npos);

  create_selection = StyioNanoCreateSelectionLatest{};
  auto empty_package_config = ParseMainOptions({"styio", "--nano-package-config="});
  EXPECT_FALSE(styio_resolve_nano_create_selection_latest(
    empty_package_config,
    nullptr,
    create_selection,
    error));
  EXPECT_NE(error.find("--nano-package-config requires a non-empty path"), std::string::npos);

  const fs::path bad_create_config = temp.path() / "bad-create-config.toml";
  WriteText(bad_create_config, "[nano\nmode = \"cloud\"\n");
  create_selection = StyioNanoCreateSelectionLatest{};
  auto malformed_package_config = ParseMainOptions({
    "styio",
    "--nano-package-config=" + bad_create_config.string()});
  EXPECT_FALSE(styio_resolve_nano_create_selection_latest(
    malformed_package_config,
    nullptr,
    create_selection,
    error));
  EXPECT_NE(error.find("malformed section header"), std::string::npos);

  create_selection = StyioNanoCreateSelectionLatest{};
  auto missing_output = ParseMainOptions({"styio", "--nano-mode=cloud"});
  EXPECT_FALSE(styio_resolve_nano_create_selection_latest(
    missing_output,
    nullptr,
    create_selection,
    error));
  EXPECT_NE(error.find("requires --nano-output"), std::string::npos);

  create_selection = StyioNanoCreateSelectionLatest{};
  auto local_missing_profile = ParseMainOptions({
    "styio",
    "--nano-mode=local-subset",
    "--nano-output=" + (temp.path() / "local").string()});
  EXPECT_FALSE(styio_resolve_nano_create_selection_latest(
    local_missing_profile,
    nullptr,
    create_selection,
    error));
  EXPECT_NE(error.find("local-subset styio-nano creation requires --nano-profile"), std::string::npos);

  create_selection = StyioNanoCreateSelectionLatest{};
  auto cloud_missing_registry = ParseMainOptions({
    "styio",
    "--nano-mode=cloud",
    "--nano-output=" + (temp.path() / "cloud").string()});
  EXPECT_FALSE(styio_resolve_nano_create_selection_latest(
    cloud_missing_registry,
    nullptr,
    create_selection,
    error));
  EXPECT_NE(error.find("requires either --nano-manifest"), std::string::npos);

  const fs::path profile = temp.path() / "local.profile.toml";
  WriteText(profile, "[profile]\nname = \"profile-name\"\n");
  create_selection = StyioNanoCreateSelectionLatest{};
  auto local_success = ParseMainOptions({
    "styio",
    "--nano-mode=local-subset",
    "--nano-output=" + (temp.path() / "local-out").string(),
    "--nano-profile=" + profile.string()});
  ASSERT_TRUE(styio_resolve_nano_create_selection_latest(
    local_success,
    "styio",
    create_selection,
    error)) << error;
  EXPECT_EQ(create_selection.package_name, "profile-name");
  EXPECT_FALSE(create_selection.source_root.empty());

  const fs::path cli_source_root = temp.path() / "cli-source-root";
  create_selection = StyioNanoCreateSelectionLatest{};
  auto local_cli_overrides = ParseMainOptions({
    "styio",
    "--nano-mode=local-subset",
    "--nano-output=" + (temp.path() / "local-cli").string(),
    "--nano-profile=" + profile.string(),
    "--nano-name=cli-name",
    "--nano-source-root=" + cli_source_root.string()});
  ASSERT_TRUE(styio_resolve_nano_create_selection_latest(
    local_cli_overrides,
    "styio",
    create_selection,
    error)) << error;
  EXPECT_EQ(create_selection.package_name, "cli-name");
  EXPECT_EQ(create_selection.source_root, styio_absolute_path_latest(cli_source_root).string());

  create_selection = StyioNanoCreateSelectionLatest{};
  auto local_default_name = ParseMainOptions({
    "styio",
    "--nano-mode=local-subset",
    "--nano-output=" + (temp.path() / "local-default-name").string(),
    "--nano-profile=" + (temp.path() / "missing-default.profile.toml").string()});
  ASSERT_TRUE(styio_resolve_nano_create_selection_latest(
    local_default_name,
    nullptr,
    create_selection,
    error)) << error;
  EXPECT_EQ(create_selection.package_name, "styio-nano");

  const fs::path configured_source_root = temp.path() / "configured-source";
  const fs::path package_config = temp.path() / "nano-package.toml";
  WriteText(
    package_config,
    "[nano]\n"
    "mode = \"local-subset\"\n"
    "output = \"configured-out\"\n"
    "name = \"configured-name\"\n"
    "[nano.local]\n"
    "profile = \"local.profile.toml\"\n"
    "binary = \"bin/config-nano\"\n"
    "source_root = \"configured-source\"\n");
  create_selection = StyioNanoCreateSelectionLatest{};
  auto configured_local = ParseMainOptions({
    "styio",
    "--nano-package-config=" + package_config.string(),
    "--nano-binary=" + (temp.path() / "cli-nano").string()});
  ASSERT_TRUE(styio_resolve_nano_create_selection_latest(
    configured_local,
    nullptr,
    create_selection,
    error)) << error;
  EXPECT_EQ(create_selection.config_path, package_config.string());
  EXPECT_EQ(create_selection.mode, "local-subset");
  EXPECT_EQ(create_selection.output_dir, styio_absolute_path_latest(temp.path() / "configured-out").string());
  EXPECT_EQ(create_selection.package_name, "configured-name");
  EXPECT_EQ(create_selection.profile_path, styio_absolute_path_latest(profile).string());
  EXPECT_EQ(create_selection.source_root, styio_absolute_path_latest(configured_source_root).string());
  EXPECT_EQ(create_selection.binary_path, styio_absolute_path_latest(temp.path() / "cli-nano").string());

  create_selection = StyioNanoCreateSelectionLatest{};
  auto cloud_manifest_success = ParseMainOptions({
    "styio",
    "--nano-mode=cloud",
    "--nano-output=" + (temp.path() / "manifest-out").string(),
    "--nano-manifest=manifest.toml"});
  ASSERT_TRUE(styio_resolve_nano_create_selection_latest(
    cloud_manifest_success,
    nullptr,
    create_selection,
    error)) << error;
  EXPECT_TRUE(create_selection.package_name.empty());
  EXPECT_EQ(create_selection.manifest_ref, styio_absolute_path_latest("manifest.toml").string());

  create_selection = StyioNanoCreateSelectionLatest{};
  auto cloud_conflict = ParseMainOptions({
    "styio",
    "--nano-mode=cloud",
    "--nano-output=" + (temp.path() / "cloud-conflict").string(),
    "--nano-manifest=manifest.toml",
    "--nano-registry=" + (temp.path() / "repo").string(),
    "--nano-package=org/pkg",
    "--nano-version=1.2.3"});
  EXPECT_FALSE(styio_resolve_nano_create_selection_latest(
    cloud_conflict,
    nullptr,
    create_selection,
    error));
  EXPECT_NE(error.find("accepts either nano.cloud.manifest"), std::string::npos);

  create_selection = StyioNanoCreateSelectionLatest{};
  auto cloud_registry_success = ParseMainOptions({
    "styio",
    "--nano-mode=cloud",
    "--nano-output=" + (temp.path() / "registry-out").string(),
    "--nano-registry=" + (temp.path() / "repo///").string(),
    "--nano-package=org/pkg",
    "--nano-version=1.2.3"});
  ASSERT_TRUE(styio_resolve_nano_create_selection_latest(
    cloud_registry_success,
    nullptr,
    create_selection,
    error)) << error;
  EXPECT_EQ(create_selection.package_name, "pkg");
  EXPECT_EQ(create_selection.registry_root, styio_absolute_path_latest(temp.path() / "repo").string());

  const std::string binary_name = styio_nano_binary_filename_latest();
  const fs::path package_dir = temp.path() / "package";
  WriteText(package_dir / "bin" / binary_name, "#!/usr/bin/env sh\nexit 0\n");

  StyioNanoPublishSelectionLatest publish_selection;
  auto publish_missing_dir = ParseMainOptions({"styio"});
  EXPECT_FALSE(styio_resolve_nano_publish_selection_latest(
    publish_missing_dir,
    publish_selection,
    error));
  EXPECT_NE(error.find("publish requires --nano-package-dir"), std::string::npos);

  publish_selection = StyioNanoPublishSelectionLatest{};
  auto empty_publish_config = ParseMainOptions({"styio", "--nano-publish-config="});
  EXPECT_FALSE(styio_resolve_nano_publish_selection_latest(
    empty_publish_config,
    publish_selection,
    error));
  EXPECT_NE(error.find("--nano-publish-config requires a non-empty path"), std::string::npos);

  const fs::path bad_publish_config = temp.path() / "bad-resolve-publish.toml";
  WriteText(bad_publish_config, "[publish\npackage_dir = \"pkg\"\n");
  publish_selection = StyioNanoPublishSelectionLatest{};
  auto malformed_publish_config = ParseMainOptions({
    "styio",
    "--nano-publish-config=" + bad_publish_config.string()});
  EXPECT_FALSE(styio_resolve_nano_publish_selection_latest(
    malformed_publish_config,
    publish_selection,
    error));
  EXPECT_NE(error.find("malformed section header"), std::string::npos);

  publish_selection = StyioNanoPublishSelectionLatest{};
  auto publish_missing_package_dir = ParseMainOptions({
    "styio",
    "--nano-package-dir=" + (temp.path() / "missing-package").string(),
    "--nano-registry=" + (temp.path() / "repo").string()});
  EXPECT_FALSE(styio_resolve_nano_publish_selection_latest(
    publish_missing_package_dir,
    publish_selection,
    error));
  EXPECT_NE(error.find("styio-nano package directory not found"), std::string::npos);

  const fs::path package_without_binary = temp.path() / "package-without-binary";
  fs::create_directories(package_without_binary / "bin");
  publish_selection = StyioNanoPublishSelectionLatest{};
  auto publish_missing_binary = ParseMainOptions({
    "styio",
    "--nano-package-dir=" + package_without_binary.string(),
    "--nano-registry=" + (temp.path() / "repo").string()});
  EXPECT_FALSE(styio_resolve_nano_publish_selection_latest(
    publish_missing_binary,
    publish_selection,
    error));
  EXPECT_NE(error.find("missing bin/" + binary_name), std::string::npos);

  publish_selection = StyioNanoPublishSelectionLatest{};
  auto publish_missing_registry = ParseMainOptions({
    "styio",
    "--nano-package-dir=" + package_dir.string()});
  EXPECT_FALSE(styio_resolve_nano_publish_selection_latest(
    publish_missing_registry,
    publish_selection,
    error));
  EXPECT_NE(error.find("publish requires --nano-registry"), std::string::npos);

  publish_selection = StyioNanoPublishSelectionLatest{};
  auto publish_http_registry = ParseMainOptions({
    "styio",
    "--nano-package-dir=" + package_dir.string(),
    "--nano-registry=https://example.test/repo"});
  EXPECT_FALSE(styio_resolve_nano_publish_selection_latest(
    publish_http_registry,
    publish_selection,
    error));
  EXPECT_NE(error.find("only supports local repository roots"), std::string::npos);

  WriteText(
    package_dir / "styio-nano-package.toml",
    "[package]\n"
    "name = \"receipt-name\"\n"
    "[artifact]\n"
    "binary = \"bin/" + binary_name + "\"\n");
  publish_selection = StyioNanoPublishSelectionLatest{};
  auto publish_missing_version = ParseMainOptions({
    "styio",
    "--nano-package-dir=" + package_dir.string(),
    "--nano-registry=" + (temp.path() / "repo").string()});
  EXPECT_FALSE(styio_resolve_nano_publish_selection_latest(
    publish_missing_version,
    publish_selection,
    error));
  EXPECT_NE(error.find("requires --nano-version"), std::string::npos);

  WriteText(package_dir / "styio-nano-package.toml", "[package\nname = bad\n");
  publish_selection = StyioNanoPublishSelectionLatest{};
  auto publish_bad_receipt = ParseMainOptions({
    "styio",
    "--nano-package-dir=" + package_dir.string(),
    "--nano-registry=" + (temp.path() / "repo").string(),
    "--nano-package=org/bad",
    "--nano-version=0.0.1"});
  EXPECT_FALSE(styio_resolve_nano_publish_selection_latest(
    publish_bad_receipt,
    publish_selection,
    error));
  EXPECT_NE(error.find("malformed section header"), std::string::npos);

  WriteText(
    package_dir / "styio-nano-package.toml",
    "[package]\n"
    "name = \"receipt-name\"\n"
    "version = \"7.8.9\"\n"
    "channel = \"beta\"\n"
    "[artifact]\n"
    "binary = \"bin/" + binary_name + "\"\n");
  publish_selection = StyioNanoPublishSelectionLatest{};
  auto publish_success = ParseMainOptions({
    "styio",
    "--nano-package-dir=" + package_dir.string(),
    "--nano-registry=file://" + (temp.path() / "repo").string()});
  ASSERT_TRUE(styio_resolve_nano_publish_selection_latest(
    publish_success,
    publish_selection,
    error)) << error;
  EXPECT_EQ(publish_selection.registry_package, "receipt-name");
  EXPECT_EQ(publish_selection.registry_version, "7.8.9");
  EXPECT_EQ(publish_selection.channel, "beta");
  EXPECT_EQ(publish_selection.registry_root, "file://" + (temp.path() / "repo").string());

  publish_selection = StyioNanoPublishSelectionLatest{};
  auto publish_cli_overrides = ParseMainOptions({
    "styio",
    "--nano-package-dir=" + package_dir.string(),
    "--nano-registry=" + (temp.path() / "repo2").string(),
    "--nano-package=org/cli",
    "--nano-version=1.0.0",
    "--nano-channel=edge"});
  ASSERT_TRUE(styio_resolve_nano_publish_selection_latest(
    publish_cli_overrides,
    publish_selection,
    error)) << error;
  EXPECT_EQ(publish_selection.registry_package, "org/cli");
  EXPECT_EQ(publish_selection.registry_version, "1.0.0");
  EXPECT_EQ(publish_selection.channel, "edge");
  EXPECT_EQ(publish_selection.registry_root, styio_absolute_path_latest(temp.path() / "repo2").string());

  std::error_code remove_ec;
  fs::remove(package_dir / "styio-nano-package.toml", remove_ec);
  ASSERT_FALSE(remove_ec) << remove_ec.message();
  publish_selection = StyioNanoPublishSelectionLatest{};
  auto publish_defaults_without_receipt = ParseMainOptions({
    "styio",
    "--nano-package-dir=" + package_dir.string(),
    "--nano-registry=" + (temp.path() / "repo3").string(),
    "--nano-version=2.0.0"});
  ASSERT_TRUE(styio_resolve_nano_publish_selection_latest(
    publish_defaults_without_receipt,
    publish_selection,
    error)) << error;
  EXPECT_EQ(publish_selection.registry_package, package_dir.filename().string());
  EXPECT_EQ(publish_selection.registry_version, "2.0.0");
  EXPECT_EQ(publish_selection.channel, "nano");

  publish_selection = StyioNanoPublishSelectionLatest{};
  auto publish_trailing_slash_without_name = ParseMainOptions({
    "styio",
    "--nano-package-dir=" + package_dir.string() + "/",
    "--nano-registry=" + (temp.path() / "repo4").string(),
    "--nano-version=3.0.0"});
  EXPECT_FALSE(styio_resolve_nano_publish_selection_latest(
    publish_trailing_slash_without_name,
    publish_selection,
    error));
  EXPECT_NE(error.find("publish requires --nano-package"), std::string::npos);

  const fs::path project_config = temp.path() / "styio.toml";
  WriteText(project_config, "dict_impl = \"linear\"\n");
  StyioDictImplSelectionLatest dict_selection;
  auto dict_from_config = ParseMainOptions({"styio", "--config=" + project_config.string()});
  ASSERT_TRUE(styio_resolve_dict_impl_selection_latest(
    dict_from_config,
    "",
    dict_selection,
    error)) << error;
  EXPECT_EQ(dict_selection.source, "project-config");
  EXPECT_EQ(dict_selection.config_path, project_config.string());

  testing::internal::CaptureStdout();
  styio_emit_machine_info_json(dict_selection);
  const std::string machine_info_with_config = testing::internal::GetCapturedStdout();
  EXPECT_NE(machine_info_with_config.find("\"config_file\":\"" + project_config.string() + "\""), std::string::npos);

  dict_selection = StyioDictImplSelectionLatest{};
  auto dict_from_cli = ParseMainOptions({"styio", "--dict-impl=linear"});
  ASSERT_TRUE(styio_resolve_dict_impl_selection_latest(
    dict_from_cli,
    "",
    dict_selection,
    error)) << error;
  EXPECT_EQ(dict_selection.impl_name, "linear");
  EXPECT_EQ(dict_selection.source, "cli");

  dict_selection = StyioDictImplSelectionLatest{};
  auto dict_default = ParseMainOptions({"styio"});
  ASSERT_TRUE(styio_resolve_dict_impl_selection_latest(
    dict_default,
    "",
    dict_selection,
    error)) << error;
  EXPECT_EQ(dict_selection.impl_name, "ordered-hash");

  testing::internal::CaptureStdout();
  styio_emit_machine_info_json(dict_selection);
  const std::string machine_info_default = testing::internal::GetCapturedStdout();
  EXPECT_NE(machine_info_default.find("\"dict_impl\":{\"selected\":\"ordered-hash\""), std::string::npos);

  dict_selection = StyioDictImplSelectionLatest{};
  auto empty_dict_config = ParseMainOptions({"styio", "--config="});
  EXPECT_FALSE(styio_resolve_dict_impl_selection_latest(
    empty_dict_config,
    "",
    dict_selection,
    error));
  EXPECT_NE(error.find("--config requires a non-empty path"), std::string::npos);

  dict_selection = StyioDictImplSelectionLatest{};
  auto bad_dict_cli = ParseMainOptions({"styio", "--dict-impl=missing-impl"});
  EXPECT_FALSE(styio_resolve_dict_impl_selection_latest(
    bad_dict_cli,
    "",
    dict_selection,
    error));
  EXPECT_NE(error.find("unsupported --dict-impl"), std::string::npos);

  const fs::path unsupported_project_config = temp.path() / "unsupported-dict.toml";
  WriteText(unsupported_project_config, "dict_impl = \"missing-impl\"\n");
  dict_selection = StyioDictImplSelectionLatest{};
  auto unsupported_dict_config = ParseMainOptions({"styio", "--config=" + unsupported_project_config.string()});
  EXPECT_FALSE(styio_resolve_dict_impl_selection_latest(
    unsupported_dict_config,
    "",
    dict_selection,
    error));
  EXPECT_NE(error.find("unsupported dict_impl in config file"), std::string::npos);

  const fs::path discovered_bad_config = temp.path() / "project" / "styio.toml";
  const fs::path discovered_source = temp.path() / "project" / "src" / "main.styio";
  WriteText(discovered_bad_config, "[dict]\nimpl = two words\n");
  WriteText(discovered_source, "print(1)\n");
  dict_selection = StyioDictImplSelectionLatest{};
  auto discover_bad_config = ParseMainOptions({"styio"});
  EXPECT_FALSE(styio_resolve_dict_impl_selection_latest(
    discover_bad_config,
    discovered_source.string(),
    dict_selection,
    error));
  EXPECT_NE(error.find("invalid dict_impl value"), std::string::npos);

  StyioCompilePlanRequestLatest request;
  request.plan_version = 1;
  request.intent = "test";
  request.entry_package_id = "pkg/main";
  request.entry_target_kind = "test";
  request.entry_target_name = "unit/name";
  request.entry_file = temp.path() / "src" / "main.styio";
  request.build_root = temp.path() / "build";
  request.artifact_dir = temp.path() / "artifacts";
  request.diag_dir = temp.path() / "diag";

  EXPECT_EQ(styio_compile_plan_unit_id_latest(request), "pkg/main::test:unit/name");
  bool success = true;
  bool executed = false;
  const CompilationPhase final_phase = CompilationPhase::Typed;
  const std::string payload = styio_render_compile_plan_unit_payload_latest(
    request,
    "test",
    &success,
    &executed,
    &final_phase);
  EXPECT_NE(payload.find("\"test_name\":\"unit/name\""), std::string::npos);
  EXPECT_NE(payload.find("\"success\":true"), std::string::npos);
  EXPECT_NE(payload.find("\"executed\":false"), std::string::npos);
  EXPECT_NE(payload.find("\"final_phase\":\"typed\""), std::string::npos);
  EXPECT_EQ(styio_compile_plan_artifact_stem_latest(request), "unit_name");

  request.entry_target_name.clear();
  EXPECT_EQ(styio_compile_plan_artifact_stem_latest(request), "main");
  request.entry_file = fs::path();
  EXPECT_EQ(styio_compile_plan_artifact_stem_latest(request), "entry");
  request.entry_target_kind.clear();
  request.entry_package_id.clear();
  EXPECT_TRUE(styio_compile_plan_unit_id_latest(request).empty());

  ASSERT_TRUE(styio_write_compile_plan_artifact_latest(
    temp.path() / "artifact" / "out.txt",
    "artifact\n",
    error)) << error;
  ASSERT_TRUE(styio_write_compile_plan_artifact_latest(
    temp.path() / "artifact" / "second.txt",
    "second\n",
    error)) << error;
  EXPECT_EQ(ReadText(temp.path() / "artifact" / "out.txt"), "artifact\n");

  request.entry_package_id = "pkg/main";
  request.entry_target_kind = "bin";
  request.entry_target_name = "app";
  request.entry_file = temp.path() / "src" / "main.styio";
  ASSERT_TRUE(styio_write_compile_plan_receipt_latest(
    request,
    {temp.path() / "artifact" / "out.txt", temp.path() / "artifact" / "second.txt"},
    "linear",
    "session-1",
    true,
    12,
    error)) << error;
  EXPECT_NE(ReadText(request.build_root / "receipt.json").find("\"session_id\":\"session-1\""), std::string::npos);

  const fs::path blocked_artifact_parent = temp.path() / "blocked-artifact-parent";
  WriteText(blocked_artifact_parent, "not a directory\n");
  EXPECT_FALSE(styio_write_compile_plan_artifact_latest(
    blocked_artifact_parent / "out.txt",
    "x",
    error));
  EXPECT_NE(error.find("cannot create artifact directory"), std::string::npos);
}

TEST(StyioMainContract, NanoRepositoryPublishAndCloudMaterializeRoundTrip) {
  TempDir temp("nano-repository-roundtrip");
  std::string error;
  const std::string binary_name = styio_nano_binary_filename_latest();

  const fs::path package_dir = temp.path() / "package";
  WriteText(package_dir / "bin" / binary_name, "#!/usr/bin/env sh\nprintf nano\n");
  MakeExecutable(package_dir / "bin" / binary_name);
  WriteText(package_dir / "styio-nano.profile.toml", "[profile]\nname = \"demo\"\n");
  WriteText(
    package_dir / "styio-nano-package.toml",
    "[package]\n"
    "name = \"org/demo\"\n"
    "version = \"1.2.3\"\n"
    "channel = \"edge\"\n"
    "[artifact]\n"
    "binary = \"bin/" + binary_name + "\"\n"
    "profile = \"styio-nano.profile.toml\"\n");

  StyioNanoPublishSelectionLatest publish_selection;
  publish_selection.package_dir = package_dir.string();
  publish_selection.registry_root = (temp.path() / "repo").string();
  publish_selection.registry_package = "org/demo";
  publish_selection.registry_version = "1.2.3";
  publish_selection.channel = "edge";
  ASSERT_TRUE(styio_publish_nano_package_latest(publish_selection, error)) << error;

  const fs::path repo = temp.path() / "repo";
  EXPECT_TRUE(fs::exists(repo / styio_nano_repository_marker_relpath_latest()));
  EXPECT_TRUE(fs::exists(repo / fs::path(styio_nano_repository_entry_relpath_latest("org/demo"))));

  StyioNanoCreateSelectionLatest registry_selection;
  registry_selection.mode = "cloud";
  registry_selection.output_dir = (temp.path() / "from-registry").string();
  registry_selection.registry_root = repo.string();
  registry_selection.registry_package = "org/demo";
  registry_selection.registry_version = "1.2.3";
  ASSERT_TRUE(styio_materialize_cloud_nano_package_latest(registry_selection, error)) << error;
  EXPECT_TRUE(styio_native_build_is_executable_file_latest(
    temp.path() / "from-registry" / "bin" / binary_name));
  const std::string registry_receipt = ReadText(temp.path() / "from-registry" / "styio-nano-package.toml");
  EXPECT_NE(registry_receipt.find("mode = \"cloud\""), std::string::npos);
  EXPECT_NE(registry_receipt.find("registry = \"" + repo.string() + "\""), std::string::npos);
  EXPECT_NE(registry_receipt.find("published_at = \""), std::string::npos);

  const fs::path manifest_root = temp.path() / "manifest-root";
  WriteText(manifest_root / "payload" / binary_name, "#!/usr/bin/env sh\nprintf manifest\n");
  MakeExecutable(manifest_root / "payload" / binary_name);
  WriteText(manifest_root / "payload" / "profile.toml", "[profile]\nname = \"manifest\"\n");
  WriteText(
    manifest_root / "manifest.toml",
    "[package]\n"
    "name = \"manifest-demo\"\n"
    "version = \"9.9.9\"\n"
    "channel = \"stable\"\n"
    "[artifact]\n"
    "binary = \"payload/" + binary_name + "\"\n"
    "profile = \"payload/profile.toml\"\n");

  StyioNanoCreateSelectionLatest manifest_selection;
  manifest_selection.mode = "cloud";
  manifest_selection.output_dir = (temp.path() / "from-manifest").string();
  manifest_selection.manifest_ref = (manifest_root / "manifest.toml").string();
  ASSERT_TRUE(styio_materialize_cloud_nano_package_latest(manifest_selection, error)) << error;
  EXPECT_TRUE(styio_native_build_is_executable_file_latest(
    temp.path() / "from-manifest" / "bin" / binary_name));
  EXPECT_EQ(
    ReadText(temp.path() / "from-manifest" / "styio-nano.profile.toml"),
    "[profile]\nname = \"manifest\"\n");
  const std::string manifest_receipt = ReadText(temp.path() / "from-manifest" / "styio-nano-package.toml");
  EXPECT_NE(manifest_receipt.find("name = \"manifest-demo\""), std::string::npos);
  EXPECT_NE(manifest_receipt.find("artifact_profile = \"payload/profile.toml\""), std::string::npos);

  const fs::path bad_manifest = temp.path() / "bad-manifest.toml";
  WriteText(bad_manifest, "[package\nname = bad\n");
  StyioNanoCreateSelectionLatest bad_manifest_selection;
  bad_manifest_selection.mode = "cloud";
  bad_manifest_selection.output_dir = (temp.path() / "bad-manifest").string();
  bad_manifest_selection.manifest_ref = bad_manifest.string();
  EXPECT_FALSE(styio_materialize_cloud_nano_package_latest(bad_manifest_selection, error));
  EXPECT_NE(error.find("malformed section header"), std::string::npos);
}

TEST(StyioMainContract, NanoPackageArchiveHelpersCoverExtractionAndCMakeEdges) {
  TempDir temp("nano-archive-helpers");
  std::string error;
  const std::string binary_name = styio_nano_binary_filename_latest();

  fs::path package_root;
  WriteText(temp.path() / "direct" / "bin" / binary_name, "#!/usr/bin/env sh\nexit 0\n");
  EXPECT_TRUE(styio_resolve_extracted_nano_package_root_latest(
    temp.path() / "direct",
    package_root,
    error));
  EXPECT_EQ(package_root, temp.path() / "direct");

  WriteText(temp.path() / "nested" / "only-child" / "bin" / binary_name, "#!/usr/bin/env sh\nexit 0\n");
  EXPECT_TRUE(styio_resolve_extracted_nano_package_root_latest(
    temp.path() / "nested",
    package_root,
    error));
  EXPECT_EQ(package_root, temp.path() / "nested" / "only-child");

  fs::create_directories(temp.path() / "empty-extract");
  EXPECT_FALSE(styio_resolve_extracted_nano_package_root_latest(
    temp.path() / "empty-extract",
    package_root,
    error));
  EXPECT_NE(error.find("does not contain bin/" + binary_name), std::string::npos);

  std::set<std::string> empty_closure;
  EXPECT_FALSE(styio_write_nano_package_cmakelists_latest(
    temp.path() / "empty-cmake",
    empty_closure,
    error));
  EXPECT_NE(error.find("does not contain any C++ sources"), std::string::npos);

  std::set<std::string> closure_files = {
    "src/StyioToken/Token.cpp",
    "src/main.cpp",
    "src/StyioToken/Token.hpp",
  };
  fs::create_directories(temp.path() / "cmake");
  ASSERT_TRUE(styio_write_nano_package_cmakelists_latest(
    temp.path() / "cmake",
    closure_files,
    error)) << error;
  const std::string cmake_text = ReadText(temp.path() / "cmake" / "CMakeLists.txt");
  EXPECT_LT(cmake_text.find("  src/main.cpp\n"), cmake_text.find("  src/StyioToken/Token.cpp\n"));
  EXPECT_NE(cmake_text.find("add_executable(styio_nano"), std::string::npos);
}

TEST(StyioMainContract, NanoLocalSubsetMaterializeCoversClosureAndBuildSuccess) {
  TempDir temp("nano-local-materialize");
  std::string error;
  const fs::path source_root = temp.path() / "source";
  const fs::path fake_bin = temp.path() / "fake-bin";
  const std::string binary_name = styio_nano_binary_filename_latest();

  WriteText(source_root / "CMakeLists.txt", "cmake_minimum_required(VERSION 3.20)\n");
  for (const std::string& relpath : styio_nano_source_roots_latest(true)) {
    WriteText(source_root / relpath, "// " + relpath + "\n");
  }
  WriteText(
    source_root / "src" / "main.cpp",
    "#include \"StyioToken/Token.hpp\"\n"
    "#include \"StyioNative/NativeToolchainConfig.hpp\"\n"
    "#include \"llvm/IR/Module.h\"\n");
  WriteText(source_root / "src" / "StyioToken" / "Token.hpp", "// token header\n");
  WriteText(
    source_root / "scripts" / "gen-styio-nano-profile.py",
    "import pathlib, sys\n"
    "out = pathlib.Path(sys.argv[sys.argv.index('--cmake-out') + 1])\n"
    "out.parent.mkdir(parents=True, exist_ok=True)\n"
    "out.write_text('set(STYIO_NANO_INCLUDE_PIPELINE_CHECK ON)\\n', encoding='utf-8')\n");

  WriteText(
    fake_bin / "cmake",
    "#!/usr/bin/env sh\n"
    "if [ \"$1\" = \"-S\" ]; then\n"
    "  build=''\n"
    "  while [ \"$#\" -gt 0 ]; do\n"
    "    if [ \"$1\" = \"-B\" ]; then shift; build=\"$1\"; fi\n"
    "    shift || exit 2\n"
    "  done\n"
    "  mkdir -p \"$build/bin\"\n"
    "  exit 0\n"
    "fi\n"
    "if [ \"$1\" = \"--build\" ]; then\n"
    "  build=\"$2\"\n"
    "  mkdir -p \"$build/bin\"\n"
    "  printf '#!/usr/bin/env sh\\nprintf local-nano\\n' > \"$build/bin/" + binary_name + "\"\n"
    "  chmod +x \"$build/bin/" + binary_name + "\"\n"
    "  exit 0\n"
    "fi\n"
    "exit 3\n");
  MakeExecutable(fake_bin / "cmake");

  const fs::path profile = temp.path() / "profile.toml";
  WriteText(profile, "[profile]\nname = \"local-demo\"\n");

  EnvVarGuard path_guard("PATH");
  const char* original_path = std::getenv("PATH");
  path_guard.set(fake_bin.string() + (original_path == nullptr ? "" : ":" + std::string(original_path)));
  EnvVarGuard jobs_guard("STYIO_NANO_BUILD_JOBS");
  jobs_guard.set("3");
  EXPECT_EQ(styio_nano_build_jobs_latest(), "3");
  jobs_guard.set("0");
  EXPECT_EQ(styio_nano_build_jobs_latest(), "2");
  jobs_guard.set("not-a-number");
  EXPECT_EQ(styio_nano_build_jobs_latest(), "2");
  jobs_guard.set("3");

  StyioNanoCreateSelectionLatest selection;
  selection.mode = "local-subset";
  selection.output_dir = (temp.path() / "local-out").string();
  selection.profile_path = profile.string();
  selection.source_root = source_root.string();
  selection.package_name = "local-demo";
  ASSERT_TRUE(styio_materialize_local_nano_package_latest(selection, "styio-driver", error)) << error;

  const fs::path output = temp.path() / "local-out";
  EXPECT_TRUE(styio_native_build_is_executable_file_latest(output / "bin" / binary_name));
  EXPECT_TRUE(styio_native_build_is_executable_file_latest(output / "build-styio-nano.sh"));
  EXPECT_EQ(ReadText(output / "styio-nano.profile.toml"), "[profile]\nname = \"local-demo\"\n");
  const std::string closure_manifest = ReadText(output / "source-closure-manifest.txt");
  EXPECT_NE(closure_manifest.find("src/StyioTesting/PipelineCheck.cpp"), std::string::npos);
  EXPECT_NE(closure_manifest.find("src/StyioToken/Token.hpp"), std::string::npos);
  const std::string receipt = ReadText(output / "styio-nano-package.toml");
  EXPECT_NE(receipt.find("mode = \"local-subset\""), std::string::npos);
  EXPECT_NE(receipt.find("source_root = \"" + source_root.string() + "\""), std::string::npos);
}

TEST(StyioMainContract, CompilePlanDirectorySetupFailuresEmitDiagnostics) {
  TempDir temp("compile-plan-dir-setup");
  const fs::path workspace = temp.path() / "workspace";
  const fs::path source = workspace / "main.styio";
  WriteText(source, "value: i64 := 1\n");

  auto write_plan = [&](const std::string& name,
                        const fs::path& build_root,
                        const fs::path& artifact_dir,
                        const fs::path& diag_dir) {
    const fs::path plan = temp.path() / (name + ".json");
    const std::string text =
      "{"
      "\"plan_version\":1,"
      "\"generated_by\":{\"tool\":\"pafio\",\"version\":\"1.0\"},"
      "\"intent\":\"check\","
      "\"workspace_root\":\"" + styio_json_escape(workspace.string()) + "\","
      "\"entry\":{\"package_id\":\"pkg/main\",\"target_kind\":\"bin\",\"target_name\":\"app\",\"file\":\""
      + styio_json_escape(source.string()) + "\"},"
      "\"toolchain\":{},"
      "\"profile\":{\"name\":\"dev\"},"
      "\"packages\":[{\"id\":\"pkg/main\"}],"
      "\"resolution\":{},"
      "\"outputs\":{\"build_root\":\"" + styio_json_escape(build_root.string())
      + "\",\"artifact_dir\":\"" + styio_json_escape(artifact_dir.string())
      + "\",\"diag_dir\":\"" + styio_json_escape(diag_dir.string()) + "\"},"
      "\"emit\":{\"error_format\":\"jsonl\",\"ast\":false,\"styio_ir\":false,\"llvm_ir\":false}"
      "}";
    WriteText(plan, text);
    return plan;
  };

  auto expect_setup_failure = [&](const std::string& name,
                                  const fs::path& build_root,
                                  const fs::path& artifact_dir,
                                  const fs::path& diag_dir,
                                  const std::string& needle) {
    const fs::path plan = write_plan(name, build_root, artifact_dir, diag_dir);
    const MainRunResult result = RunMain({"styio", "--compile-plan=" + plan.string()});
    EXPECT_EQ(result.exit_code, static_cast<int>(StyioExitCode::RuntimeError)) << result.stderr_text;
    EXPECT_NE(result.stderr_text.find("\"category\":\"RuntimeError\""), std::string::npos)
      << result.stderr_text;
    EXPECT_NE(result.stderr_text.find(needle), std::string::npos) << result.stderr_text;
    styio_clear_diagnostic_sink_latest();
    styio_clear_runtime_event_sink_latest();
  };

  const fs::path ok_build = temp.path() / "ok-build";
  const fs::path ok_artifacts = temp.path() / "ok-artifacts";
  const fs::path ok_diag = temp.path() / "ok-diag";

  const fs::path diag_blocker = temp.path() / "diag-blocker";
  WriteText(diag_blocker, "not a directory\n");
  expect_setup_failure(
    "diag-dir-blocked",
    ok_build,
    ok_artifacts,
    diag_blocker / "diag",
    "cannot create compile-plan diag_dir");

  const fs::path build_blocker = temp.path() / "build-blocker";
  WriteText(build_blocker, "not a directory\n");
  expect_setup_failure(
    "build-root-blocked",
    build_blocker / "build",
    ok_artifacts,
    ok_diag,
    "cannot create compile-plan build_root");

  const fs::path artifact_blocker = temp.path() / "artifact-blocker";
  WriteText(artifact_blocker, "not a directory\n");
  expect_setup_failure(
    "artifact-dir-blocked",
    ok_build,
    artifact_blocker / "artifacts",
    ok_diag,
    "cannot create compile-plan artifact_dir");

  const fs::path event_build = temp.path() / "event-build";
  std::error_code ec;
  fs::create_directories(event_build / "runtime-events.jsonl", ec);
  ASSERT_FALSE(ec) << ec.message();
  expect_setup_failure(
    "runtime-events-blocked",
    event_build,
    temp.path() / "event-artifacts",
    temp.path() / "event-diag",
    "runtime-events.jsonl");
}

TEST(StyioMainContract, CompilePlanContractParserRejectsMalformedSchemaEdges) {
  TempDir temp("compile-plan-contract");
  const fs::path workspace = temp.path() / "workspace";
  const fs::path source = workspace / "src" / "main.styio";
  const fs::path build_root = temp.path() / "build";
  const fs::path artifact_dir = temp.path() / "artifacts";
  const fs::path diag_dir = temp.path() / "diag";
  WriteText(source, "value: i64 := 1\n");

  auto quote = [](const fs::path& path) {
    return path.string();
  };
  auto valid_plan_text = [&](const std::string& generated_by_tool,
                             const std::string& intent,
                             const std::string& entry_target_kind,
                             const std::string& error_format) {
    return std::string("{")
      + "\"plan_version\":1,"
      + "\"generated_by\":{\"tool\":\"" + generated_by_tool + "\",\"version\":\"1.0\"},"
      + "\"intent\":\"" + intent + "\","
      + "\"workspace_root\":\"" + quote(workspace) + "\","
      + "\"entry\":{\"package_id\":\"pkg/main\",\"target_kind\":\"" + entry_target_kind
      + "\",\"target_name\":\"unit\",\"file\":\"" + quote(source) + "\"},"
      + "\"toolchain\":{},"
      + "\"profile\":{\"name\":\"dev\"},"
      + "\"packages\":[{\"id\":\"pkg/main\"}],"
      + "\"resolution\":{},"
      + "\"outputs\":{\"build_root\":\"" + quote(build_root)
      + "\",\"artifact_dir\":\"" + quote(artifact_dir)
      + "\",\"diag_dir\":\"" + quote(diag_dir) + "\"},"
      + "\"emit\":{\"error_format\":\"" + error_format
      + "\",\"ast\":true,\"styio_ir\":false,\"llvm_ir\":true}"
      + "}";
  };
  auto write_plan = [&](const std::string& name, const std::string& text) {
    const fs::path path = temp.path() / name;
    WriteText(path, text);
    return path;
  };
  auto expect_parse_error = [&](const fs::path& path, const std::string& needle) {
    StyioCompilePlanRequestLatest request;
    std::string error;
    EXPECT_FALSE(styio::config::parse_compile_plan(path, request, error)) << path;
    EXPECT_NE(error.find(needle), std::string::npos) << error;
  };

  {
    fs::path probed;
    EXPECT_FALSE(styio_probe_compile_plan_diag_dir_latest(temp.path() / "missing.json", probed));
    EXPECT_FALSE(styio::config::probe_compile_plan_diag_dir(
      write_plan("probe-invalid.json", "{bad"),
      probed));
    EXPECT_FALSE(styio::config::probe_compile_plan_diag_dir(
      write_plan("probe-array.json", "[]"),
      probed));
    EXPECT_FALSE(styio::config::probe_compile_plan_diag_dir(
      write_plan("probe-no-outputs.json", "{\"outputs\":null}"),
      probed));
    EXPECT_FALSE(styio::config::probe_compile_plan_diag_dir(
      write_plan("probe-relative.json", "{\"outputs\":{\"diag_dir\":\"relative\"}}"),
      probed));
    EXPECT_FALSE(styio::config::probe_compile_plan_diag_dir(
      write_plan("probe-empty-diag.json", "{\"outputs\":{\"diag_dir\":\"\"}}"),
      probed));
    EXPECT_TRUE(styio::config::probe_compile_plan_diag_dir(
      write_plan("probe-ok.json", "{\"outputs\":{\"diag_dir\":\"" + quote(diag_dir) + "\"}}"),
      probed));
    EXPECT_EQ(probed, diag_dir);
  }

  expect_parse_error(temp.path() / "missing.json", "cannot read file");
  expect_parse_error(write_plan("invalid-json.json", "{bad"), "not valid JSON");
  expect_parse_error(write_plan("root-array.json", "[]"), "must be a JSON object");
  expect_parse_error(write_plan("missing-plan-marker.json", "{}"), "missing required integer field: plan_version");
  expect_parse_error(
    write_plan("missing-generated-by.json", "{\"plan_version\":1}"),
    "missing required object field: generated_by");
  expect_parse_error(
    write_plan("missing-packages-array.json",
               "{\"plan_version\":1,\"generated_by\":{\"tool\":\"pafio\",\"version\":\"1\"},"
               "\"intent\":\"test\",\"workspace_root\":\"" + quote(workspace) + "\","
               "\"entry\":{\"package_id\":\"pkg/main\",\"target_kind\":\"test\","
               "\"target_name\":\"unit\",\"file\":\"" + quote(source) + "\"},"
               "\"toolchain\":{},\"profile\":{\"name\":\"dev\"},"
               "\"resolution\":{},\"outputs\":{\"build_root\":\"" + quote(build_root)
               + "\",\"artifact_dir\":\"" + quote(artifact_dir)
               + "\",\"diag_dir\":\"" + quote(diag_dir)
               + "\"},\"emit\":{\"error_format\":\"text\",\"ast\":false,"
               "\"styio_ir\":false,\"llvm_ir\":false}}"),
    "missing required array field: packages");
  {
    StyioCompilePlanRequestLatest request;
    std::string error;
    EXPECT_TRUE(styio::config::parse_compile_plan(
      write_plan("state-plan-marker.json",
                 valid_plan_text("pafio", "test", "test", "jsonl").replace(16, 1, "2")),
      request,
      error)) << error;
    EXPECT_EQ(request.plan_version, 2);
  }
  expect_parse_error(
    write_plan("bad-tool.json", valid_plan_text("other", "test", "test", "jsonl")),
    "generated_by.tool");
  expect_parse_error(
    write_plan("missing-generated-by-version.json",
               "{\"plan_version\":1,\"generated_by\":{\"tool\":\"pafio\"},"
               "\"intent\":\"test\",\"workspace_root\":\"" + quote(workspace) + "\","
               "\"entry\":{\"package_id\":\"pkg/main\",\"target_kind\":\"test\","
               "\"target_name\":\"unit\",\"file\":\"" + quote(source) + "\"},"
               "\"toolchain\":{},\"profile\":{\"name\":\"dev\"},"
               "\"packages\":[{\"id\":\"pkg/main\"}],\"resolution\":{},"
               "\"outputs\":{\"build_root\":\"" + quote(build_root)
               + "\",\"artifact_dir\":\"" + quote(artifact_dir)
               + "\",\"diag_dir\":\"" + quote(diag_dir)
               + "\"},\"emit\":{\"error_format\":\"text\",\"ast\":false,"
               "\"styio_ir\":false,\"llvm_ir\":false}}"),
    "missing required string field: version");
  expect_parse_error(
    write_plan("missing-profile-name.json",
               "{\"plan_version\":1,\"generated_by\":{\"tool\":\"pafio\",\"version\":\"1\"},"
               "\"intent\":\"test\",\"workspace_root\":\"" + quote(workspace) + "\","
               "\"entry\":{\"package_id\":\"pkg/main\",\"target_kind\":\"test\","
               "\"target_name\":\"unit\",\"file\":\"" + quote(source) + "\"},"
               "\"toolchain\":{},\"profile\":{},\"packages\":[{\"id\":\"pkg/main\"}],"
               "\"resolution\":{},\"outputs\":{\"build_root\":\"" + quote(build_root)
               + "\",\"artifact_dir\":\"" + quote(artifact_dir)
               + "\",\"diag_dir\":\"" + quote(diag_dir)
               + "\"},\"emit\":{\"error_format\":\"text\",\"ast\":false,"
               "\"styio_ir\":false,\"llvm_ir\":false}}"),
    "missing required string field: name");
  {
    std::string unsupported_profile =
      valid_plan_text("pafio", "test", "test", "jsonl");
    const std::string profile_marker = "\"profile\":{\"name\":\"dev\"}";
    const size_t marker = unsupported_profile.find(profile_marker);
    ASSERT_NE(marker, std::string::npos);
    unsupported_profile.replace(
      marker,
      profile_marker.size(),
      "\"profile\":{\"name\":\"dev\",\"legacy\":true}");
    expect_parse_error(
      write_plan("unsupported-profile-field.json", unsupported_profile),
      "profile contains an unsupported field: legacy");
  }
  expect_parse_error(
    write_plan("bad-intent.json", valid_plan_text("pafio", "deploy", "test", "jsonl")),
    "unsupported compile-plan intent");
  expect_parse_error(
    write_plan("empty-packages.json",
               "{\"plan_version\":1,\"generated_by\":{\"tool\":\"pafio\",\"version\":\"1\"},"
               "\"intent\":\"test\",\"workspace_root\":\"" + quote(workspace) + "\","
               "\"entry\":{\"package_id\":\"pkg/main\",\"target_kind\":\"test\","
               "\"target_name\":\"unit\",\"file\":\"" + quote(source) + "\"},"
               "\"toolchain\":{},\"profile\":{\"name\":\"dev\"},\"packages\":[],"
               "\"resolution\":{},\"outputs\":{\"build_root\":\"" + quote(build_root)
               + "\",\"artifact_dir\":\"" + quote(artifact_dir)
               + "\",\"diag_dir\":\"" + quote(diag_dir)
               + "\"},\"emit\":{\"error_format\":\"text\",\"ast\":false,"
               "\"styio_ir\":false,\"llvm_ir\":false}}"),
    "packages array must not be empty");
  expect_parse_error(
    write_plan("package-not-object.json",
               "{\"plan_version\":1,\"generated_by\":{\"tool\":\"pafio\",\"version\":\"1\"},"
               "\"intent\":\"test\",\"workspace_root\":\"" + quote(workspace) + "\","
               "\"entry\":{\"package_id\":\"pkg/main\",\"target_kind\":\"test\","
               "\"target_name\":\"unit\",\"file\":\"" + quote(source) + "\"},"
               "\"toolchain\":{},\"profile\":{\"name\":\"dev\"},\"packages\":[null],"
               "\"resolution\":{},\"outputs\":{\"build_root\":\"" + quote(build_root)
               + "\",\"artifact_dir\":\"" + quote(artifact_dir)
               + "\",\"diag_dir\":\"" + quote(diag_dir)
               + "\"},\"emit\":{\"error_format\":\"text\",\"ast\":false,"
               "\"styio_ir\":false,\"llvm_ir\":false}}"),
    "packages[0] must be an object");
  expect_parse_error(
    write_plan("missing-package-id.json",
               "{\"plan_version\":1,\"generated_by\":{\"tool\":\"pafio\",\"version\":\"1\"},"
               "\"intent\":\"test\",\"workspace_root\":\"" + quote(workspace) + "\","
               "\"entry\":{\"package_id\":\"pkg/main\",\"target_kind\":\"test\","
               "\"target_name\":\"unit\",\"file\":\"" + quote(source) + "\"},"
               "\"toolchain\":{},\"profile\":{\"name\":\"dev\"},\"packages\":[{}],"
               "\"resolution\":{},\"outputs\":{\"build_root\":\"" + quote(build_root)
               + "\",\"artifact_dir\":\"" + quote(artifact_dir)
               + "\",\"diag_dir\":\"" + quote(diag_dir)
               + "\"},\"emit\":{\"error_format\":\"text\",\"ast\":false,"
               "\"styio_ir\":false,\"llvm_ir\":false}}"),
    "packages[0].id");
  expect_parse_error(
    write_plan("missing-entry-package.json",
               "{\"plan_version\":1,\"generated_by\":{\"tool\":\"pafio\",\"version\":\"1\"},"
               "\"intent\":\"test\",\"workspace_root\":\"" + quote(workspace) + "\","
               "\"entry\":{\"package_id\":\"pkg/main\",\"target_kind\":\"test\","
               "\"target_name\":\"unit\",\"file\":\"" + quote(source) + "\"},"
               "\"toolchain\":{},\"profile\":{\"name\":\"dev\"},\"packages\":[{\"id\":\"other\"}],"
               "\"resolution\":{},\"outputs\":{\"build_root\":\"" + quote(build_root)
               + "\",\"artifact_dir\":\"" + quote(artifact_dir)
               + "\",\"diag_dir\":\"" + quote(diag_dir)
               + "\"},\"emit\":{\"error_format\":\"text\",\"ast\":false,"
               "\"styio_ir\":false,\"llvm_ir\":false}}"),
    "entry.package_id is not present");
  expect_parse_error(
    write_plan("missing-entry-file.json",
               "{\"plan_version\":1,\"generated_by\":{\"tool\":\"pafio\",\"version\":\"1\"},"
               "\"intent\":\"test\",\"workspace_root\":\"" + quote(workspace) + "\","
               "\"entry\":{\"package_id\":\"pkg/main\",\"target_kind\":\"test\","
               "\"target_name\":\"unit\"},"
               "\"toolchain\":{},\"profile\":{\"name\":\"dev\"},"
               "\"packages\":[{\"id\":\"pkg/main\"}],\"resolution\":{},"
               "\"outputs\":{\"build_root\":\"" + quote(build_root)
               + "\",\"artifact_dir\":\"" + quote(artifact_dir)
               + "\",\"diag_dir\":\"" + quote(diag_dir)
               + "\"},\"emit\":{\"error_format\":\"text\",\"ast\":false,"
               "\"styio_ir\":false,\"llvm_ir\":false}}"),
    "missing required string field: file");
  expect_parse_error(
    write_plan("bad-target-kind.json",
               valid_plan_text("pafio", "test", "bench", "jsonl")),
    "unsupported compile-plan entry.target_kind");
  expect_parse_error(
    write_plan("relative-workspace.json",
               "{\"plan_version\":1,\"generated_by\":{\"tool\":\"pafio\",\"version\":\"1\"},"
               "\"intent\":\"test\",\"workspace_root\":\"relative\","
               "\"entry\":{\"package_id\":\"pkg/main\",\"target_kind\":\"test\","
               "\"target_name\":\"unit\",\"file\":\"" + quote(source) + "\"},"
               "\"toolchain\":{},\"profile\":{\"name\":\"dev\"},"
               "\"packages\":[{\"id\":\"pkg/main\"}],\"resolution\":{},"
               "\"outputs\":{\"build_root\":\"" + quote(build_root)
               + "\",\"artifact_dir\":\"" + quote(artifact_dir)
               + "\",\"diag_dir\":\"" + quote(diag_dir)
               + "\"},\"emit\":{\"error_format\":\"text\",\"ast\":false,"
               "\"styio_ir\":false,\"llvm_ir\":false}}"),
    "path must be absolute: workspace_root");
  expect_parse_error(
    write_plan("missing-bool.json",
               "{\"plan_version\":1,\"generated_by\":{\"tool\":\"pafio\",\"version\":\"1\"},"
               "\"intent\":\"test\",\"workspace_root\":\"" + quote(workspace) + "\","
               "\"entry\":{\"package_id\":\"pkg/main\",\"target_kind\":\"test\","
               "\"target_name\":\"unit\",\"file\":\"" + quote(source) + "\"},"
               "\"toolchain\":{},\"profile\":{\"name\":\"dev\"},"
               "\"packages\":[{\"id\":\"pkg/main\"}],\"resolution\":{},"
               "\"outputs\":{\"build_root\":\"" + quote(build_root)
               + "\",\"artifact_dir\":\"" + quote(artifact_dir)
               + "\",\"diag_dir\":\"" + quote(diag_dir)
               + "\"},\"emit\":{\"error_format\":\"text\",\"styio_ir\":false,"
               "\"llvm_ir\":false}}"),
    "missing required boolean field: ast");
  expect_parse_error(
    write_plan("missing-build-root.json",
               "{\"plan_version\":1,\"generated_by\":{\"tool\":\"pafio\",\"version\":\"1\"},"
               "\"intent\":\"test\",\"workspace_root\":\"" + quote(workspace) + "\","
               "\"entry\":{\"package_id\":\"pkg/main\",\"target_kind\":\"test\","
               "\"target_name\":\"unit\",\"file\":\"" + quote(source) + "\"},"
               "\"toolchain\":{},\"profile\":{\"name\":\"dev\"},"
               "\"packages\":[{\"id\":\"pkg/main\"}],\"resolution\":{},"
               "\"outputs\":{\"artifact_dir\":\"" + quote(artifact_dir)
               + "\",\"diag_dir\":\"" + quote(diag_dir)
               + "\"},\"emit\":{\"error_format\":\"text\",\"ast\":false,"
               "\"styio_ir\":false,\"llvm_ir\":false}}"),
    "missing required string field: build_root");
  expect_parse_error(
    write_plan("bad-error-format.json",
               valid_plan_text("pafio", "test", "test", "yaml")),
    "unsupported compile-plan emit.error_format");

  StyioCompilePlanRequestLatest parsed;
  std::string error;
  ASSERT_TRUE(styio::config::parse_compile_plan(
    write_plan("ok.json", valid_plan_text("pafio", "run", "bin", "text")),
    parsed,
    error)) << error;
  EXPECT_EQ(parsed.intent, "run");
  EXPECT_EQ(parsed.entry_target_kind, "bin");
  EXPECT_EQ(parsed.error_format, "text");
  EXPECT_TRUE(parsed.emit_ast);
  EXPECT_FALSE(parsed.emit_styio_ir);
}

TEST(StyioMainContract, ConfigScalarsCommentsAndProjectConfigAreParsedConservatively) {
  EXPECT_EQ(styio_trim_copy_latest(" \t value \n"), "value");
  EXPECT_EQ(styio_strip_inline_comment_latest("name = \"#kept\" # stripped"), "name = \"#kept\" ");
  EXPECT_EQ(styio_strip_inline_comment_latest("name = '#kept' # stripped"), "name = '#kept' ");
  EXPECT_EQ(styio_strip_inline_comment_latest("name = \"quote \\\" # kept\" # stripped"), "name = \"quote \\\" # kept\" ");

  std::string value;
  std::string error;
  EXPECT_TRUE(styio_parse_config_scalar_latest("\"hello world\"", value, error));
  EXPECT_EQ(value, "hello world");
  EXPECT_TRUE(styio_parse_config_scalar_latest("'literal # hash'", value, error));
  EXPECT_EQ(value, "literal # hash");
  EXPECT_TRUE(styio_parse_config_scalar_latest("bare_value", value, error));
  EXPECT_EQ(value, "bare_value");
  EXPECT_FALSE(styio_parse_config_scalar_latest("", value, error));
  EXPECT_EQ(error, "empty config value");
  EXPECT_FALSE(styio_parse_config_scalar_latest("\"", value, error));
  EXPECT_EQ(error, "malformed quoted config value");
  EXPECT_FALSE(styio_parse_config_scalar_latest("two words", value, error));
  EXPECT_EQ(error, "bare config values cannot contain whitespace");

  TempDir temp("project-config");
  const fs::path config = temp.path() / "styio.toml";
  WriteText(
    config,
    "\n"
    "# ignored comment\n"
    "ignored-without-equals\n"
    "dict_impl = \"ordered-hash\"\n"
    "[dictionary]\n"
    "impl = 'ordered-hash'\n"
    "[ignored]\n"
    "impl = nope\n");

  StyioProjectConfigLatest project_config;
  EXPECT_TRUE(styio_parse_project_config_latest(config, project_config, error)) << error;
  EXPECT_TRUE(project_config.has_dict_impl);
  EXPECT_EQ(project_config.dict_impl_raw, "ordered-hash");
  EXPECT_EQ(project_config.loaded_from, config.string());

  const fs::path source = temp.path() / "nested" / "main.styio";
  WriteText(source, "print(1)\n");
  fs::path found_config;
  EXPECT_TRUE(styio_find_project_config_latest(source.string(), found_config));
  EXPECT_EQ(found_config, config);

  EXPECT_EQ(
    styio_resolve_path_from_config_latest(config, "local/file"),
    styio_absolute_path_latest(temp.path() / "local" / "file").string());
  EXPECT_EQ(styio_resolve_path_from_config_latest(config, "https://example.test/a"), "https://example.test/a");
  EXPECT_EQ(styio_resolve_cli_path_latest("file:///tmp/styio"), "file:///tmp/styio");

  StyioProjectConfigLatest bad_project;
  WriteText(temp.path() / "bad.toml", "[dict]\nimpl = ordered hash\n");
  EXPECT_FALSE(styio_parse_project_config_latest(temp.path() / "bad.toml", bad_project, error));
  EXPECT_NE(error.find("invalid dict_impl value"), std::string::npos);

  WriteText(temp.path() / "bad-section.toml", "[dict\nimpl = \"linear\"\n");
  EXPECT_FALSE(styio_parse_project_config_latest(temp.path() / "bad-section.toml", bad_project, error));
  EXPECT_NE(error.find("malformed section header"), std::string::npos);
}

TEST(StyioMainContract, ConfigEnumFallbacksAndPathHelpersCoverBoundaryBranches) {
  EXPECT_EQ(
    styio_parse_project_config_section_latest("runtime"),
    StyioProjectConfigSectionLatest::RootOrRuntime);
  EXPECT_EQ(
    styio_parse_project_config_section_latest("dict"),
    StyioProjectConfigSectionLatest::Dict);
  EXPECT_EQ(
    styio_parse_project_config_section_latest("unknown"),
    StyioProjectConfigSectionLatest::Other);
  EXPECT_EQ(
    styio_parse_project_config_field_latest(StyioProjectConfigSectionLatest::RootOrRuntime, "dictionary_impl"),
    StyioProjectConfigFieldLatest::DictImpl);
  EXPECT_EQ(
    styio_parse_project_config_field_latest(StyioProjectConfigSectionLatest::Other, "impl"),
    StyioProjectConfigFieldLatest::None);

  EXPECT_EQ(
    styio_parse_nano_package_config_section_latest("nano"),
    StyioNanoPackageConfigSectionLatest::RootOrNano);
  EXPECT_EQ(
    styio_parse_nano_package_config_section_latest("elsewhere"),
    StyioNanoPackageConfigSectionLatest::Other);
  EXPECT_EQ(
    styio_parse_nano_package_config_field_latest(
      StyioNanoPackageConfigSectionLatest::RootOrNano,
      "manifest"),
    StyioNanoPackageConfigFieldLatest::Manifest);
  EXPECT_EQ(
    styio_parse_nano_package_config_field_latest(
      StyioNanoPackageConfigSectionLatest::RootOrNano,
      "ignored"),
    StyioNanoPackageConfigFieldLatest::None);
  EXPECT_EQ(
    styio_parse_nano_package_config_field_latest(
      StyioNanoPackageConfigSectionLatest::Other,
      "mode"),
    StyioNanoPackageConfigFieldLatest::None);

  EXPECT_EQ(
    styio_parse_nano_publish_config_section_latest("nano.publish"),
    StyioNanoPublishConfigSectionLatest::Publish);
  EXPECT_EQ(
    styio_parse_nano_publish_field_latest(StyioNanoPublishConfigSectionLatest::Other, "registry"),
    StyioNanoPublishFieldLatest::None);
  EXPECT_EQ(
    styio_parse_nano_manifest_section_latest("artifact"),
    StyioNanoManifestSectionLatest::Artifact);
  EXPECT_EQ(
    styio_parse_nano_manifest_section_latest("other"),
    StyioNanoManifestSectionLatest::Other);
  EXPECT_EQ(
    styio_parse_nano_manifest_field_latest(StyioNanoManifestSectionLatest::Other, "binary"),
    StyioNanoManifestFieldLatest::None);

  EXPECT_TRUE(styio_ref_is_http_url_latest("http://example.test"));
  EXPECT_TRUE(styio_ref_is_http_url_latest("https://example.test"));
  EXPECT_FALSE(styio_ref_is_http_url_latest("file:///tmp/a"));
  EXPECT_TRUE(styio_ref_is_file_uri_latest("file:///tmp/a"));
  EXPECT_EQ(styio_file_uri_to_path_latest("file://tmp/a"), "/tmp/a");

  TempDir temp("config-boundaries");
  const fs::path config = temp.path() / "cfg" / "styio.toml";
  EXPECT_EQ(styio_resolve_path_from_config_latest(config, ""), "");
  EXPECT_EQ(styio_resolve_path_from_config_latest(config, "file://tmp/a"), "file://tmp/a");
  EXPECT_EQ(
    styio_resolve_cli_path_latest("relative/path"),
    styio_absolute_path_latest("relative/path").string());

  std::string error;
  StyioProjectConfigLatest missing_project;
  EXPECT_FALSE(styio_parse_project_config_latest(temp.path() / "missing.toml", missing_project, error));
  EXPECT_NE(error.find("cannot open config file"), std::string::npos);
}

TEST(StyioMainContract, NanoConfigParsersHonorSectionsAliasesAndFailures) {
  TempDir temp("nano-config");
  std::string error;

  const fs::path package_config = temp.path() / "nano-package.toml";
  WriteText(
    package_config,
    "\n"
    "# ignored package parser trivia\n"
    "not an assignment\n"
    "mode = \"cloud\"\n"
    "output = \"dist\"\n"
    "name = \"root-name\"\n"
    "profile = \"root.profile\"\n"
    "binary = \"bin/root\"\n"
    "source_root = \"src\"\n"
    "[nano.local]\n"
    "profile = \"local.profile\"\n"
    "binary = \"bin/local\"\n"
    "source_root = \"local-src\"\n"
    "[nano.cloud]\n"
    "manifest = \"manifest.toml\"\n"
    "registry = \"file:///tmp/styio-registry\"\n"
    "package = \"org/pkg\"\n"
    "version = \"1.2.3\"\n");

  StyioNanoPackageConfigLatest package;
  ASSERT_TRUE(styio_parse_nano_package_config_latest(package_config, package, error)) << error;
  EXPECT_TRUE(package.has_mode);
  EXPECT_EQ(package.mode_raw, "cloud");
  EXPECT_TRUE(package.has_output_dir);
  EXPECT_EQ(package.output_dir_raw, "dist");
  EXPECT_TRUE(package.has_package_name);
  EXPECT_EQ(package.package_name, "root-name");
  EXPECT_TRUE(package.has_profile);
  EXPECT_EQ(package.profile_raw, "local.profile");
  EXPECT_TRUE(package.has_binary);
  EXPECT_EQ(package.binary_raw, "bin/local");
  EXPECT_TRUE(package.has_source_root);
  EXPECT_EQ(package.source_root_raw, "local-src");
  EXPECT_TRUE(package.has_manifest);
  EXPECT_EQ(package.manifest_raw, "manifest.toml");
  EXPECT_TRUE(package.has_registry);
  EXPECT_EQ(package.registry_raw, "file:///tmp/styio-registry");
  EXPECT_TRUE(package.has_registry_package);
  EXPECT_EQ(package.registry_package_raw, "org/pkg");
  EXPECT_TRUE(package.has_registry_version);
  EXPECT_EQ(package.registry_version_raw, "1.2.3");

  const fs::path publish_config = temp.path() / "nano-publish.toml";
  WriteText(
    publish_config,
    "\n"
    "# ignored publish parser trivia\n"
    "not an assignment\n"
    "[publish]\n"
    "package_root = \"pkgdir\"\n"
    "registry = \"repo\"\n"
    "package = \"org/pkg\"\n"
    "version = \"2.0.0\"\n"
    "channel = \"edge\"\n");
  StyioNanoPublishConfigLatest publish;
  ASSERT_TRUE(styio_parse_nano_publish_config_latest(publish_config, publish, error)) << error;
  EXPECT_TRUE(publish.has_package_dir);
  EXPECT_EQ(publish.package_dir_raw, "pkgdir");
  EXPECT_TRUE(publish.has_registry);
  EXPECT_EQ(publish.registry_raw, "repo");
  EXPECT_TRUE(publish.has_registry_package);
  EXPECT_EQ(publish.registry_package_raw, "org/pkg");
  EXPECT_TRUE(publish.has_registry_version);
  EXPECT_EQ(publish.registry_version_raw, "2.0.0");
  EXPECT_TRUE(publish.has_channel);
  EXPECT_EQ(publish.channel_raw, "edge");

  const fs::path manifest_path = temp.path() / "manifest.toml";
  WriteText(
    manifest_path,
    "[package]\n"
    "name = \"manifest-pkg\"\n"
    "version = \"3.0.0\"\n"
    "channel = \"beta\"\n"
    "[artifact]\n"
    "binary_url = \"bin/styio-nano\"\n"
    "profile_url = \"styio-nano.profile.toml\"\n");
  StyioNanoPackageManifestLatest manifest;
  ASSERT_TRUE(styio_parse_nano_package_manifest_latest(manifest_path, manifest, error)) << error;
  EXPECT_EQ(manifest.package_name, "manifest-pkg");
  EXPECT_EQ(manifest.version, "3.0.0");
  EXPECT_EQ(manifest.channel, "beta");
  EXPECT_EQ(manifest.binary_ref, "bin/styio-nano");
  EXPECT_EQ(manifest.profile_ref, "styio-nano.profile.toml");

  WriteText(temp.path() / "bad-package.toml", "[nano\nmode = cloud\n");
  StyioNanoPackageConfigLatest bad_package;
  EXPECT_FALSE(styio_parse_nano_package_config_latest(temp.path() / "bad-package.toml", bad_package, error));
  EXPECT_NE(error.find("malformed section header"), std::string::npos);

  StyioNanoPackageConfigLatest missing_package_config;
  EXPECT_FALSE(styio_parse_nano_package_config_latest(
    temp.path() / "missing-package.toml",
    missing_package_config,
    error));
  EXPECT_NE(error.find("cannot open nano package config"), std::string::npos);

  WriteText(
    temp.path() / "ignored-package.toml",
    "[nano]\n"
    "this line has no equals\n"
    "ignored = \"value\"\n"
    "[elsewhere]\n"
    "mode = \"ignored\"\n");
  StyioNanoPackageConfigLatest ignored_package;
  ASSERT_TRUE(styio_parse_nano_package_config_latest(
    temp.path() / "ignored-package.toml",
    ignored_package,
    error)) << error;
  EXPECT_FALSE(ignored_package.has_mode);
  EXPECT_EQ(ignored_package.loaded_from, (temp.path() / "ignored-package.toml").string());

  WriteText(temp.path() / "bad-package-value.toml", "[nano]\nmode = two words\n");
  StyioNanoPackageConfigLatest bad_package_value;
  EXPECT_FALSE(styio_parse_nano_package_config_latest(
    temp.path() / "bad-package-value.toml",
    bad_package_value,
    error));
  EXPECT_NE(error.find("invalid nano package value"), std::string::npos);

  WriteText(temp.path() / "bad-publish.toml", "[publish]\nversion = two words\n");
  StyioNanoPublishConfigLatest bad_publish;
  EXPECT_FALSE(styio_parse_nano_publish_config_latest(temp.path() / "bad-publish.toml", bad_publish, error));
  EXPECT_NE(error.find("invalid nano publish value"), std::string::npos);

  WriteText(temp.path() / "bad-publish-section.toml", "[publish\nversion = \"1.0.0\"\n");
  EXPECT_FALSE(styio_parse_nano_publish_config_latest(
    temp.path() / "bad-publish-section.toml",
    bad_publish,
    error));
  EXPECT_NE(error.find("malformed section header"), std::string::npos);

  StyioNanoPublishConfigLatest missing_publish;
  EXPECT_FALSE(styio_parse_nano_publish_config_latest(
    temp.path() / "missing-publish.toml",
    missing_publish,
    error));
  EXPECT_NE(error.find("cannot open nano publish config"), std::string::npos);

  WriteText(
    temp.path() / "ignored-publish.toml",
    "[publish]\n"
    "not an assignment\n"
    "ignored = \"value\"\n"
    "[other]\n"
    "registry = \"ignored\"\n");
  StyioNanoPublishConfigLatest ignored_publish;
  ASSERT_TRUE(styio_parse_nano_publish_config_latest(
    temp.path() / "ignored-publish.toml",
    ignored_publish,
    error)) << error;
  EXPECT_FALSE(ignored_publish.has_registry);
  EXPECT_EQ(ignored_publish.loaded_from, (temp.path() / "ignored-publish.toml").string());

  WriteText(temp.path() / "missing-binary.toml", "name = fallback-name\n");
  StyioNanoPackageManifestLatest missing_binary;
  EXPECT_FALSE(styio_parse_nano_package_manifest_latest(temp.path() / "missing-binary.toml", missing_binary, error));
  EXPECT_EQ(error, "nano package manifest is missing artifact.binary");

  StyioNanoPackageManifestLatest missing_manifest;
  EXPECT_FALSE(styio_parse_nano_package_manifest_latest(
    temp.path() / "missing-manifest.toml",
    missing_manifest,
    error));
  EXPECT_NE(error.find("cannot open nano package manifest"), std::string::npos);

  WriteText(temp.path() / "bad-manifest-value.toml", "[artifact]\nbinary = two words\n");
  StyioNanoPackageManifestLatest bad_manifest_value;
  EXPECT_FALSE(styio_parse_nano_package_manifest_latest(
    temp.path() / "bad-manifest-value.toml",
    bad_manifest_value,
    error));
  EXPECT_NE(error.find("invalid nano package manifest value"), std::string::npos);

  WriteText(
    temp.path() / "defaulted-manifest.toml",
    "not an assignment\n"
    "ignored = \"value\"\n"
    "[other]\n"
    "binary = \"ignored\"\n"
    "[artifact]\n"
    "binary = \"bin/defaulted\"\n");
  StyioNanoPackageManifestLatest defaulted_manifest;
  defaulted_manifest.channel.clear();
  ASSERT_TRUE(styio_parse_nano_package_manifest_latest(
    temp.path() / "defaulted-manifest.toml",
    defaulted_manifest,
    error)) << error;
  EXPECT_EQ(defaulted_manifest.package_name, "defaulted-manifest");
  EXPECT_EQ(defaulted_manifest.channel, "nano");
  EXPECT_EQ(defaulted_manifest.binary_ref, "bin/defaulted");
}

TEST(StyioMainContract, NanoProfileNameUsesProfileFieldAndStemFallback) {
  TempDir temp("nano-profile");
  std::string error;
  std::string profile_name;

  const fs::path named = temp.path() / "custom.profile.toml";
  WriteText(
    named,
    "\n"
    "# leading trivia is ignored\n"
    "[ignored]\n"
    "name = nope\n"
    "[profile]\n"
    "name = \"tiny-local\"\n");
  EXPECT_TRUE(styio_parse_nano_profile_name_latest(named, profile_name, error)) << error;
  EXPECT_EQ(profile_name, "tiny-local");

  const fs::path fallback = temp.path() / "fallback.profile.toml";
  WriteText(fallback, "[profile]\nignored-without-equals\nthreads = 1\n");
  EXPECT_TRUE(styio_parse_nano_profile_name_latest(fallback, profile_name, error)) << error;
  EXPECT_EQ(profile_name, "fallback.profile");

  const fs::path malformed = temp.path() / "malformed.profile.toml";
  WriteText(malformed, "[profile\nname = bad\n");
  EXPECT_FALSE(styio_parse_nano_profile_name_latest(malformed, profile_name, error));
  EXPECT_NE(error.find("malformed section header"), std::string::npos);

  const fs::path bad_value = temp.path() / "bad-value.profile.toml";
  WriteText(bad_value, "[profile]\nname = two words\n");
  EXPECT_FALSE(styio_parse_nano_profile_name_latest(bad_value, profile_name, error));
  EXPECT_NE(error.find("invalid profile.name"), std::string::npos);

  EXPECT_FALSE(styio_parse_nano_profile_name_latest(temp.path() / "missing.profile.toml", profile_name, error));
  EXPECT_NE(error.find("cannot open styio-nano profile"), std::string::npos);
}

TEST(StyioMainContract, ArtifactRefsCopyLocalPathsAndRejectUnsafeInputs) {
  TempDir temp("artifact-refs");
  std::string error;

  const fs::path source = temp.path() / "artifact.txt";
  WriteText(source, "artifact payload\n");
  const fs::path relative_dest = temp.path() / "out" / "relative.txt";
  ASSERT_TRUE(styio_fetch_ref_to_file_latest("artifact.txt", temp.path(), relative_dest, false, error)) << error;
  EXPECT_EQ(ReadText(relative_dest), "artifact payload\n");

  EXPECT_FALSE(styio_fetch_ref_to_file_latest("", temp.path(), temp.path() / "empty.txt", false, error));
  EXPECT_EQ(error, "empty artifact reference");
  EXPECT_FALSE(styio_fetch_ref_to_file_latest("relative.txt", {}, temp.path() / "no-base.txt", false, error));
  EXPECT_NE(error.find("relative artifact reference requires"), std::string::npos);
  EXPECT_FALSE(styio_fetch_ref_to_file_latest("missing.txt", temp.path(), temp.path() / "missing.txt", false, error));
  EXPECT_NE(error.find("artifact not found"), std::string::npos);
  EXPECT_FALSE(styio_fetch_ref_to_file_latest(
    "http://127.0.0.1:9/missing-artifact",
    {},
    temp.path() / "out" / "http-missing.txt",
    false,
    error));
  EXPECT_NE(error.find("failed to download artifact via curl"), std::string::npos);

  const fs::path file_uri_dest = temp.path() / "out" / "file-uri.txt";
  const std::string file_uri = "file://" + source.string();
  ASSERT_TRUE(styio_fetch_ref_to_file_latest(file_uri, {}, file_uri_dest, true, error)) << error;
  EXPECT_EQ(ReadText(file_uri_dest), "artifact payload\n");
  EXPECT_TRUE(styio_native_build_is_executable_file_latest(file_uri_dest));
  EXPECT_EQ(styio_file_uri_to_path_latest(file_uri), source.string());
  EXPECT_EQ(styio_file_uri_to_path_latest("plain/path"), "plain/path");
  EXPECT_EQ(styio_shell_quote_latest("a'b"), "'a'\\''b'");
}

TEST(StyioMainContract, NanoArchiveBoundariesRejectTraversalAndSymlinks) {
  TempDir temp("nano-archive-boundaries");
  std::string error;

  EXPECT_TRUE(styio_validate_tar_listing_text_latest(
    ".\n./bin/" + styio_nano_binary_filename_latest() + "\n./styio-nano-package.toml\n",
    error)) << error;
  EXPECT_FALSE(styio_validate_tar_listing_text_latest("../escape.txt\n", error));
  EXPECT_NE(error.find("unsafe nano package archive entry"), std::string::npos);
  EXPECT_FALSE(styio_validate_tar_listing_text_latest("/tmp/escape.txt\n", error));
  std::string drive_qualified_entry = "C:";
  drive_qualified_entry += "/escape.txt\n";
  EXPECT_FALSE(styio_validate_tar_listing_text_latest(drive_qualified_entry, error));
  EXPECT_FALSE(styio_validate_tar_listing_text_latest("dir\\escape.txt\n", error));

  const fs::path package_root = temp.path() / "package";
  WriteText(package_root / "bin" / styio_nano_binary_filename_latest(), "#!/usr/bin/env sh\nexit 0\n");
  ASSERT_TRUE(styio_validate_nano_package_tree_latest(package_root, error)) << error;

  const fs::path outside = temp.path() / "outside.txt";
  WriteText(outside, "outside\n");
  std::error_code symlink_ec;
  fs::create_symlink(outside, package_root / "linked-outside", symlink_ec);
  if (!symlink_ec) {
    EXPECT_FALSE(styio_validate_nano_package_tree_latest(package_root, error));
    EXPECT_NE(error.find("symbolic link"), std::string::npos);
  }
}

TEST(StyioMainContract, NativeCompilerFallbackAndHttpFetchHelpersStayExplicit) {
  TempDir temp("native-http-helper-branches");
  std::string error;

  const fs::path fake_bin = temp.path() / "bin";
  const fs::path fake_clang = fake_bin / "clang++";
  WriteText(fake_clang, "#!/usr/bin/env sh\nexit 0\n");
  MakeExecutable(fake_clang);
  const fs::path fake_curl = fake_bin / "curl";
  WriteText(
    fake_curl,
    "#!/usr/bin/env sh\n"
    "out=''\n"
    "while [ \"$#\" -gt 0 ]; do\n"
    "  if [ \"$1\" = '-o' ]; then\n"
    "    shift\n"
    "    out=\"$1\"\n"
    "  fi\n"
    "  shift || break\n"
    "done\n"
    "/bin/mkdir -p \"$(/usr/bin/dirname \"$out\")\"\n"
    "printf 'downloaded\\n' > \"$out\"\n");
  MakeExecutable(fake_curl);

  EnvVarGuard path_guard("PATH");
  EnvVarGuard native_cxx_guard("STYIO_NATIVE_CXX");
  EnvVarGuard toolchain_root_guard("STYIO_NATIVE_TOOLCHAIN_ROOT");
  const char* old_path = std::getenv("PATH");
  path_guard.set(fake_bin.string() + (old_path != nullptr ? ":" + std::string(old_path) : ""));
  native_cxx_guard.unset();
  toolchain_root_guard.set((temp.path() / "missing-toolchain").string());

  std::string resolved_tool;
  ASSERT_TRUE(styio_resolve_process_tool_latest("curl", resolved_tool));
  EXPECT_EQ(resolved_tool, styio_absolute_path_latest(fake_curl).string());

  EXPECT_EQ(
    styio_native_build_compiler_from_config_latest({}, "g++"),
    fake_clang.string());

  const fs::path downloaded = temp.path() / "download" / "tool.bin";
  ASSERT_TRUE(styio_fetch_ref_to_file_latest(
    "https://example.test/artifact.bin",
    {},
    downloaded,
    true,
    error)) << error;
  EXPECT_EQ(ReadText(downloaded), "downloaded\n");
  EXPECT_TRUE(styio_native_build_is_executable_file_latest(downloaded));
}

TEST(StyioMainContract, NanoRepositoryJsonContractsValidateAndRoundTrip) {
  std::string error;
  EXPECT_TRUE(styio_validate_nano_repository_marker_latest(
    "{\n\"kind\":\"styio-nano-static\",\"schema\":\"styio-nano-static-repository\"\n}\n",
    error));
  EXPECT_FALSE(styio_validate_nano_repository_marker_latest("{", error));
  EXPECT_NE(error.find("not valid JSON"), std::string::npos);
  EXPECT_FALSE(styio_validate_nano_repository_marker_latest("[]", error));
  EXPECT_EQ(error, "nano repository marker must be a JSON object");
  EXPECT_FALSE(styio_validate_nano_repository_marker_latest(
    "{\"kind\":\"other\",\"schema\":\"styio-nano-static-repository\"}",
    error));
  EXPECT_NE(error.find("does not match"), std::string::npos);

  const std::string valid_entry =
    "{"
    "\"schema\":\"styio-nano-repository-entry\","
    "\"package\":\"org/pkg\","
    "\"version\":\"1.0.0\","
    "\"channel\":\"edge\","
    "\"sha256\":\"" + GoodSha('b') + "\","
    "\"size_bytes\":42,"
    "\"blob_path\":\"blobs/sha256/bb/pkg.tar\","
    "\"published_at\":\"2026-01-01T00:00:00Z\""
    "}";
  StyioNanoRepositoryEntryLatest entry;
  ASSERT_TRUE(styio_parse_nano_repository_entry_latest(valid_entry, "org/pkg", "1.0.0", entry, error)) << error;
  EXPECT_EQ(entry.package_name, "org/pkg");
  EXPECT_EQ(entry.version, "1.0.0");
  EXPECT_EQ(entry.channel, "edge");
  EXPECT_EQ(entry.sha256, GoodSha('b'));
  EXPECT_EQ(entry.size_bytes, 42U);
  EXPECT_EQ(entry.blob_path, "blobs/sha256/bb/pkg.tar");
  EXPECT_EQ(entry.published_at, "2026-01-01T00:00:00Z");

  const std::string minimal_entry =
    "{\"schema\":\"styio-nano-repository-entry\",\"package\":\"org/pkg\",\"version\":\"1.0.0\","
    "\"sha256\":\"" + GoodSha('c') + "\",\"size_bytes\":0,\"blob_path\":\"blob.tar\"}";
  ASSERT_TRUE(styio_parse_nano_repository_entry_latest(minimal_entry, "org/pkg", "1.0.0", entry, error)) << error;
  EXPECT_EQ(entry.channel, "nano");
  EXPECT_TRUE(entry.published_at.empty());

  EXPECT_FALSE(styio_parse_nano_repository_entry_latest("{", "org/pkg", "1.0.0", entry, error));
  EXPECT_NE(error.find("not valid JSON"), std::string::npos);
  EXPECT_FALSE(styio_parse_nano_repository_entry_latest("[]", "org/pkg", "1.0.0", entry, error));
  EXPECT_EQ(error, "nano repository entry must be a JSON object");
  EXPECT_FALSE(styio_parse_nano_repository_entry_latest(
    "{\"schema\":\"other-entry\",\"package\":\"org/pkg\",\"version\":\"1.0.0\"}",
    "org/pkg",
    "1.0.0",
    entry,
    error));
  EXPECT_EQ(error, "nano repository entry does not match the supported schema");
  EXPECT_FALSE(styio_parse_nano_repository_entry_latest(
    "{\"schema\":\"styio-nano-repository-entry\",\"package\":\"other\",\"version\":\"1.0.0\"}",
    "org/pkg",
    "1.0.0",
    entry,
    error));
  EXPECT_EQ(error, "nano repository entry package mismatch");
  EXPECT_FALSE(styio_parse_nano_repository_entry_latest(
    "{\"schema\":\"styio-nano-repository-entry\",\"package\":\"org/pkg\",\"version\":\"2.0.0\"}",
    "org/pkg",
    "1.0.0",
    entry,
    error));
  EXPECT_EQ(error, "nano repository entry version mismatch");
  EXPECT_FALSE(styio_parse_nano_repository_entry_latest(
    "{\"schema\":\"styio-nano-repository-entry\",\"package\":\"org/pkg\",\"version\":\"1.0.0\","
    "\"sha256\":\"ABC\",\"size_bytes\":1,\"blob_path\":\"blob.tar\"}",
    "org/pkg",
    "1.0.0",
    entry,
    error));
  EXPECT_EQ(error, "nano repository entry is missing a valid sha256 digest");
  EXPECT_FALSE(styio_parse_nano_repository_entry_latest(
    "{\"schema\":\"styio-nano-repository-entry\",\"package\":\"org/pkg\",\"version\":\"1.0.0\","
    "\"sha256\":\"" + GoodSha('d') + "\",\"size_bytes\":1}",
    "org/pkg",
    "1.0.0",
    entry,
    error));
  EXPECT_EQ(error, "nano repository entry is missing blob_path");
  EXPECT_FALSE(styio_parse_nano_repository_entry_latest(
    "{\"schema\":\"styio-nano-repository-entry\",\"package\":\"org/pkg\",\"version\":\"1.0.0\","
    "\"sha256\":\"" + GoodSha('e') + "\",\"size_bytes\":-1,\"blob_path\":\"blob.tar\"}",
    "org/pkg",
    "1.0.0",
    entry,
    error));
  EXPECT_EQ(error, "nano repository entry is missing a valid size_bytes");
}

TEST(StyioMainContract, NanoRepositoryFilesAndRegistryRefsRoundTripLocally) {
  TempDir temp("repo-files");
  const fs::path repo = temp.path() / "repo";
  std::string error;

  ASSERT_TRUE(styio_ensure_writable_nano_repository_latest(repo, error)) << error;
  std::string marker_text;
  ASSERT_TRUE(styio_fetch_registry_text_latest(
    repo.string(),
    styio_nano_repository_marker_relpath_latest(),
    marker_text,
    error)) << error;
  EXPECT_TRUE(styio_validate_nano_repository_marker_latest(marker_text, error)) << error;
  EXPECT_TRUE(styio_ensure_writable_nano_repository_latest(repo, error)) << error;

  StyioNanoRepositoryEntryLatest entry;
  entry.package_name = "org/pkg";
  entry.version = "1.2.3";
  entry.channel = "nightly";
  entry.sha256 = GoodSha('f');
  entry.size_bytes = 1234;
  entry.blob_path = styio_nano_repository_blob_relpath_latest(entry.sha256);
  entry.published_at = "2026-06-05T00:00:00Z";
  ASSERT_TRUE(styio_write_nano_repository_entry_latest(repo, entry, error)) << error;

  std::string entry_text;
  ASSERT_TRUE(styio_fetch_registry_text_latest(
    "file://" + repo.string(),
    styio_nano_repository_entry_relpath_latest(entry.package_name),
    entry_text,
    error)) << error;
  StyioNanoRepositoryEntryLatest parsed;
  ASSERT_TRUE(styio_parse_nano_repository_entry_latest(entry_text, "org/pkg", "1.2.3", parsed, error)) << error;
  EXPECT_EQ(parsed.channel, "nightly");
  EXPECT_EQ(parsed.blob_path, entry.blob_path);
  EXPECT_FALSE(styio_fetch_registry_text_latest(
    "http://127.0.0.1:9",
    styio_nano_repository_marker_relpath_latest(),
    entry_text,
    error));
  EXPECT_NE(error.find("failed to download artifact via curl"), std::string::npos);

  const fs::path invalid_repo = temp.path() / "invalid-repo";
  WriteText(
    invalid_repo / styio_nano_repository_marker_relpath_latest(),
    "{\"kind\":\"bad\",\"schema\":\"styio-nano-static-repository\"}");
  EXPECT_FALSE(styio_ensure_writable_nano_repository_latest(invalid_repo, error));
  EXPECT_NE(error.find("does not match"), std::string::npos);

  const fs::path unreadable_marker_repo = temp.path() / "unreadable-marker-repo";
  const fs::path unreadable_marker =
    unreadable_marker_repo / styio_nano_repository_marker_relpath_latest();
  WriteText(unreadable_marker, "{\"kind\":\"styio-nano-static\"}\n");
  std::error_code perm_ec;
  fs::permissions(
    unreadable_marker,
    fs::perms::owner_read | fs::perms::group_read | fs::perms::others_read,
    fs::perm_options::remove,
    perm_ec);
  ASSERT_FALSE(perm_ec) << perm_ec.message();
  EXPECT_FALSE(styio_ensure_writable_nano_repository_latest(unreadable_marker_repo, error));
  EXPECT_NE(error.find("cannot open file"), std::string::npos);
  fs::permissions(unreadable_marker, fs::perms::owner_read, fs::perm_options::add, perm_ec);

  EXPECT_EQ(styio_nano_repository_package_leaf_latest("org/pkg"), "pkg");
  EXPECT_EQ(styio_nano_repository_package_leaf_latest("pkg"), "pkg");
  EXPECT_EQ(styio_nano_repository_package_leaf_latest("org/"), "org/");
  EXPECT_EQ(
    styio_nano_repository_blob_relpath_latest(GoodSha('a')),
    "blobs/sha256/aa/aa/" + GoodSha('a') + ".tar");
}

TEST(StyioMainContract, CloudManifestMaterializationCopiesBinaryProfileAndReceipt) {
  TempDir temp("cloud-manifest");
  const std::string binary_name = styio_nano_binary_filename_latest();
  std::string error;

  const fs::path source = temp.path() / "source";
  WriteText(source / "bin" / binary_name, "#!/usr/bin/env sh\nexit 0\n");
  MakeExecutable(source / "bin" / binary_name);
  WriteText(source / "profile.toml", "[profile]\nname = \"cloud-profile\"\n");
  WriteText(
    source / "styio-nano-package.toml",
    "[package]\n"
    "name = \"manifest-name\"\n"
    "version = \"9.8.7\"\n"
    "channel = \"beta\"\n"
    "[artifact]\n"
    "binary = \"bin/" + binary_name + "\"\n"
    "profile = \"profile.toml\"\n");

  StyioNanoCreateSelectionLatest selection;
  selection.mode = "cloud";
  selection.output_dir = (temp.path() / "out").string();
  selection.package_name = "override-name";
  selection.manifest_ref = (source / "styio-nano-package.toml").string();
  ASSERT_TRUE(styio_materialize_cloud_nano_package_latest(selection, error)) << error;

  EXPECT_EQ(ReadText(temp.path() / "out" / "bin" / binary_name), "#!/usr/bin/env sh\nexit 0\n");
  EXPECT_TRUE(styio_native_build_is_executable_file_latest(temp.path() / "out" / "bin" / binary_name));
  EXPECT_EQ(ReadText(temp.path() / "out" / "styio-nano.profile.toml"), "[profile]\nname = \"cloud-profile\"\n");
  const std::string receipt = ReadText(temp.path() / "out" / "styio-nano-package.toml");
  EXPECT_NE(receipt.find("name = \"override-name\""), std::string::npos);
  EXPECT_NE(receipt.find("channel = \"beta\""), std::string::npos);
  EXPECT_NE(receipt.find("version = \"9.8.7\""), std::string::npos);
  EXPECT_NE(receipt.find("artifact_binary = \"bin/" + binary_name + "\""), std::string::npos);
}

TEST(StyioMainContract, CloudMaterializationRejectsInvalidRegistryAndManifestInputs) {
  TempDir temp("cloud-invalid");
  const std::string binary_name = styio_nano_binary_filename_latest();
  std::string error;

  const fs::path blocked_output = temp.path() / "blocked-output";
  WriteText(blocked_output, "not a directory\n");
  StyioNanoCreateSelectionLatest selection;
  selection.mode = "cloud";
  selection.output_dir = blocked_output.string();
  selection.manifest_ref = (temp.path() / "manifest.toml").string();
  EXPECT_FALSE(styio_materialize_cloud_nano_package_latest(selection, error));
  EXPECT_NE(error.find("failed to create output directory"), std::string::npos);

  selection = StyioNanoCreateSelectionLatest{};
  selection.mode = "cloud";
  selection.output_dir = (temp.path() / "missing-manifest-out").string();
  selection.manifest_ref = (temp.path() / "missing-manifest.toml").string();
  EXPECT_FALSE(styio_materialize_cloud_nano_package_latest(selection, error));
  EXPECT_NE(error.find("artifact not found"), std::string::npos);

  const fs::path source = temp.path() / "manifest-source";
  WriteText(
    source / "missing-binary.toml",
    "[package]\n"
    "name = \"missing-binary\"\n"
    "[artifact]\n"
    "binary = \"bin/missing-nano\"\n");
  selection = StyioNanoCreateSelectionLatest{};
  selection.mode = "cloud";
  selection.output_dir = (temp.path() / "missing-binary-out").string();
  selection.manifest_ref = (source / "missing-binary.toml").string();
  EXPECT_FALSE(styio_materialize_cloud_nano_package_latest(selection, error));
  EXPECT_NE(error.find("artifact not found"), std::string::npos);

  WriteText(source / "bin" / binary_name, "#!/usr/bin/env sh\nexit 0\n");
  MakeExecutable(source / "bin" / binary_name);
  WriteText(
    source / "missing-profile.toml",
    "[package]\n"
    "name = \"missing-profile\"\n"
    "[artifact]\n"
    "binary = \"bin/" + binary_name + "\"\n"
    "profile = \"profiles/missing.toml\"\n");
  selection = StyioNanoCreateSelectionLatest{};
  selection.mode = "cloud";
  selection.output_dir = (temp.path() / "missing-profile-out").string();
  selection.manifest_ref = (source / "missing-profile.toml").string();
  EXPECT_FALSE(styio_materialize_cloud_nano_package_latest(selection, error));
  EXPECT_NE(error.find("artifact not found"), std::string::npos);

  WriteText(
    source / "receipt-block.toml",
    "[package]\n"
    "name = \"receipt-block\"\n"
    "[artifact]\n"
    "binary = \"bin/" + binary_name + "\"\n");
  const fs::path manifest_receipt_block_out = temp.path() / "manifest-receipt-block-out";
  fs::create_directories(manifest_receipt_block_out / "styio-nano-package.toml");
  selection = StyioNanoCreateSelectionLatest{};
  selection.mode = "cloud";
  selection.output_dir = manifest_receipt_block_out.string();
  selection.manifest_ref = (source / "receipt-block.toml").string();
  EXPECT_FALSE(styio_materialize_cloud_nano_package_latest(selection, error));
  EXPECT_NE(error.find("cannot open file for writing"), std::string::npos) << error;

  const fs::path repo_missing_marker = temp.path() / "repo-missing-marker";
  selection = StyioNanoCreateSelectionLatest{};
  selection.mode = "cloud";
  selection.output_dir = (temp.path() / "repo-missing-marker-out").string();
  selection.registry_root = repo_missing_marker.string();
  selection.registry_package = "org/pkg";
  selection.registry_version = "1.0.0";
  EXPECT_FALSE(styio_materialize_cloud_nano_package_latest(selection, error));
  EXPECT_NE(error.find("cannot open file"), std::string::npos);

  const fs::path repo_bad_marker = temp.path() / "repo-bad-marker";
  WriteText(
    repo_bad_marker / styio_nano_repository_marker_relpath_latest(),
    "{\"kind\":\"bad\",\"schema\":\"styio-nano-static-repository\"}");
  selection.output_dir = (temp.path() / "repo-bad-marker-out").string();
  selection.registry_root = repo_bad_marker.string();
  EXPECT_FALSE(styio_materialize_cloud_nano_package_latest(selection, error));
  EXPECT_NE(error.find("does not match"), std::string::npos);

  const fs::path repo_missing_entry = temp.path() / "repo-missing-entry";
  ASSERT_TRUE(styio_ensure_writable_nano_repository_latest(repo_missing_entry, error)) << error;
  selection.output_dir = (temp.path() / "repo-missing-entry-out").string();
  selection.registry_root = repo_missing_entry.string();
  EXPECT_FALSE(styio_materialize_cloud_nano_package_latest(selection, error));
  EXPECT_NE(error.find("cannot open file"), std::string::npos);

  const fs::path repo_size_mismatch = temp.path() / "repo-size-mismatch";
  ASSERT_TRUE(styio_ensure_writable_nano_repository_latest(repo_size_mismatch, error)) << error;
  const std::string sha = GoodSha('a');
  const std::string blob_relpath = styio_nano_repository_blob_relpath_latest(sha);
  WriteText(repo_size_mismatch / fs::path(blob_relpath), "abc");
  StyioNanoRepositoryEntryLatest entry;
  entry.package_name = "org/pkg";
  entry.version = "1.0.0";
  entry.channel = "nano";
  entry.sha256 = sha;
  entry.blob_path = blob_relpath;
  entry.size_bytes = 999;
  ASSERT_TRUE(styio_write_nano_repository_entry_latest(repo_size_mismatch, entry, error)) << error;
  selection.output_dir = (temp.path() / "repo-size-mismatch-out").string();
  selection.registry_root = repo_size_mismatch.string();
  EXPECT_FALSE(styio_materialize_cloud_nano_package_latest(selection, error));
  EXPECT_NE(error.find("blob size mismatch"), std::string::npos);

  const fs::path repo_sha_mismatch = temp.path() / "repo-sha-mismatch";
  ASSERT_TRUE(styio_ensure_writable_nano_repository_latest(repo_sha_mismatch, error)) << error;
  const std::string wrong_sha = GoodSha('b');
  const std::string wrong_blob_relpath = styio_nano_repository_blob_relpath_latest(wrong_sha);
  WriteText(repo_sha_mismatch / fs::path(wrong_blob_relpath), "abc");
  entry.sha256 = wrong_sha;
  entry.blob_path = wrong_blob_relpath;
  entry.size_bytes = 3;
  ASSERT_TRUE(styio_write_nano_repository_entry_latest(repo_sha_mismatch, entry, error)) << error;
  selection.output_dir = (temp.path() / "repo-sha-mismatch-out").string();
  selection.registry_root = repo_sha_mismatch.string();
  EXPECT_FALSE(styio_materialize_cloud_nano_package_latest(selection, error));
  EXPECT_NE(error.find("blob sha256 mismatch"), std::string::npos);

  const fs::path repo_missing_blob = temp.path() / "repo-missing-blob";
  ASSERT_TRUE(styio_ensure_writable_nano_repository_latest(repo_missing_blob, error)) << error;
  entry = StyioNanoRepositoryEntryLatest{};
  entry.package_name = "org/missing-blob";
  entry.version = "1.0.0";
  entry.channel = "nano";
  entry.sha256 = GoodSha('c');
  entry.blob_path = styio_nano_repository_blob_relpath_latest(entry.sha256);
  entry.size_bytes = 1;
  ASSERT_TRUE(styio_write_nano_repository_entry_latest(repo_missing_blob, entry, error)) << error;
  selection.output_dir = (temp.path() / "repo-missing-blob-out").string();
  selection.registry_root = repo_missing_blob.string();
  selection.registry_package = entry.package_name;
  selection.registry_version = entry.version;
  EXPECT_FALSE(styio_materialize_cloud_nano_package_latest(selection, error));
  EXPECT_NE(error.find("artifact not found"), std::string::npos) << error;

  const fs::path repo_bad_tar = temp.path() / "repo-bad-tar";
  ASSERT_TRUE(styio_ensure_writable_nano_repository_latest(repo_bad_tar, error)) << error;
  const fs::path bad_tar_blob = temp.path() / "bad-blob.tar";
  WriteText(bad_tar_blob, "not a tar archive\n");
  WriteNanoRepositoryBlobEntry(repo_bad_tar, bad_tar_blob, "org/bad-tar", "1.0.0", entry, error);
  selection.output_dir = (temp.path() / "repo-bad-tar-out").string();
  selection.registry_root = repo_bad_tar.string();
  selection.registry_package = entry.package_name;
  selection.registry_version = entry.version;
  EXPECT_FALSE(styio_materialize_cloud_nano_package_latest(selection, error));
  EXPECT_NE(error.find("blob extraction"), std::string::npos) << error;

  const fs::path no_bin_root = temp.path() / "no-bin-root";
  WriteText(no_bin_root / "README.txt", "no binary here\n");
  const fs::path no_bin_blob = temp.path() / "no-bin.tar";
  RunProcessOrFail(
    {"tar", "-cf", no_bin_blob.string(), "-C", no_bin_root.string(), "."},
    "test no-bin nano archive");
  const fs::path repo_no_bin = temp.path() / "repo-no-bin";
  ASSERT_TRUE(styio_ensure_writable_nano_repository_latest(repo_no_bin, error)) << error;
  WriteNanoRepositoryBlobEntry(repo_no_bin, no_bin_blob, "org/no-bin", "1.0.0", entry, error);
  selection.output_dir = (temp.path() / "repo-no-bin-out").string();
  selection.registry_root = repo_no_bin.string();
  selection.registry_package = entry.package_name;
  selection.registry_version = entry.version;
  EXPECT_FALSE(styio_materialize_cloud_nano_package_latest(selection, error));
  EXPECT_NE(error.find("does not contain bin/" + binary_name), std::string::npos) << error;

  const fs::path copy_fail_root = temp.path() / "copy-fail-root";
  WriteText(copy_fail_root / "bin" / binary_name, "#!/usr/bin/env sh\nexit 0\n");
  WriteText(copy_fail_root / "conflict" / "payload.txt", "conflict\n");
  const fs::path copy_fail_blob = temp.path() / "copy-fail.tar";
  RunProcessOrFail(
    {"tar", "-cf", copy_fail_blob.string(), "-C", copy_fail_root.string(), "."},
    "test copy-fail nano archive");
  const fs::path repo_copy_fail = temp.path() / "repo-copy-fail";
  ASSERT_TRUE(styio_ensure_writable_nano_repository_latest(repo_copy_fail, error)) << error;
  WriteNanoRepositoryBlobEntry(repo_copy_fail, copy_fail_blob, "org/copy-fail", "1.0.0", entry, error);
  const fs::path copy_fail_output = temp.path() / "repo-copy-fail-out";
  WriteText(copy_fail_output / "conflict", "blocks directory copy\n");
  selection.output_dir = copy_fail_output.string();
  selection.registry_root = repo_copy_fail.string();
  selection.registry_package = entry.package_name;
  selection.registry_version = entry.version;
  EXPECT_FALSE(styio_materialize_cloud_nano_package_latest(selection, error));
  EXPECT_NE(error.find("failed to copy package content"), std::string::npos) << error;
}

TEST(StyioMainContract, CloudRepositoryMaterializationVerifiesBlobAndExtractsPackage) {
  TempDir temp("cloud-repo-install");
  const std::string binary_name = styio_nano_binary_filename_latest();
  std::string error;

  const fs::path package_root = temp.path() / "package-root";
  WriteText(package_root / "bin" / binary_name, "#!/usr/bin/env sh\nexit 0\n");
  MakeExecutable(package_root / "bin" / binary_name);
  WriteText(package_root / "styio-nano.profile.toml", "[profile]\nname = \"repo-profile\"\n");
  WriteText(package_root / "README.txt", "payload\n");

  const fs::path blob = temp.path() / "package.tar";
  RunProcessOrFail(
    {"tar", "-cf", blob.string(), "-C", package_root.string(), "."},
    "test nano package archive");

  std::string sha256;
  ASSERT_TRUE(styio_compute_file_sha256_latest(blob, sha256, error)) << error;
  std::error_code ec;
  const uint64_t size_bytes = fs::file_size(blob, ec);
  ASSERT_FALSE(ec) << ec.message();

  const fs::path repo = temp.path() / "repo";
  ASSERT_TRUE(styio_ensure_writable_nano_repository_latest(repo, error)) << error;
  const std::string blob_relpath = styio_nano_repository_blob_relpath_latest(sha256);
  ASSERT_TRUE(styio_copy_file_with_exec_latest(blob, repo / fs::path(blob_relpath), false, error)) << error;

  StyioNanoRepositoryEntryLatest entry;
  entry.package_name = "org/repo-pkg";
  entry.version = "1.0.1";
  entry.channel = "nightly";
  entry.sha256 = sha256;
  entry.blob_path = blob_relpath;
  entry.size_bytes = size_bytes;
  entry.published_at = "2026-06-05T00:00:00Z";
  ASSERT_TRUE(styio_write_nano_repository_entry_latest(repo, entry, error)) << error;

  StyioNanoCreateSelectionLatest selection;
  selection.mode = "cloud";
  selection.output_dir = (temp.path() / "installed").string();
  selection.registry_root = repo.string();
  selection.registry_package = entry.package_name;
  selection.registry_version = entry.version;
  ASSERT_TRUE(styio_materialize_cloud_nano_package_latest(selection, error)) << error;

  EXPECT_EQ(ReadText(temp.path() / "installed" / "bin" / binary_name), "#!/usr/bin/env sh\nexit 0\n");
  EXPECT_EQ(ReadText(temp.path() / "installed" / "README.txt"), "payload\n");
  EXPECT_TRUE(styio_native_build_is_executable_file_latest(temp.path() / "installed" / "bin" / binary_name));
  const std::string receipt = ReadText(temp.path() / "installed" / "styio-nano-package.toml");
  EXPECT_NE(receipt.find("name = \"repo-pkg\""), std::string::npos);
  EXPECT_NE(receipt.find("registry = \"" + repo.string() + "\""), std::string::npos);
  EXPECT_NE(receipt.find("sha256 = \"" + sha256 + "\""), std::string::npos);
}

TEST(StyioMainContract, PublishNanoPackageWritesStaticRepositoryEntryAndBlob) {
  TempDir temp("publish");
  const std::string binary_name = styio_nano_binary_filename_latest();
  std::string error;

  const fs::path package_dir = temp.path() / "package";
  WriteText(package_dir / "bin" / binary_name, "#!/usr/bin/env sh\nexit 0\n");
  MakeExecutable(package_dir / "bin" / binary_name);
  WriteText(package_dir / "styio-nano-package.toml", "[package]\nname = \"receipt-name\"\nversion = \"0.0.1\"\n");
  WriteText(package_dir / "payload.txt", "publish payload\n");

  const fs::path repo_blocker = temp.path() / "repo-blocker";
  WriteText(repo_blocker, "not a directory\n");
  StyioNanoPublishSelectionLatest selection;
  selection.package_dir = package_dir.string();
  selection.registry_root = repo_blocker.string();
  selection.registry_package = "org/blocked";
  selection.registry_version = "1.0.0";
  EXPECT_FALSE(styio_publish_nano_package_latest(selection, error));
  EXPECT_NE(error.find("failed to create nano repository root"), std::string::npos);

  selection = StyioNanoPublishSelectionLatest{};
  selection.package_dir = (temp.path() / "missing-package").string();
  selection.registry_root = (temp.path() / "missing-package-repo").string();
  selection.registry_package = "org/missing";
  selection.registry_version = "1.0.0";
  EXPECT_FALSE(styio_publish_nano_package_latest(selection, error));
  EXPECT_NE(error.find("nano package directory is not a directory"), std::string::npos);

  {
    const fs::path fake_tar_bin = temp.path() / "fake-tar-bin";
    const fs::path fake_tar = fake_tar_bin / "tar";
    WriteText(fake_tar, "#!/bin/sh\nexit 0\n");
    MakeExecutable(fake_tar);
    EnvVarGuard path_guard("PATH");
    const char* original_path_raw = std::getenv("PATH");
    path_guard.set(
      fake_tar_bin.string()
      + (original_path_raw == nullptr ? "" : ":" + std::string(original_path_raw)));

    selection = StyioNanoPublishSelectionLatest{};
    selection.package_dir = package_dir.string();
    selection.registry_root = (temp.path() / "repo-missing-tar-output").string();
    selection.registry_package = "org/missing-tar-output";
    selection.registry_version = "1.0.0";
    error.clear();
    EXPECT_FALSE(styio_publish_nano_package_latest(selection, error));
    EXPECT_NE(error.find("failed to compute sha256"), std::string::npos) << error;
  }

  const fs::path repo_blob_blocker = temp.path() / "repo-blob-blocker";
  ASSERT_TRUE(styio_ensure_writable_nano_repository_latest(repo_blob_blocker, error)) << error;
  WriteText(repo_blob_blocker / "blobs", "blocks blob directory\n");
  selection = StyioNanoPublishSelectionLatest{};
  selection.package_dir = package_dir.string();
  selection.registry_root = repo_blob_blocker.string();
  selection.registry_package = "org/blob-blocked";
  selection.registry_version = "1.0.0";
  EXPECT_FALSE(styio_publish_nano_package_latest(selection, error));
  EXPECT_NE(error.find("failed to copy"), std::string::npos) << error;

  selection = StyioNanoPublishSelectionLatest{};
  selection.package_dir = package_dir.string();
  selection.registry_root = (temp.path() / "repo").string();
  selection.registry_package = "org/published";
  selection.registry_version = "2.3.4";
  selection.channel = "stable";
  ASSERT_TRUE(styio_publish_nano_package_latest(selection, error)) << error;

  std::string entry_text;
  ASSERT_TRUE(styio_fetch_registry_text_latest(
    selection.registry_root,
    styio_nano_repository_entry_relpath_latest(selection.registry_package),
    entry_text,
    error)) << error;
  StyioNanoRepositoryEntryLatest entry;
  ASSERT_TRUE(styio_parse_nano_repository_entry_latest(
    entry_text,
    selection.registry_package,
    selection.registry_version,
    entry,
    error)) << error;
  EXPECT_EQ(entry.channel, "stable");
  EXPECT_TRUE(styio_is_hex_digest_64_latest(entry.sha256));
  EXPECT_GT(entry.size_bytes, 0U);
  EXPECT_TRUE(fs::is_regular_file(fs::path(selection.registry_root) / fs::path(entry.blob_path)));

  std::string marker_text;
  EXPECT_TRUE(styio_fetch_registry_text_latest(
    selection.registry_root,
    styio_nano_repository_marker_relpath_latest(),
    marker_text,
    error)) << error;
  EXPECT_TRUE(styio_validate_nano_repository_marker_latest(marker_text, error)) << error;
}

TEST(StyioMainContract, RegistryPathsPackageRootsAndSiblingNanoBinaryResolveDeterministically) {
  TempDir temp("paths");
  std::string error;

  EXPECT_EQ(styio_normalize_nano_registry_root_latest("https://example.test/repo///"), "https://example.test/repo");
  EXPECT_EQ(styio_normalize_nano_registry_root_latest("file:///tmp/styio-registry///"), "file:///tmp/styio-registry");
  EXPECT_EQ(
    styio_normalize_nano_registry_root_latest((temp.path() / "local///").string()),
    styio_absolute_path_latest(temp.path() / "local").string());
  EXPECT_EQ(styio_normalize_nano_registry_root_latest("///"), "");
  EXPECT_EQ(styio_join_registry_ref_latest("https://example.test/repo", "index/pkg.json"), "https://example.test/repo/index/pkg.json");
  EXPECT_EQ(styio_join_registry_ref_latest("file:///tmp/repo", "index/pkg.json"), "file:///tmp/repo/index/pkg.json");
  EXPECT_EQ(
    styio_join_registry_ref_latest((temp.path() / "repo").string(), "index/pkg.json"),
    (temp.path() / "repo" / "index" / "pkg.json").string());

  const std::string binary_name = styio_nano_binary_filename_latest();
  const fs::path direct_root = temp.path() / "direct";
  WriteText(direct_root / "bin" / binary_name, "nano");
  fs::path package_root;
  EXPECT_TRUE(styio_resolve_extracted_nano_package_root_latest(direct_root, package_root, error)) << error;
  EXPECT_EQ(package_root, direct_root);

  const fs::path nested_extract = temp.path() / "nested";
  WriteText(nested_extract / "pkg" / "bin" / binary_name, "nano");
  EXPECT_TRUE(styio_resolve_extracted_nano_package_root_latest(nested_extract, package_root, error)) << error;
  EXPECT_EQ(package_root, nested_extract / "pkg");

  const fs::path invalid_extract = temp.path() / "invalid";
  std::error_code ec;
  fs::create_directories(invalid_extract / "a", ec);
  ASSERT_FALSE(ec) << ec.message();
  fs::create_directories(invalid_extract / "b", ec);
  ASSERT_FALSE(ec) << ec.message();
  EXPECT_FALSE(styio_resolve_extracted_nano_package_root_latest(invalid_extract, package_root, error));
  EXPECT_NE(error.find("does not contain bin/"), std::string::npos);

  EXPECT_TRUE(styio_guess_sibling_nano_binary_latest(nullptr).empty());
  EXPECT_TRUE(styio_guess_sibling_nano_binary_latest("").empty());
  EXPECT_EQ(
    styio_guess_sibling_nano_binary_latest((temp.path() / "bin" / "styio").string().c_str()),
    styio_absolute_path_latest(temp.path() / "bin" / "styio").parent_path() / "styio-nano");
  EXPECT_EQ(
    styio_guess_sibling_nano_binary_latest((temp.path() / "bin" / "styio.exe").string().c_str()).filename(),
    fs::path("styio-nano.exe"));
}

TEST(StyioMainContract, NativeBuildDiscoveryHelpersResolveExecutablesAndRoots) {
  TempDir temp("native-discovery");
  std::string error;
  std::string command;

  EXPECT_FALSE(styio_native_build_find_executable_latest("", command));
  EXPECT_FALSE(styio_native_build_is_executable_file_latest(temp.path() / "missing"));
  EXPECT_FALSE(styio_native_build_find_clang_in_root_latest({}, command));
  EXPECT_FALSE(styio_path_has_runtime_source_latest({}));

  const fs::path direct_tool = temp.path() / "tools" / "direct-cxx";
  WriteText(direct_tool, "#!/usr/bin/env sh\nexit 0\n");
  MakeExecutable(direct_tool);
  ASSERT_TRUE(styio_native_build_find_executable_latest(direct_tool.string(), command));
  EXPECT_EQ(command, direct_tool.string());

  const fs::path path_dir = temp.path() / "path-bin";
  const fs::path path_tool = path_dir / "path-cxx";
  WriteText(path_tool, "#!/usr/bin/env sh\nexit 0\n");
  MakeExecutable(path_tool);
  EnvVarGuard path_guard("PATH");
  path_guard.set((temp.path() / "empty-path-dir").string() + ":" + path_dir.string());
  ASSERT_TRUE(styio_native_build_find_executable_latest("path-cxx", command));
  EXPECT_EQ(command, path_tool.string());
  EXPECT_FALSE(styio_native_build_find_executable_latest("missing-cxx", command));
  path_guard.set(":" + path_dir.string());
  ASSERT_TRUE(styio_native_build_find_executable_latest("path-cxx", command));
  EXPECT_EQ(command, path_tool.string());
  path_guard.set("");
  EXPECT_FALSE(styio_native_build_find_executable_latest("path-cxx", command));

  const fs::path toolchain_root = temp.path() / "toolchain";
  const fs::path bundled_clang = toolchain_root / "bin" / "clang++";
  WriteText(bundled_clang, "#!/usr/bin/env sh\nexit 0\n");
  MakeExecutable(bundled_clang);
  ASSERT_TRUE(styio_native_build_find_clang_in_root_latest(toolchain_root, command));
  EXPECT_EQ(command, bundled_clang.string());
  EXPECT_FALSE(styio_native_build_find_clang_in_root_latest(temp.path() / "no-clang-root", command));

  EnvVarGuard cxx_guard("STYIO_NATIVE_CXX");
  cxx_guard.set("custom-clang++");
  EXPECT_EQ(styio_native_build_compiler_latest({}), "custom-clang++");
  cxx_guard.unset();

  EnvVarGuard toolchain_guard("STYIO_NATIVE_TOOLCHAIN_ROOT");
  toolchain_guard.set(toolchain_root.string());
  EXPECT_EQ(
    styio_native_build_compiler_from_config_latest({}, "g++"),
    bundled_clang.string());
  toolchain_guard.unset();

  const fs::path sibling_toolchain =
    temp.path() / "install" / STYIO_NATIVE_TOOLCHAIN_RELATIVE_DIR;
  const fs::path sibling_clang = sibling_toolchain / "bin" / "clang++-18";
  WriteText(sibling_clang, "#!/usr/bin/env sh\nexit 0\n");
  MakeExecutable(sibling_clang);
  EXPECT_EQ(
    styio_native_build_compiler_from_config_latest(
      temp.path() / "install" / "bin" / "styio",
      "g++"),
    sibling_clang.string());

  const fs::path runtime_root = temp.path() / "runtime-root";
  WriteText(runtime_root / "src" / "StyioExtern" / "ExternLib.cpp", "// runtime marker\n");
  WriteText(runtime_root / "bin" / "styio", "#!/usr/bin/env sh\nexit 0\n");
  MakeExecutable(runtime_root / "bin" / "styio");
  EXPECT_TRUE(styio_path_has_runtime_source_latest(runtime_root));
  EXPECT_EQ(
    styio_resolve_source_root_from_config_latest(
      runtime_root / "bin" / "styio",
      temp.path() / "not-a-source-root"),
    runtime_root);
  EXPECT_TRUE(styio_resolve_source_root_from_config_latest(
    temp.path() / "orphan" / "bin" / "styio",
    temp.path() / "not-a-source-root").empty());
  const fs::path resolved = styio_resolve_source_root_latest(runtime_root / "bin" / "styio");
  EXPECT_FALSE(resolved.empty());
  EXPECT_TRUE(styio_path_has_runtime_source_latest(resolved));

  const fs::path temp_root = styio_create_native_build_temp_root_latest(error);
  ASSERT_FALSE(temp_root.empty()) << error;
  EXPECT_TRUE(fs::is_directory(temp_root));
  std::error_code ec;
  fs::remove_all(temp_root, ec);
  EXPECT_FALSE(ec) << ec.message();
}

TEST(StyioMainContract, NanoSourceClosureHelpersCoverSuccessAndFailureEdges) {
  TempDir temp("nano-closure");
  std::string error;

  const std::vector<std::string> roots_without_pipeline = styio_nano_source_roots_latest(false);
  const std::vector<std::string> roots_with_pipeline = styio_nano_source_roots_latest(true);
  EXPECT_EQ(
    std::find(roots_without_pipeline.begin(), roots_without_pipeline.end(), "src/StyioTesting/PipelineCheck.cpp"),
    roots_without_pipeline.end());
  EXPECT_NE(
    std::find(roots_with_pipeline.begin(), roots_with_pipeline.end(), "src/StyioTesting/PipelineCheck.cpp"),
    roots_with_pipeline.end());
  EXPECT_NE(
    std::find(roots_without_pipeline.begin(), roots_without_pipeline.end(), "share/styio/prelude/resources.styio"),
    roots_without_pipeline.end());

  const fs::path source_root = temp.path() / "source";
  WriteText(source_root / "src" / "main.cpp", "#include \"local_extra.hpp\"\n");
  for (const std::string& relpath : roots_with_pipeline) {
    const fs::path path = source_root / relpath;
    if (!fs::exists(path)) {
      WriteText(path, "// " + relpath + "\n");
    }
  }
  WriteText(source_root / "src" / "local_extra.hpp", "// local include\n");
  WriteText(source_root / "src" / "a" / "current.cpp", "#include \"local.hpp\"\n");
  WriteText(source_root / "src" / "a" / "local.hpp", "// local\n");
  WriteText(source_root / "src" / "StyioToken" / "Token.hpp", "// token\n");
  WriteText(source_root / "include" / "root.hpp", "// root\n");
  const fs::path sibling_source_root = temp.path() / "source_sibling";
  WriteText(sibling_source_root / "escape.hpp", "// sibling include must not be captured\n");

  std::string include_relpath;
  EXPECT_TRUE(styio_resolve_local_include_relpath_latest(
    source_root,
    "src/a/current.cpp",
    "local.hpp",
    include_relpath));
  EXPECT_EQ(include_relpath, "src/a/local.hpp");
  EXPECT_TRUE(styio_resolve_local_include_relpath_latest(
    source_root,
    "src/a/current.cpp",
    "StyioToken/Token.hpp",
    include_relpath));
  EXPECT_EQ(include_relpath, "src/StyioToken/Token.hpp");
  EXPECT_TRUE(styio_resolve_local_include_relpath_latest(
    source_root,
    "src/a/current.cpp",
    "include/root.hpp",
    include_relpath));
  EXPECT_EQ(include_relpath, "include/root.hpp");
  EXPECT_FALSE(styio_resolve_local_include_relpath_latest(
    source_root,
    "src/a/current.cpp",
    "missing.hpp",
    include_relpath));
  EXPECT_FALSE(styio_resolve_local_include_relpath_latest(
    source_root,
    "src/a/current.cpp",
    "../../../source_sibling/escape.hpp",
    include_relpath));

  std::set<std::string> closure_files;
  ASSERT_TRUE(styio_collect_nano_closure_files_latest(source_root, true, closure_files, error)) << error;
  EXPECT_NE(closure_files.find("src/main.cpp"), closure_files.end());
  EXPECT_NE(closure_files.find("src/local_extra.hpp"), closure_files.end());
  EXPECT_NE(closure_files.find("src/StyioTesting/PipelineCheck.cpp"), closure_files.end());
  EXPECT_NE(closure_files.find("share/styio/prelude/resources.styio"), closure_files.end());

  std::set<std::string> missing_closure;
  EXPECT_FALSE(styio_collect_nano_closure_files_latest(temp.path() / "missing-source", false, missing_closure, error));
  EXPECT_NE(error.find("nano source closure is missing required file"), std::string::npos);

  const fs::path unreadable_source_root = temp.path() / "unreadable-source";
  for (const std::string& relpath : roots_without_pipeline) {
    WriteText(unreadable_source_root / relpath, "// " + relpath + "\n");
  }
  const fs::path unreadable_source = unreadable_source_root / "src" / "main.cpp";
  std::error_code perm_ec;
  fs::permissions(
    unreadable_source,
    fs::perms::owner_read | fs::perms::group_read | fs::perms::others_read,
    fs::perm_options::remove,
    perm_ec);
  ASSERT_FALSE(perm_ec) << perm_ec.message();
  std::set<std::string> unreadable_closure;
  EXPECT_FALSE(styio_collect_nano_closure_files_latest(
    unreadable_source_root,
    false,
    unreadable_closure,
    error));
  EXPECT_NE(error.find("cannot read nano closure source"), std::string::npos);
  fs::permissions(unreadable_source, fs::perms::owner_read, fs::perm_options::add, perm_ec);

  const fs::path output_dir = temp.path() / "out";
  ASSERT_TRUE(styio_copy_closure_files_latest(source_root, output_dir, closure_files, error)) << error;
  EXPECT_TRUE(fs::exists(output_dir / "src" / "main.cpp"));
  ASSERT_TRUE(styio_write_nano_closure_manifest_latest(output_dir, closure_files, error)) << error;
  EXPECT_NE(ReadText(output_dir / "source-closure-manifest.txt").find("src/main.cpp"), std::string::npos);

  const std::vector<std::string> cpp_sources = styio_nano_cpp_sources_from_closure_latest({
    "src/z.cpp",
    "src/main.cpp",
    "src/a.hpp",
  });
  ASSERT_EQ(cpp_sources.size(), 2u);
  EXPECT_EQ(cpp_sources[0], "src/main.cpp");
  EXPECT_EQ(cpp_sources[1], "src/z.cpp");

  std::set<std::string> no_cpp_sources{"include/only.hpp"};
  EXPECT_FALSE(styio_write_nano_package_cmakelists_latest(temp.path() / "no-cpp", no_cpp_sources, error));
  EXPECT_EQ(error, "nano source closure does not contain any C++ sources");

  ASSERT_TRUE(styio_write_nano_package_cmakelists_latest(output_dir, closure_files, error)) << error;
  const std::string cmake_text = ReadText(output_dir / "CMakeLists.txt");
  EXPECT_NE(cmake_text.find("add_executable(styio_nano"), std::string::npos);
  EXPECT_NE(cmake_text.find("src/main.cpp"), std::string::npos);

  WriteText(temp.path() / "profile-on.cmake", "set(STYIO_NANO_INCLUDE_PIPELINE_CHECK ON)\n");
  bool include_pipeline_check = false;
  ASSERT_TRUE(styio_profile_cmake_includes_pipeline_check_latest(
    temp.path() / "profile-on.cmake",
    include_pipeline_check,
    error)) << error;
  EXPECT_TRUE(include_pipeline_check);
  WriteText(temp.path() / "profile-off.cmake", "# off\n");
  ASSERT_TRUE(styio_profile_cmake_includes_pipeline_check_latest(
    temp.path() / "profile-off.cmake",
    include_pipeline_check,
    error)) << error;
  EXPECT_FALSE(include_pipeline_check);

  EnvVarGuard jobs_guard("STYIO_NANO_BUILD_JOBS");
  jobs_guard.set("4");
  EXPECT_EQ(styio_nano_build_jobs_latest(), "4");
  jobs_guard.set("0");
  EXPECT_EQ(styio_nano_build_jobs_latest(), "2");
  jobs_guard.set("abc");
  EXPECT_EQ(styio_nano_build_jobs_latest(), "2");
}

TEST(StyioMainContract, FileProcessAndRuntimeSinksCoverFailureAndAppendPaths) {
  TempDir temp("io-sinks");
  std::string error;

  EXPECT_FALSE(styio_write_text_file_latest(temp.path() / "missing" / "write.txt", "x", error));
  EXPECT_NE(error.find("cannot open file for writing"), std::string::npos);
  EXPECT_FALSE(styio_run_process_latest({"/bin/false"}, "expected failure", error));
  EXPECT_NE(error.find("expected failure failed"), std::string::npos);
  EXPECT_FALSE(styio_copy_file_with_exec_latest(
    temp.path() / "missing-source.txt",
    temp.path() / "out" / "copy.txt",
    false,
    error));
  EXPECT_NE(error.find("failed to copy"), std::string::npos);

  std::string first_token;
  EXPECT_TRUE(styio_read_first_process_token_latest({"printf", "alpha beta\n"}, first_token));
  EXPECT_EQ(first_token, "alpha");
  EXPECT_FALSE(styio_read_first_process_token_latest({"/bin/false"}, first_token));

  std::string sha256;
  EXPECT_FALSE(styio_compute_file_sha256_latest(temp.path() / "missing.bin", sha256, error));
  EXPECT_NE(error.find("failed to compute sha256"), std::string::npos);
  {
    const fs::path fake_bin = temp.path() / "fake-sha-bin";
    WriteText(fake_bin / "shasum", "#!/bin/sh\nexit 7\n");
    WriteText(
      fake_bin / "sha256sum",
      "#!/bin/sh\n"
      "printf '%s  %s\\n' '" + GoodSha('b') + "' \"$1\"\n");
    MakeExecutable(fake_bin / "shasum");
    MakeExecutable(fake_bin / "sha256sum");
    WriteText(temp.path() / "payload.bin", "payload\n");

    EnvVarGuard path_guard("PATH");
    path_guard.set(fake_bin.string());
    ASSERT_TRUE(styio_compute_file_sha256_latest(temp.path() / "payload.bin", sha256, error)) << error;
    EXPECT_EQ(sha256, GoodSha('b'));
  }
  EXPECT_TRUE(styio_is_hex_digest_64_latest(GoodSha('a')));
  EXPECT_FALSE(styio_is_hex_digest_64_latest(std::string(63, 'a')));
  EXPECT_FALSE(styio_is_hex_digest_64_latest(std::string(64, 'A')));
  EXPECT_FALSE(styio_is_hex_digest_64_latest(std::string(64, 'g')));

  const fs::path append_path = temp.path() / "logs" / "append.txt";
  ASSERT_TRUE(styio_append_text_file_latest(append_path, "one\n", error)) << error;
  ASSERT_TRUE(styio_append_text_file_latest(append_path, "two\n", error)) << error;
  EXPECT_EQ(ReadText(append_path), "one\ntwo\n");
  ASSERT_TRUE(styio_ensure_text_file_exists_latest(temp.path() / "logs" / "empty.txt", error)) << error;
  EXPECT_TRUE(fs::exists(temp.path() / "logs" / "empty.txt"));
  const fs::path blocked_parent = temp.path() / "blocked-parent";
  WriteText(blocked_parent, "not a directory\n");
  EXPECT_FALSE(styio_append_text_file_latest(blocked_parent / "append.txt", "x", error));
  EXPECT_NE(error.find("cannot create file parent directory"), std::string::npos);
  EXPECT_FALSE(styio_ensure_text_file_exists_latest(blocked_parent / "ensure.txt", error));
  EXPECT_NE(error.find("cannot create file parent directory"), std::string::npos);
  if (fs::exists("/dev/full")) {
    EXPECT_FALSE(styio_append_text_file_latest("/dev/full", "x", error));
    EXPECT_NE(error.find("failed to append file"), std::string::npos);
  }

  const std::string rendered = styio_render_diagnostic_jsonl_latest(
    StyioErrorCategory::LexError,
    "bad.styio",
    "unterminated string",
    "");
  EXPECT_NE(rendered.find("\"category\":\"LexError\""), std::string::npos);
  EXPECT_NE(rendered.find("\"phase\":\"lex\""), std::string::npos);

  styio_set_diagnostic_sink_latest(temp.path() / "diag");
  testing::internal::CaptureStderr();
  styio_emit_diagnostic("text", StyioErrorCategory::CliError, "cli", "bad option", "service_invalid_option");
  const std::string text_diag = testing::internal::GetCapturedStderr();
  EXPECT_NE(text_diag.find("[CliError] bad option"), std::string::npos);
  EXPECT_NE(ReadText(temp.path() / "diag" / "diagnostics.jsonl").find("service_invalid_option"), std::string::npos);
  styio_clear_diagnostic_sink_latest();

  styio_set_runtime_event_sink_latest(temp.path() / "events");
  styio_emit_runtime_event_latest("custom.event", "unit", "{\"ok\":true}");
  styio_runtime_log_sink_latest("stdout", "hello");
  styio_runtime_log_sink_latest(nullptr, "ignored");
  styio_runtime_log_sink_latest("stderr", nullptr);
  const std::string intent = "test";
  styio_emit_runtime_phase_transition_latest(
    CompilationPhase::Parsed,
    CompilationPhase::Parsed,
    "noop",
    "file.styio",
    &intent);
  styio_emit_runtime_phase_transition_latest(
    CompilationPhase::Parsed,
    CompilationPhase::Typed,
    "typecheck",
    "file.styio",
    &intent);
  const std::string events = ReadText(temp.path() / "events" / "runtime-events.jsonl");
  EXPECT_NE(events.find("\"eventKind\":\"custom.event\""), std::string::npos);
  EXPECT_NE(events.find("\"eventKind\":\"log.emitted\""), std::string::npos);
  EXPECT_NE(events.find("\"eventKind\":\"transition.fired\""), std::string::npos);
  EXPECT_NE(events.find("\"eventKind\":\"state.changed\""), std::string::npos);
  EXPECT_NE(events.find("\"intent\":\"test\""), std::string::npos);
  EXPECT_STREQ(styio_runtime_phase_name_latest(CompilationPhase::Empty), "empty");
  EXPECT_STREQ(styio_runtime_phase_name_latest(CompilationPhase::Failed), "failed");
  EXPECT_STREQ(styio_runtime_phase_name_latest(static_cast<CompilationPhase>(999)), "unknown");
  styio_clear_runtime_event_sink_latest();
}

TEST(StyioMainContract, FrontendProfilerSerializesPhasesCountersAndWriteFailures) {
  using styio::profiler::FrontendProfiler;

  FrontendProfiler disabled;
  EXPECT_FALSE(disabled.enabled());
  EXPECT_FALSE(disabled.written());
  EXPECT_TRUE(disabled.output_path().empty());
  disabled.record_phase("ignored", 10);
  disabled.add_counter("ignored", 1);
  disabled.mark_status("ignored");
  disabled.set_source_summary(12, 2);
  disabled.set_parser_route_stats(1, 2, 3, 4);
  disabled.set_async_scheduler_stats(1, 2, 1, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
  EXPECT_TRUE(disabled.write());
  EXPECT_FALSE(disabled.written());
  const std::string disabled_json = disabled.to_json();
  EXPECT_NE(disabled_json.find("\"total_duration_ns\": 0"), std::string::npos);
  EXPECT_NE(disabled_json.find("\"parser_route\": null"), std::string::npos);
  EXPECT_NE(disabled_json.find("\"async_scheduler\": null"), std::string::npos);

  EXPECT_EQ(FrontendProfiler::default_output_path_for_source(""), "styio-profile.json");
  EXPECT_EQ(
    FrontendProfiler::default_output_path_for_source("main.styio"),
    "main.styio.profile.json");

  TempDir temp("frontend-profiler");
  const fs::path output = temp.path() / "profiles" / "frontend.json";

  FrontendProfiler profiler;
  profiler.enable("weird\\file\"name\nreturn\r.styio", "nightly\tengine", output.string());
  EXPECT_TRUE(profiler.enabled());
  EXPECT_FALSE(profiler.written());
  EXPECT_EQ(profiler.output_path(), output.string());
  profiler.add_counter("manual_counter", 7);
  profiler.set_source_summary(128, 5);
  profiler.record_phase("manual\nphase", 42);
  profiler.mark_status("ok", std::string("detail ") + static_cast<char>(1));
  profiler.set_parser_route_stats(3, 2, 1, 0);
  profiler.set_async_scheduler_stats(1, 4, 1, 3, 2, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 11);

  StyioToken* name = StyioToken::Create(StyioTokenType::NAME, "name");
  StyioToken* integer = StyioToken::Create(StyioTokenType::INTEGER, "1");
  profiler.set_token_histogram({name, nullptr, integer});
  delete name;
  delete integer;

  {
    auto self = profiler.phase("self");
    self = std::move(self);
  }
  {
    auto source = profiler.phase("move-ctor");
    auto moved = std::move(source);
  }
  {
    auto first = profiler.phase("first");
    auto second = profiler.phase("second");
    second = std::move(first);
  }
  {
    auto unnamed = profiler.phase(nullptr);
  }

  const std::string json = profiler.to_json();
  EXPECT_NE(json.find("weird\\\\file\\\"name\\nreturn\\r.styio"), std::string::npos);
  EXPECT_NE(json.find("nightly\\tengine"), std::string::npos);
  EXPECT_NE(json.find("\\u0001"), std::string::npos);
  EXPECT_NE(json.find("\"manual_counter\": 7"), std::string::npos);
  EXPECT_NE(json.find("\"source_bytes\": 128"), std::string::npos);
  EXPECT_NE(json.find("\"token_count\": 3"), std::string::npos);
  EXPECT_NE(
    json.find("\"" + StyioToken::getTokName(StyioTokenType::NAME) + "\": 1"),
    std::string::npos);
  EXPECT_NE(
    json.find("\"" + StyioToken::getTokName(StyioTokenType::INTEGER) + "\": 1"),
    std::string::npos);
  EXPECT_NE(json.find("\"nightly_subset_statements\": 3"), std::string::npos);
  EXPECT_NE(json.find("\"ready_queue_kind\": 1"), std::string::npos);
  EXPECT_NE(json.find("\"max_queue_depth\": 11"), std::string::npos);
  EXPECT_NE(json.find("\"name\": \"second\""), std::string::npos);

  std::string error;
  ASSERT_TRUE(profiler.write(&error)) << error;
  EXPECT_TRUE(profiler.written());
  EXPECT_NE(ReadText(output).find("\"tool\": \"styio-profiler\""), std::string::npos);

  FrontendProfiler empty_output;
  empty_output.enable("file.styio", "nightly", "");
  EXPECT_FALSE(empty_output.write(nullptr));
  EXPECT_FALSE(empty_output.write(&error));
  EXPECT_EQ(error, "profile output path is empty");

  FrontendProfiler directory_output;
  directory_output.enable("file.styio", "nightly", temp.path().string());
  EXPECT_FALSE(directory_output.write(&error));
  EXPECT_NE(error.find("cannot open profile output path"), std::string::npos);

  const fs::path parent_blocker = temp.path() / "profile-parent-blocker";
  WriteText(parent_blocker, "not a directory\n");
  FrontendProfiler blocked_parent;
  blocked_parent.enable("file.styio", "nightly", (parent_blocker / "profile.json").string());
  EXPECT_FALSE(blocked_parent.write(&error));
  EXPECT_NE(error.find("cannot create profile output directory"), std::string::npos);

#ifndef _WIN32
  if (fs::exists("/dev/full")) {
    FrontendProfiler full_output;
    full_output.enable("file.styio", "nightly", "/dev/full");
    full_output.mark_status("failed", std::string(1 << 20, 'x'));
    EXPECT_FALSE(full_output.write(&error));
    EXPECT_NE(error.find("cannot write profile output path"), std::string::npos);
  }
#endif
}

TEST(StyioMainContract, NativeBuildArgsAndExecutableDiscoveryStayFailClosed) {
  std::string error;
  StyioNativeBuildArgsLatest args;

  EXPECT_TRUE(ParseNativeBuildArgs({"styio", "build", "--help"}, args, error));
  EXPECT_TRUE(args.help);

  args = StyioNativeBuildArgsLatest{};
  ASSERT_TRUE(ParseNativeBuildArgs({"styio", "build", "input.styio", "-o", "out"}, args, error)) << error;
  EXPECT_EQ(args.input, fs::path("input.styio"));
  EXPECT_EQ(args.output, fs::path("out"));

  args = StyioNativeBuildArgsLatest{};
  EXPECT_FALSE(ParseNativeBuildArgs({"styio", "build", "-o"}, args, error));
  EXPECT_NE(error.find("requires a path after -o"), std::string::npos);
  args = StyioNativeBuildArgsLatest{};
  EXPECT_FALSE(ParseNativeBuildArgs({"styio", "build", "--bad"}, args, error));
  EXPECT_NE(error.find("unsupported styio build option"), std::string::npos);
  args = StyioNativeBuildArgsLatest{};
  EXPECT_FALSE(ParseNativeBuildArgs({"styio", "build", "a.styio", "b.styio", "-o", "out"}, args, error));
  EXPECT_NE(error.find("exactly one input file"), std::string::npos);
  args = StyioNativeBuildArgsLatest{};
  EXPECT_FALSE(ParseNativeBuildArgs({"styio", "build", "-o", "out"}, args, error));
  EXPECT_NE(error.find("requires an input file"), std::string::npos);
  args = StyioNativeBuildArgsLatest{};
  EXPECT_FALSE(ParseNativeBuildArgs({"styio", "build", "input.styio"}, args, error));
  EXPECT_NE(error.find("requires -o"), std::string::npos);

  std::ostringstream usage;
  styio_print_native_build_usage_latest(usage);
  EXPECT_NE(usage.str().find("Usage: styio build <file_path> -o <artifact_name>"), std::string::npos);

  TempDir temp("native-discovery");
  const fs::path tool = temp.path() / "fake-clang++";
  WriteText(tool, "#!/usr/bin/env sh\nexit 0\n");
#if defined(_WIN32)
  EXPECT_TRUE(styio_native_build_is_executable_file_latest(tool));
#else
  EXPECT_FALSE(styio_native_build_is_executable_file_latest(tool));
#endif
  MakeExecutable(tool);

  std::string command;
  EXPECT_TRUE(styio_native_build_find_executable_latest(tool.string(), command));
  EXPECT_EQ(command, tool.string());
  EXPECT_FALSE(styio_native_build_find_executable_latest((temp.path() / "missing").string(), command));

  EnvVarGuard path_guard("PATH");
  path_guard.set(temp.path().string());
  EXPECT_TRUE(styio_native_build_find_executable_latest("fake-clang++", command));
  EXPECT_EQ(command, (temp.path() / "fake-clang++").string());
  path_guard.unset();
  EXPECT_FALSE(styio_native_build_find_executable_latest("fake-clang++", command));
  EXPECT_FALSE(styio_native_build_find_executable_latest("", command));

  path_guard.set((temp.path() / "empty-bin").string());
  fs::create_directories(temp.path() / "empty-bin");
  EnvVarGuard cxx_guard("STYIO_NATIVE_CXX");
  EnvVarGuard toolchain_guard("STYIO_NATIVE_TOOLCHAIN_ROOT");
  cxx_guard.unset();
  toolchain_guard.set((temp.path() / "missing-toolchain").string());
#if defined(_WIN32)
  EXPECT_FALSE(
    styio_native_build_compiler_from_config_latest(temp.path() / "bin" / "styio", "g++").empty());
#else
  EXPECT_EQ(
    styio_native_build_compiler_from_config_latest(temp.path() / "bin" / "styio", "g++"),
    "clang++");
#endif

  const fs::path clang_root = temp.path() / "toolchain";
  WriteText(clang_root / "bin" / "clang++-18", "#!/usr/bin/env sh\nexit 0\n");
  MakeExecutable(clang_root / "bin" / "clang++-18");
  EXPECT_TRUE(styio_native_build_find_clang_in_root_latest(clang_root, command));
  EXPECT_EQ(command, (clang_root / "bin" / "clang++-18").string());
  EXPECT_FALSE(styio_native_build_find_clang_in_root_latest({}, command));
  EXPECT_TRUE(styio_native_build_command_looks_like_clang_cxx_latest("/opt/LLVM/bin/Clang++-18"));
  EXPECT_TRUE(styio_native_build_command_looks_like_clang_cxx_latest("clang-cl"));
  EXPECT_FALSE(styio_native_build_command_looks_like_clang_cxx_latest("g++"));
}

TEST(StyioMainContract, NativeBuildCompilerAndGeneratedArtifactsAreDeterministic) {
  TempDir temp("native-artifacts");
  std::string error;

  EnvVarGuard cxx_guard("STYIO_NATIVE_CXX");
  cxx_guard.set("/tmp/custom-native-cxx");
  EXPECT_EQ(styio_native_build_compiler_latest(temp.path() / "bin" / "styio"), "/tmp/custom-native-cxx");
  cxx_guard.unset();
  EXPECT_FALSE(styio_native_build_compiler_latest(temp.path() / "bin" / "styio").empty());

  const auto frontend_argv = styio_native_build_frontend_argv_latest(
    temp.path() / "styio self; touch nope",
    temp.path() / "plan file.json");
  ASSERT_EQ(frontend_argv.size(), 3u);
  EXPECT_EQ(frontend_argv[0], (temp.path() / "styio self; touch nope").string());
  EXPECT_EQ(frontend_argv[1], "--compile-plan");
  EXPECT_EQ(frontend_argv[2], (temp.path() / "plan file.json").string());

  const std::vector<fs::path> extern_objects = {
    temp.path() / "extern one.o",
    temp.path() / "extern;two.o",
  };
  const auto link_argv = styio_native_build_link_argv_latest(
    "/tmp/clang++; touch nope",
    temp.path() / "native ir.ll",
    temp.path() / "wrapper file.cpp",
    std::vector<fs::path>{temp.path() / "runtime file.cpp"},
    temp.path() / "include dir",
    extern_objects,
    temp.path() / "out file");
#if defined(_WIN32)
  ASSERT_EQ(link_argv.size(), 14u);
#else
  ASSERT_EQ(link_argv.size(), 16u);
#endif
  EXPECT_EQ(link_argv[0], "/tmp/clang++; touch nope");
  EXPECT_EQ(link_argv[1], "-std=c++20");
  EXPECT_EQ(link_argv[5], (temp.path() / "native ir.ll").string());
  EXPECT_EQ(link_argv[8], "-I");
  EXPECT_EQ(link_argv[9], (temp.path() / "include dir").string());
  EXPECT_EQ(link_argv[10], extern_objects[0].string());
  EXPECT_EQ(link_argv[11], extern_objects[1].string());
  EXPECT_EQ(link_argv[12], "-o");
  EXPECT_EQ(link_argv[13], (temp.path() / "out file").string());
#if !defined(_WIN32)
  EXPECT_EQ(link_argv[14], "-ldl");
  EXPECT_EQ(link_argv[15], "-pthread");
#endif

  const fs::path input = temp.path() / "src" / "main.styio";
  WriteText(input, "print(1)\n");
  const fs::path build_root = temp.path() / "build";
  const fs::path artifact_dir = build_root / "artifacts";
  const fs::path diag_dir = build_root / "diag";
  const fs::path plan = build_root / "plan.json";
  std::error_code create_ec;
  fs::create_directories(build_root, create_ec);
  ASSERT_FALSE(create_ec) << create_ec.message();
  ASSERT_TRUE(styio_native_build_write_compile_plan_latest(
    plan,
    input,
    build_root,
    artifact_dir,
    diag_dir,
    error)) << error;
  const std::string plan_text = ReadText(plan);
  EXPECT_NE(plan_text.find("\"plan_version\": 1"), std::string::npos);
  EXPECT_NE(plan_text.find("\"llvm_ir\": true"), std::string::npos);
  EXPECT_NE(plan_text.find(styio_json_escape(input.string())), std::string::npos);

  const fs::path wrapper = build_root / "artifact.wrapper.cpp";
  ASSERT_TRUE(styio_native_build_write_wrapper_latest(wrapper, error)) << error;
  const std::string wrapper_text = ReadText(wrapper);
  EXPECT_NE(wrapper_text.find("styio_user_main"), std::string::npos);
  EXPECT_NE(wrapper_text.find("STYIO_NATIVE_PROFILE_OUT"), std::string::npos);

  const fs::path compile_plan_ir = build_root / "artifact.llvm.ir";
  const fs::path native_ir = build_root / "artifact.native.ll";
  WriteText(compile_plan_ir, "define i32 @main() {\n  ret i32 0\n}\n");
  ASSERT_TRUE(styio_native_build_prepare_ir_latest(compile_plan_ir, native_ir, error)) << error;
  const std::string native_ir_text = ReadText(native_ir);
  EXPECT_NE(native_ir_text.find("define i32 @styio_user_main()"), std::string::npos);
  EXPECT_EQ(native_ir_text.find("define i32 @main("), std::string::npos);

  WriteText(compile_plan_ir, "define i32 @not_main() {\n  ret i32 0\n}\n");
  EXPECT_FALSE(styio_native_build_prepare_ir_latest(compile_plan_ir, native_ir, error));
  EXPECT_EQ(error, "generated LLVM IR does not define `main`");
  EXPECT_FALSE(styio_native_build_prepare_ir_latest(build_root / "missing.ll", native_ir, error));
  EXPECT_NE(error.find("cannot open file"), std::string::npos);

  WriteText(build_root / "native.log", "compiler log\n");
  EXPECT_EQ(styio_native_build_read_log_latest(build_root / "native.log"), "compiler log\n");
  EXPECT_EQ(styio_native_build_read_log_latest(build_root / "missing.log"), "");

  const fs::path temp_root = styio_create_native_build_temp_root_latest(error);
  ASSERT_FALSE(temp_root.empty()) << error;
  EXPECT_TRUE(fs::is_directory(temp_root));
  std::error_code ec;
  fs::remove_all(temp_root, ec);
}

TEST(StyioMainContract, NativeBuildArtifactWriteFailuresStayExplicit) {
  TempDir temp("native-artifact-write-failures");
  std::string error;

  const fs::path input = temp.path() / "src" / "main.styio";
  WriteText(input, "print(1)\n");
  const fs::path build_root = temp.path() / "build";
  const fs::path artifact_dir = build_root / "artifacts";
  const fs::path diag_dir = build_root / "diag";
  std::error_code ec;
  fs::create_directories(build_root, ec);
  ASSERT_FALSE(ec) << ec.message();

  const fs::path plan_directory = build_root / "plan-as-directory.json";
  fs::create_directories(plan_directory, ec);
  ASSERT_FALSE(ec) << ec.message();
  EXPECT_FALSE(styio_native_build_write_compile_plan_latest(
    plan_directory,
    input,
    build_root,
    artifact_dir,
    diag_dir,
    error));
  EXPECT_NE(error.find("cannot open file for writing"), std::string::npos) << error;

  const fs::path wrapper_directory = build_root / "wrapper-as-directory.cpp";
  fs::create_directories(wrapper_directory, ec);
  ASSERT_FALSE(ec) << ec.message();
  EXPECT_FALSE(styio_native_build_write_wrapper_latest(wrapper_directory, error));
  EXPECT_NE(error.find("cannot open file for writing"), std::string::npos) << error;

  const fs::path compile_plan_ir = build_root / "artifact.llvm.ir";
  const fs::path native_ir_directory = build_root / "native-ir-as-directory.ll";
  WriteText(compile_plan_ir, "define i32 @main() {\n  ret i32 0\n}\n");
  fs::create_directories(native_ir_directory, ec);
  ASSERT_FALSE(ec) << ec.message();
  EXPECT_FALSE(styio_native_build_prepare_ir_latest(compile_plan_ir, native_ir_directory, error));
  EXPECT_NE(error.find("cannot open file for writing"), std::string::npos) << error;

  const fs::path native_dir = build_root / "native-extern";
  fs::create_directories(native_dir / "extern-0.c", ec);
  ASSERT_FALSE(ec) << ec.message();
  std::vector<StyioNativeExternUnitLatest> units;
  units.push_back({"c", "int styio_native_write_fail(void) { return 0; }\n"});
  std::vector<fs::path> objects;
  EXPECT_FALSE(styio_native_build_compile_extern_units_latest(
    build_root,
    units,
    objects,
    error));
  EXPECT_TRUE(objects.empty());
  EXPECT_NE(error.find("cannot open file for writing"), std::string::npos) << error;
}

TEST(StyioMainContract, NativeExternCompileFailureIncludesCompilerLog) {
  TempDir temp("native-extern-failure");
  std::vector<StyioNativeExternUnitLatest> units;
  units.push_back({"c++", "extern \"C\" int styio_broken( {\n"});
  std::vector<fs::path> objects;
  std::string error;

  EXPECT_FALSE(styio_native_build_compile_extern_units_latest(
    temp.path(),
    units,
    objects,
    error
  ));
  EXPECT_TRUE(objects.empty());
  EXPECT_NE(error.find("native @extern artifact compile failed"), std::string::npos);
  EXPECT_NE(error.find("extern-0.cpp"), std::string::npos);

  const fs::path log_path = temp.path() / "native-extern" / "extern-0.log";
  EXPECT_TRUE(fs::exists(log_path));
  EXPECT_FALSE(ReadText(log_path).empty());
}

TEST(StyioMainContract, NativeExternCollectionAndDirectoryFailuresStayExplicit) {
  TempDir temp("native-extern-edges");
  std::string error;

  {
    std::vector<StyioNativeExternUnitLatest> units;
    EXPECT_FALSE(styio_native_build_collect_extern_units_latest(
      temp.path() / "missing.styio",
      units,
      error));
    EXPECT_TRUE(units.empty());
    EXPECT_NE(error.find("file not found"), std::string::npos);
  }

  {
    const fs::path broken_source = temp.path() / "broken.styio";
    WriteText(broken_source, "# broken := (a: i32) => a +\n");
    std::vector<StyioNativeExternUnitLatest> units;
    EXPECT_FALSE(styio_native_build_collect_extern_units_latest(
      broken_source,
      units,
      error));
    EXPECT_TRUE(units.empty());
    EXPECT_FALSE(error.empty());
  }

  {
    const fs::path build_root_file = temp.path() / "build-root-file";
    WriteText(build_root_file, "not a directory\n");
    std::vector<StyioNativeExternUnitLatest> units;
    std::vector<fs::path> objects;
    EXPECT_FALSE(styio_native_build_compile_extern_units_latest(
      build_root_file,
      units,
      objects,
      error));
    EXPECT_TRUE(objects.empty());
    EXPECT_NE(error.find("cannot create native @extern build directory"), std::string::npos);
  }
}

TEST(StyioMainContract, NativeExternCompileSuccessPersistsSourceAndObject) {
  TempDir temp("native-extern-success");
  std::vector<StyioNativeExternUnitLatest> units;
  units.push_back({"c", "int styio_native_add_one(int value) { return value + 1; }\n"});
  units.push_back({"c++", "extern \"C\" long long styio_native_answer() { return 42; }\n"});
  std::vector<fs::path> objects;
  std::string error;

  ASSERT_TRUE(styio_native_build_compile_extern_units_latest(
    temp.path(),
    units,
    objects,
    error
  )) << error;

  ASSERT_EQ(objects.size(), 2u);
  EXPECT_EQ(objects[0].filename(), fs::path(std::string("extern-0") + styio::platform::object_suffix()));
  EXPECT_EQ(objects[1].filename(), fs::path(std::string("extern-1") + styio::platform::object_suffix()));
  EXPECT_TRUE(fs::is_regular_file(objects[0]));
  EXPECT_TRUE(fs::is_regular_file(objects[1]));
  EXPECT_NE(ReadText(temp.path() / "native-extern" / "extern-0.c").find("styio_native_add_one"), std::string::npos);
  EXPECT_NE(ReadText(temp.path() / "native-extern" / "extern-1.cpp").find("styio_native_answer"), std::string::npos);
}

TEST(StyioMainContract, ParserShadowCompareReportsShadowEngineErrors) {
  TempDir temp("shadow-compare-error");
  const fs::path source = temp.path() / "await.styio";
  const fs::path shadow_dir = temp.path() / "shadow";
  WriteText(source, "?| task -> out: i64 | 0\n");

  const MainRunResult result = RunMain({
    "styio",
    "--file",
    source.string(),
    "--parser-engine=nightly",
    "--parser-shadow-compare",
    "--parser-shadow-artifact-dir=" + shadow_dir.string(),
    "--error-format=jsonl",
  });

  EXPECT_EQ(result.exit_code, static_cast<int>(StyioExitCode::ParseError));
  EXPECT_NE(result.stderr_text.find("shadow parser failed under --parser-shadow-compare"), std::string::npos);

  bool saw_jsonl = false;
  bool saw_shadow_error = false;
  for (const auto& entry : fs::directory_iterator(shadow_dir)) {
    const std::string path = entry.path().string();
    if (path.ends_with(".jsonl")) {
      saw_jsonl = true;
      saw_shadow_error =
        saw_shadow_error || ReadText(entry.path()).find("\"status\":\"shadow_error\"") != std::string::npos;
    }
  }
  EXPECT_TRUE(saw_jsonl);
  EXPECT_TRUE(saw_shadow_error);
}

TEST(StyioMainContract, ShadowArtifactWriterPersistsMismatchAndErrorPayloads) {
  TempDir temp("shadow-artifacts");
  const fs::path mismatch_dir = temp.path() / "mismatch";
  styio_write_shadow_artifact_latest(
    mismatch_dir.string(),
    "input.styio",
    "print(1)\n",
    StyioParserEngine::Legacy,
    StyioParserEngine::Nightly,
    "shadow_error",
    "primary ast",
    "shadow ast",
    "shadow exploded",
    "parse detail"
  );

  fs::path jsonl_path;
  fs::path source_path;
  fs::path primary_path;
  fs::path shadow_path;
  fs::path error_path;
  for (const auto& entry : fs::directory_iterator(mismatch_dir)) {
    const std::string path = entry.path().string();
    if (path.ends_with(".jsonl")) {
      jsonl_path = entry.path();
    }
    else if (path.ends_with(".styio")) {
      source_path = entry.path();
    }
    else if (path.ends_with(".primary.ast.txt")) {
      primary_path = entry.path();
    }
    else if (path.ends_with(".shadow.ast.txt")) {
      shadow_path = entry.path();
    }
    else if (path.ends_with(".shadow.err.txt")) {
      error_path = entry.path();
    }
  }

  ASSERT_FALSE(jsonl_path.empty());
  ASSERT_FALSE(source_path.empty());
  ASSERT_FALSE(primary_path.empty());
  ASSERT_FALSE(shadow_path.empty());
  ASSERT_FALSE(error_path.empty());
  EXPECT_NE(ReadText(jsonl_path).find("\"status\":\"shadow_error\""), std::string::npos);
  EXPECT_EQ(ReadText(source_path), "print(1)\n");
  EXPECT_EQ(ReadText(primary_path), "primary ast");
  EXPECT_EQ(ReadText(shadow_path), "shadow ast");
  EXPECT_EQ(ReadText(error_path), "shadow exploded");

  const fs::path match_dir = temp.path() / "match";
  styio_write_shadow_artifact_latest(
    match_dir.string(),
    "input.styio",
    "print(1)\n",
    StyioParserEngine::Nightly,
    StyioParserEngine::Legacy,
    "match",
    "same ast",
    "same ast",
    "",
    ""
  );

  std::size_t match_files = 0;
  fs::path match_jsonl;
  for (const auto& entry : fs::directory_iterator(match_dir)) {
    ++match_files;
    if (entry.path().string().ends_with(".jsonl")) {
      match_jsonl = entry.path();
    }
  }
  EXPECT_EQ(match_files, 1u);
  ASSERT_FALSE(match_jsonl.empty());
  EXPECT_NE(ReadText(match_jsonl).find("\"status\":\"match\""), std::string::npos);
}
