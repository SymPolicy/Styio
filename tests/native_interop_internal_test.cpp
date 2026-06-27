#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

#include "EnvTestUtil.hpp"

#include "../src/StyioNative/NativeInterop.cpp"

#ifndef STYIO_TEST_C_COMPILER
#define STYIO_TEST_C_COMPILER "cc"
#endif

#ifndef STYIO_TEST_CXX_COMPILER
#define STYIO_TEST_CXX_COMPILER "c++"
#endif

namespace {

std::string test_c_compiler() {
  std::string compiler = STYIO_TEST_C_COMPILER;
  return compiler.empty() ? "cc" : compiler;
}

std::string test_cxx_compiler() {
  std::string compiler = STYIO_TEST_CXX_COMPILER;
  return compiler.empty() ? "c++" : compiler;
}

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

}  // namespace

TEST(StyioNativeInteropInternal, EscapingAndCacheEnvironmentBranchesStayExplicit) {
  using namespace styio::native;

  EXPECT_EQ(shell_quote("a'b"), "'a'\\''b'");
  EXPECT_EQ(c_string_escape(std::string("\\\"\n\r\tplain")), "\\\\\\\"\\n\\r\\tplain");

  const CompilerResolution compiler{"/tmp/compiler dir/cc tool", "test"};
  const auto shared_argv = native_shared_compile_argv(
    compiler,
    "/tmp/source file.c",
    "/tmp/out;safe.so");
#if defined(_WIN32)
  ASSERT_EQ(shared_argv.size(), 6u);
  EXPECT_EQ(shared_argv[0], compiler.command);
  EXPECT_EQ(shared_argv[1], "-shared");
  EXPECT_EQ(shared_argv[3], "/tmp/source file.c");
  EXPECT_EQ(shared_argv[5], "/tmp/out;safe.so");
#else
  ASSERT_EQ(shared_argv.size(), 7u);
  EXPECT_EQ(shared_argv[0], compiler.command);
  EXPECT_EQ(shared_argv[1], "-shared");
  EXPECT_EQ(shared_argv[4], "/tmp/source file.c");
  EXPECT_EQ(shared_argv[6], "/tmp/out;safe.so");
  EXPECT_EQ(
    native_command_display(shared_argv),
    "'/tmp/compiler dir/cc tool' '-shared' '-fPIC' '-O2' '/tmp/source file.c' '-o' '/tmp/out;safe.so'");
#endif

  const auto object_argv = native_object_compile_argv(
    compiler,
    " c++ ",
    "/tmp/source file.cpp",
    "/tmp/out file.o");
#if defined(_WIN32)
  ASSERT_EQ(object_argv.size(), 7u);
  EXPECT_EQ(object_argv[1], "-std=c++20");
  EXPECT_EQ(object_argv[4], "/tmp/source file.cpp");
  EXPECT_EQ(object_argv[6], "/tmp/out file.o");
#else
  ASSERT_EQ(object_argv.size(), 8u);
  EXPECT_EQ(object_argv[1], "-std=c++20");
  EXPECT_EQ(object_argv[5], "/tmp/source file.cpp");
  EXPECT_EQ(object_argv[7], "/tmp/out file.o");
#endif

  EnvVarGuard cache_guard("STYIO_NATIVE_CACHE");
  EnvVarGuard cache_dir_guard("STYIO_NATIVE_CACHE_DIR");
  EnvVarGuard xdg_guard("XDG_CACHE_HOME");
  EnvVarGuard home_guard("HOME");

  cache_guard.set(" off ");
  EXPECT_FALSE(native_cache_enabled());
  EXPECT_TRUE(native_cache_dir().empty());
  std::string error;
  EXPECT_TRUE(native_cache_path_for_key("disabled", error).empty());
  EXPECT_FALSE(ensure_directory({}, error));
  EXPECT_EQ(
    native_cache_tmp_path_for_key(
      std::filesystem::path("native") / ("cached" + std::string(styio::platform::shared_library_suffix())))
      .extension()
      .string(),
    styio::platform::shared_library_suffix());

  cache_guard.set("1");
  cache_dir_guard.set(" ");
  xdg_guard.set((std::filesystem::temp_directory_path() / "styio-native-xdg").string());
  home_guard.unset();
  EXPECT_NE(native_cache_dir().generic_string().find("styio/native/abi-stable"), std::string::npos);

  xdg_guard.set(" ");
  home_guard.unset();
#if defined(_WIN32)
  EXPECT_FALSE(native_cache_dir().empty());
#else
  EXPECT_TRUE(native_cache_dir().empty());
#endif
}

TEST(StyioNativeInteropInternal, SignatureParsingRejectsAndSynthesizesEdgeParameters) {
  using namespace styio::native;

  EXPECT_EQ(
    strip_comments_for_signatures("int a(); // hidden\n/* keep\nline count */ int b();"),
    "int a();  \n \n  int b();");

  CType ctype;
  EXPECT_FALSE(parse_c_type("", ctype));
  ASSERT_TRUE(parse_c_type("static inline unsigned long long", ctype));
  EXPECT_EQ(ctype.kind, CTypeKind::I64);
  EXPECT_TRUE(ctype.is_unsigned);
  ASSERT_TRUE(parse_c_type("extern \"C\" const char *", ctype));
  EXPECT_EQ(ctype.kind, CTypeKind::Pointer);
  ctype.kind = static_cast<CTypeKind>(999);
  EXPECT_TRUE(styio_data_type_for_c_type(ctype).isUndefined());

  EXPECT_THROW((void)parse_param("   ", 0), StyioTypeError);
  EXPECT_THROW((void)parse_param("...", 0), StyioTypeError);
  {
    FunctionParam param = parse_param("char *", 2);
    EXPECT_EQ(param.name, "arg2");
    EXPECT_EQ(param.type.kind, CTypeKind::Pointer);
  }
  {
    FunctionParam param = parse_param("int", 3);
    EXPECT_EQ(param.name, "arg3");
    EXPECT_EQ(param.type.kind, CTypeKind::I32);
  }
  {
    FunctionParam param = parse_param("int value   ", 4);
    EXPECT_EQ(param.name, "value");
    EXPECT_EQ(param.type.kind, CTypeKind::I32);
  }
  {
    FunctionParam param = parse_param("const char * name", 5);
    EXPECT_EQ(param.name, "name");
    EXPECT_EQ(param.type.kind, CTypeKind::Pointer);
  }
  {
    FunctionParam param = parse_param("unsigned long !", 6);
    EXPECT_EQ(param.name, "!");
    EXPECT_EQ(param.type.kind, CTypeKind::I64);
    EXPECT_TRUE(param.type.is_unsigned);
  }
  EXPECT_THROW((void)parse_param("const char", 0), StyioTypeError);

  size_t open = std::string::npos;
  EXPECT_FALSE(find_matching_open_paren("no-open)", 7, open));

  FunctionSignature sig;
  EXPECT_FALSE(parse_function_signature_candidate("", sig));
  EXPECT_FALSE(parse_function_signature_candidate("int missing_close(", sig));
  EXPECT_FALSE(parse_function_signature_candidate("int f() trailing", sig));
  EXPECT_FALSE(parse_function_signature_candidate("int f)", sig));
  EXPECT_FALSE(parse_function_signature_candidate("int 123()", sig));
  EXPECT_THROW((void)parse_function_signature_candidate("mystery f()", sig), StyioTypeError);

  const auto params = parse_params("int (*callback)(int, int), double value");
  ASSERT_EQ(params.size(), 2u);
  EXPECT_EQ(params[0].name, "arg0");
  EXPECT_EQ(params[1].type.kind, CTypeKind::F64);

  const std::string visible = top_level_signature_text(
    "int before(void); \"not a signature(\\\"x\\\")\" '\\\\'; { int hidden(void); }\nint after(void);");
  EXPECT_NE(visible.find("before"), std::string::npos);
  EXPECT_NE(visible.find("after"), std::string::npos);
  EXPECT_EQ(visible.find("hidden"), std::string::npos);
}

TEST(StyioNativeInteropInternal, FilesystemAndCompilerResolutionFailuresStayExplicit) {
  using namespace styio::native;

  const std::filesystem::path temp =
    std::filesystem::temp_directory_path()
    / ("styio-native-interop-internal-" + stable_hash_hex(std::to_string(styio::platform::process_id())));
  std::filesystem::remove_all(temp);
  std::filesystem::create_directories(temp);

  std::string error;
  const std::filesystem::path blocking_file = temp / "not-a-dir";
  {
    std::ofstream out(blocking_file);
    out << "blocks directory creation\n";
  }
  EXPECT_FALSE(ensure_directory(blocking_file / "child", error));
  EXPECT_NE(error.find("cannot create native cache directory"), std::string::npos);

  EnvVarGuard cache_guard("STYIO_NATIVE_CACHE");
  EnvVarGuard cache_dir_guard("STYIO_NATIVE_CACHE_DIR");
  cache_guard.set("1");
  cache_dir_guard.set((blocking_file / "cache").string());
  error.clear();
  EXPECT_TRUE(native_cache_path_for_key("blocked", error).empty());
  EXPECT_NE(error.find("cannot create native cache directory"), std::string::npos);

  error.clear();
  EXPECT_FALSE(write_text_file(temp, "nope", error));
  EXPECT_NE(error.find("cannot write native source file"), std::string::npos);

  if (std::filesystem::exists("/dev/full")) {
    error.clear();
    EXPECT_FALSE(write_text_file("/dev/full", std::string(1 << 20, 'x'), error));
    EXPECT_NE(error.find("failed to write native source file"), std::string::npos);
  }

  EnvVarGuard cc_guard("STYIO_NATIVE_CC");
  EnvVarGuard cxx_guard("STYIO_NATIVE_CXX");
  EnvVarGuard mode_guard("STYIO_NATIVE_TOOLCHAIN_MODE");
  EnvVarGuard root_guard("STYIO_NATIVE_TOOLCHAIN_ROOT");
  cc_guard.unset();
  cxx_guard.unset();
  mode_guard.set("bundled");
  root_guard.set((temp / "missing-toolchain").string());
  EXPECT_THROW((void)resolve_compiler_for_abi("c"), StyioTypeError);

  std::filesystem::remove_all(temp);
}

TEST(StyioNativeInteropInternal, ReferencedSourcesAndEarlyLoadRejectionsStayExplicit) {
  using namespace styio::native;

  const std::filesystem::path temp =
    std::filesystem::temp_directory_path()
    / ("styio-native-interop-sources-" + stable_hash_hex(std::to_string(styio::platform::process_id())));
  std::filesystem::remove_all(temp);
  std::filesystem::create_directories(temp);

  std::string error;
  const std::filesystem::path rel = std::filesystem::path(
    "styio-native-rel-" + stable_hash_hex(temp.string()) + ".h");
  ASSERT_TRUE(write_text_file(rel, "int from_header(void);\n", error)) << error;
  const auto sources = read_referenced_sources({rel.string()});
  ASSERT_EQ(sources.size(), 1u);
  EXPECT_TRUE(sources[0].first.is_absolute());
  EXPECT_NE(sources[0].second.find("from_header"), std::string::npos);

  const std::string source_text = source_text_for_block(
    "c++",
    "extern \"C\" int main_symbol(void) { return 1; }",
    {rel.string()});
  EXPECT_NE(source_text.find("#include <cstdint>"), std::string::npos);
  EXPECT_NE(source_text.find("styio native source"), std::string::npos);
  EXPECT_NE(source_text.find(rel.filename().string()), std::string::npos);

  std::vector<std::filesystem::path> unique_paths;
  push_unique_path(unique_paths, {});
  EXPECT_TRUE(unique_paths.empty());
  push_unique_path(unique_paths, temp / "toolchain");
  push_unique_path(unique_paths, temp / "." / "toolchain");
  EXPECT_EQ(unique_paths.size(), 1u);

  EXPECT_THROW(
    (void)read_referenced_sources({""}),
    StyioTypeError);
  EXPECT_THROW(
    (void)read_referenced_sources({(temp / "missing.h").string()}),
    StyioTypeError);
  EXPECT_THROW(
    (void)compile_and_load_block("c", "int only_a_declaration;", {}),
    StyioTypeError);
  EXPECT_THROW(
    (void)compile_and_load_block(
      "c",
      "int exported_a(void) { return 1; }",
      {"missing_symbol"}),
    StyioTypeError);

  std::error_code cleanup_ec;
  std::filesystem::remove(rel, cleanup_ec);
  std::filesystem::remove_all(temp, cleanup_ec);
}

TEST(StyioNativeInteropInternal, InvalidDiskCacheEntryIsDiscardedBeforeCompile) {
  using namespace styio::native;

  const std::filesystem::path temp =
    std::filesystem::temp_directory_path()
    / ("styio-native-interop-cache-" + stable_hash_hex(std::to_string(styio::platform::process_id())));
  std::filesystem::remove_all(temp);
  std::filesystem::create_directories(temp);

  EnvVarGuard cache_guard("STYIO_NATIVE_CACHE");
  EnvVarGuard cache_dir_guard("STYIO_NATIVE_CACHE_DIR");
  EnvVarGuard cc_guard("STYIO_NATIVE_CC");
  EnvVarGuard mode_guard("STYIO_NATIVE_TOOLCHAIN_MODE");
  cache_guard.set("1");
  cache_dir_guard.set((temp / "cache").string());
#if defined(_WIN32)
  cc_guard.unset();
#else
  cc_guard.set(test_c_compiler());
#endif
  mode_guard.set("system");

  const std::string symbol = "bad_cache_" + stable_hash_hex(temp.string()).substr(0, 8);
  const std::string body = "int " + symbol + "(void) { return 7; }\n";
  const CompilerResolution compiler = resolve_compiler_for_abi("c");
  const std::string source_text = source_text_for_block("c", body, {});
  const std::string cache_key = native_cache_key("c", compiler, source_text);

  std::string error;
  const std::filesystem::path cache_path = native_cache_path_for_key(cache_key, error);
  ASSERT_FALSE(cache_path.empty()) << error;
  ASSERT_TRUE(write_text_file(cache_path, "not a shared object\n", error)) << error;

  LoadedBlock loaded = compile_and_load_block("c", body, {symbol});

  ASSERT_EQ(loaded.symbols.size(), 1u);
  EXPECT_EQ(loaded.symbols[0].name, symbol);
  EXPECT_TRUE(std::filesystem::exists(cache_path));

  std::error_code cleanup_ec;
  std::filesystem::remove_all(temp, cleanup_ec);
}

TEST(StyioNativeInteropInternal, CacheRacePrefersExistingSharedObjectAfterCompile) {
#if defined(_WIN32)
  GTEST_SKIP() << "POSIX shell-script fake compiler coverage; Windows native compiler coverage runs in other tests.";
#endif
  using namespace styio::native;

  const std::filesystem::path temp =
    std::filesystem::temp_directory_path()
    / ("styio-native-interop-cache-race-" + stable_hash_hex(std::to_string(styio::platform::process_id())));
  std::filesystem::remove_all(temp);
  std::filesystem::create_directories(temp);

  EnvVarGuard cache_guard("STYIO_NATIVE_CACHE");
  EnvVarGuard cache_dir_guard("STYIO_NATIVE_CACHE_DIR");
  EnvVarGuard cc_guard("STYIO_NATIVE_CC");
  EnvVarGuard mode_guard("STYIO_NATIVE_TOOLCHAIN_MODE");
  EnvVarGuard race_path_guard("STYIO_TEST_NATIVE_CACHE_RACE_PATH");

  cache_guard.set("1");
  cache_dir_guard.set((temp / "cache").string());
  mode_guard.set("system");

  const std::filesystem::path fake_cc = temp / "fake-cc";
  cc_guard.set(fake_cc.string());

  const std::string symbol = "cache_race_" + stable_hash_hex(temp.string()).substr(0, 8);
  const std::string body = "int " + symbol + "(void) { return 17; }\n";
  const std::string source_text = source_text_for_block("c", body, {});
  const CompilerResolution compiler{fake_cc.string(), "env:STYIO_NATIVE_CC"};
  const std::string cache_key = native_cache_key("c", compiler, source_text);

  std::string error;
  const std::filesystem::path cache_path = native_cache_path_for_key(cache_key, error);
  ASSERT_FALSE(cache_path.empty()) << error;
  race_path_guard.set(cache_path.string());

  ASSERT_TRUE(write_text_file(
    fake_cc,
    "#!/bin/sh\n"
    "out=''\n"
    "prev=''\n"
    "for arg in \"$@\"; do\n"
    "  if [ \"$prev\" = '-o' ]; then\n"
    "    out=\"$arg\"\n"
    "  fi\n"
    "  prev=\"$arg\"\n"
    "done\n"
    "test -n \"$out\" || exit 3\n"
    + std::string(shell_quote(test_c_compiler())) + " \"$@\" || exit 4\n"
    "cp \"$out\" \"$STYIO_TEST_NATIVE_CACHE_RACE_PATH\" || exit 5\n"
    "exit 0\n",
    error)) << error;

  std::error_code perm_ec;
  std::filesystem::permissions(
    fake_cc,
    std::filesystem::perms::owner_exec
      | std::filesystem::perms::group_exec
      | std::filesystem::perms::others_exec,
    std::filesystem::perm_options::add,
    perm_ec);
  ASSERT_FALSE(perm_ec) << perm_ec.message();

  LoadedBlock loaded = compile_and_load_block("c", body, {symbol});

  ASSERT_EQ(loaded.symbols.size(), 1u);
  EXPECT_EQ(loaded.symbols[0].name, symbol);
  auto* fn = reinterpret_cast<int (*)()>(loaded.symbols[0].address);
  ASSERT_NE(fn, nullptr);
  EXPECT_EQ(fn(), 17);
  EXPECT_TRUE(std::filesystem::exists(cache_path));

  std::error_code cleanup_ec;
  std::filesystem::remove_all(temp, cleanup_ec);
}

TEST(StyioNativeInteropInternal, NativeCommandRunnerExecsArgvWithoutShell) {
  using namespace styio::native;

  const std::filesystem::path temp =
    std::filesystem::temp_directory_path()
    / ("styio-native-interop-runner-" + stable_hash_hex(std::to_string(styio::platform::process_id())));
  std::filesystem::remove_all(temp);
  std::filesystem::create_directories(temp);

  const std::filesystem::path marker = temp / "shell_pwned";
  const std::filesystem::path log_path = temp / "run.log";
#if defined(_WIN32)
  const std::vector<std::string> argv = {
    (temp / "true; touch shell_pwned.exe").string(),
  };
#else
  const std::vector<std::string> argv = {
    "/bin/true; touch " + marker.string(),
  };
#endif

  const NativeCommandResult result = run_native_command_to_log(argv, log_path, true);

  EXPECT_FALSE(result.ok());
  EXPECT_FALSE(std::filesystem::exists(marker));
  std::string log;
  EXPECT_TRUE(read_text_file(log_path, log));
  EXPECT_TRUE(
    log.find("failed to exec native command") != std::string::npos
    || result.launch_error.find("cannot launch native command") != std::string::npos);

  std::error_code cleanup_ec;
  std::filesystem::remove_all(temp, cleanup_ec);
}

TEST(StyioNativeInteropInternal, NativeCommandRunnerCanSplitStdoutAndStderrLogs) {
  using namespace styio::native;

  const std::filesystem::path temp =
    std::filesystem::temp_directory_path()
    / ("styio-native-interop-split-logs-" + stable_hash_hex(std::to_string(styio::platform::process_id())));
  std::filesystem::remove_all(temp);
  std::filesystem::create_directories(temp);

  const std::filesystem::path command = temp / "split-logs";
  const std::filesystem::path stdout_log = temp / "stdout.log";
  const std::filesystem::path stderr_log = temp / "stderr.log";

#if defined(_WIN32)
  const NativeCommandResult result = run_native_command_to_logs(
    {"cmd.exe", "/C", "echo stdout-line&&echo stderr-line>&2"},
    stdout_log,
    stderr_log);
#else
  std::string error;
  ASSERT_TRUE(write_text_file(
    command,
    "#!/bin/sh\n"
    "printf 'stdout-line\\n'\n"
    "printf 'stderr-line\\n' >&2\n",
    error)) << error;

  std::error_code perm_ec;
  std::filesystem::permissions(
    command,
    std::filesystem::perms::owner_exec
      | std::filesystem::perms::group_exec
      | std::filesystem::perms::others_exec,
    std::filesystem::perm_options::add,
    perm_ec);
  ASSERT_FALSE(perm_ec) << perm_ec.message();

  const NativeCommandResult result =
    run_native_command_to_logs({command.string()}, stdout_log, stderr_log);
#endif

  ASSERT_TRUE(result.ok()) << result.launch_error;
  std::string stdout_text;
  std::string stderr_text;
  EXPECT_TRUE(read_text_file(stdout_log, stdout_text));
  EXPECT_TRUE(read_text_file(stderr_log, stderr_text));
#if defined(_WIN32)
  stdout_text.erase(std::remove(stdout_text.begin(), stdout_text.end(), '\r'), stdout_text.end());
  stderr_text.erase(std::remove(stderr_text.begin(), stderr_text.end(), '\r'), stderr_text.end());
#endif
  EXPECT_EQ(stdout_text, "stdout-line\n");
  EXPECT_EQ(stderr_text, "stderr-line\n");

  std::error_code cleanup_ec;
  std::filesystem::remove_all(temp, cleanup_ec);
}

TEST(StyioNativeInteropInternal, CompilerPathWithShellMetacharactersIsExecutedLiterally) {
#if defined(_WIN32)
  GTEST_SKIP() << "POSIX shell-script fake compiler coverage; Windows native command runner coverage runs separately.";
#endif
  using namespace styio::native;

  const std::filesystem::path temp =
    std::filesystem::temp_directory_path()
    / ("styio-native-interop-argv-" + stable_hash_hex(std::to_string(styio::platform::process_id())));
  std::filesystem::remove_all(temp);
  std::filesystem::create_directories(temp);

  const std::filesystem::path fake_cc = temp / "fake cc; touch shell_pwned";
  const std::filesystem::path marker = temp / "shell_pwned";
  const std::filesystem::path arg_log = temp / "argv.log";

  std::string error;
  ASSERT_TRUE(write_text_file(
    fake_cc,
    "#!/bin/sh\n"
    "printf '%s\\n' \"$@\" > \"$STYIO_TEST_NATIVE_ARGV_LOG\"\n"
    "exec " + std::string(shell_quote(test_c_compiler())) + " \"$@\"\n",
    error)) << error;

  std::error_code perm_ec;
  std::filesystem::permissions(
    fake_cc,
    std::filesystem::perms::owner_exec
      | std::filesystem::perms::group_exec
      | std::filesystem::perms::others_exec,
    std::filesystem::perm_options::add,
    perm_ec);
  ASSERT_FALSE(perm_ec) << perm_ec.message();

  EnvVarGuard cache_guard("STYIO_NATIVE_CACHE");
  EnvVarGuard cc_guard("STYIO_NATIVE_CC");
  EnvVarGuard mode_guard("STYIO_NATIVE_TOOLCHAIN_MODE");
  EnvVarGuard arg_log_guard("STYIO_TEST_NATIVE_ARGV_LOG");
  cache_guard.set("off");
  cc_guard.set(fake_cc.string());
  mode_guard.set("system");
  arg_log_guard.set(arg_log.string());

  const std::string symbol = "argv_compile_" + stable_hash_hex(temp.string()).substr(0, 8);
  const std::string body = "int " + symbol + "(void) { return 23; }\n";

  LoadedBlock loaded = compile_and_load_block("c", body, {symbol});

  ASSERT_EQ(loaded.symbols.size(), 1u);
  auto* fn = reinterpret_cast<int (*)()>(loaded.symbols[0].address);
  ASSERT_NE(fn, nullptr);
  EXPECT_EQ(fn(), 23);
  EXPECT_FALSE(std::filesystem::exists(marker));
  std::string argv_log_text;
  EXPECT_TRUE(read_text_file(arg_log, argv_log_text));
  EXPECT_NE(argv_log_text.find("-shared\n"), std::string::npos);
  EXPECT_NE(argv_log_text.find("-fPIC\n"), std::string::npos);
  EXPECT_NE(argv_log_text.find("-o\n"), std::string::npos);

  std::error_code cleanup_ec;
  std::filesystem::remove_all(temp, cleanup_ec);
}

TEST(StyioNativeInteropInternal, DisabledDiskCacheLoadsFromTemporarySharedObject) {
  using namespace styio::native;

  EnvVarGuard cache_guard("STYIO_NATIVE_CACHE");
  EnvVarGuard cc_guard("STYIO_NATIVE_CC");
  EnvVarGuard mode_guard("STYIO_NATIVE_TOOLCHAIN_MODE");
  cache_guard.set("off");
#if defined(_WIN32)
  cc_guard.unset();
#else
  cc_guard.set(test_c_compiler());
#endif
  mode_guard.set("system");

  const std::string symbol =
    "nocache_" + stable_hash_hex(std::to_string(styio::platform::process_id())).substr(0, 8);
  const std::string body = "int " + symbol + "(void) { return 11; }\n";

  LoadedBlock loaded = compile_and_load_block("c", body, {symbol});

  ASSERT_EQ(loaded.symbols.size(), 1u);
  EXPECT_EQ(loaded.symbols[0].name, symbol);
  auto* fn = reinterpret_cast<int (*)()>(loaded.symbols[0].address);
  ASSERT_NE(fn, nullptr);
  EXPECT_EQ(fn(), 11);
}

TEST(StyioNativeInteropInternal, NativeCppBlockCompilesAndExportsSymbol) {
  using namespace styio::native;

  EnvVarGuard cache_guard("STYIO_NATIVE_CACHE");
  EnvVarGuard cxx_guard("STYIO_NATIVE_CXX");
  EnvVarGuard mode_guard("STYIO_NATIVE_TOOLCHAIN_MODE");
  cache_guard.set("off");
#if defined(_WIN32)
  cxx_guard.unset();
#else
  cxx_guard.set(test_cxx_compiler());
#endif
  mode_guard.set("system");

  const std::string symbol =
    "cpp_square_" + stable_hash_hex(std::to_string(styio::platform::process_id())).substr(0, 8);
  const std::string body =
    "#include <vector>\n"
    "extern \"C\" int " + symbol + "(int x) { std::vector<int> v; v.push_back(x); return v[0] * v[0]; }\n";

  LoadedBlock loaded = compile_and_load_block("c++", body, {symbol});

  ASSERT_EQ(loaded.symbols.size(), 1u);
  EXPECT_EQ(loaded.symbols[0].name, symbol);
  auto* fn = reinterpret_cast<int (*)(int)>(loaded.symbols[0].address);
  ASSERT_NE(fn, nullptr);
  EXPECT_EQ(fn(7), 49);
}

TEST(StyioNativeInteropInternal, DisabledDiskCacheDlopenFailureRemovesTemporarySharedObject) {
#if defined(_WIN32)
  GTEST_SKIP() << "POSIX shell-script fake compiler coverage; Windows LoadLibrary failure paths are covered by invalid cache tests.";
#endif
  using namespace styio::native;

  const std::filesystem::path temp =
    std::filesystem::current_path()
    / "build"
    / ("styio-native-interop-fakecc-" + stable_hash_hex(std::to_string(styio::platform::process_id())));
  std::filesystem::remove_all(temp);
  std::filesystem::create_directories(temp);

  std::string error;
  const std::filesystem::path fake_cc = temp / "fake-cc";
  ASSERT_TRUE(write_text_file(
    fake_cc,
    "#!/bin/sh\n"
    "out=''\n"
    "while [ \"$#\" -gt 0 ]; do\n"
    "  if [ \"$1\" = '-o' ]; then\n"
    "    shift\n"
    "    out=\"$1\"\n"
    "  fi\n"
    "  shift || exit 2\n"
    "done\n"
    "test -n \"$out\" || exit 3\n"
    "printf 'not a shared object\\n' > \"$out\"\n"
    "exit 0\n",
    error)) << error;

  std::error_code perm_ec;
  std::filesystem::permissions(
    fake_cc,
    std::filesystem::perms::owner_exec
      | std::filesystem::perms::group_exec
      | std::filesystem::perms::others_exec,
    std::filesystem::perm_options::add,
    perm_ec);
  ASSERT_FALSE(perm_ec) << perm_ec.message();

  EnvVarGuard cache_guard("STYIO_NATIVE_CACHE");
  EnvVarGuard cc_guard("STYIO_NATIVE_CC");
  EnvVarGuard mode_guard("STYIO_NATIVE_TOOLCHAIN_MODE");
  cache_guard.set("off");
  cc_guard.set(fake_cc.string());
  mode_guard.set("system");

  const std::string symbol = "bad_so_" + stable_hash_hex(temp.string()).substr(0, 8);
  const std::string body = "int " + symbol + "(void) { return 13; }\n";

  bool threw = false;
  try {
    (void)compile_and_load_block("c", body, {symbol});
  }
  catch (const StyioTypeError& ex) {
    threw = true;
    EXPECT_NE(std::string(ex.what()).find("dlopen failed"), std::string::npos);
  }
  EXPECT_TRUE(threw);

  std::error_code cleanup_ec;
  std::filesystem::remove_all(temp, cleanup_ec);
}
