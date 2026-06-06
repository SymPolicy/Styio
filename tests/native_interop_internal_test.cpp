#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

#include "../src/StyioNative/NativeInterop.cpp"

namespace {

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
      setenv(name_.c_str(), original_->c_str(), 1);
    }
    else {
      unsetenv(name_.c_str());
    }
  }

  void set(const std::string& value) {
    setenv(name_.c_str(), value.c_str(), 1);
  }

  void unset() {
    unsetenv(name_.c_str());
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

  cache_guard.set("1");
  cache_dir_guard.set(" ");
  xdg_guard.set((std::filesystem::temp_directory_path() / "styio-native-xdg").string());
  home_guard.unset();
  EXPECT_NE(native_cache_dir().string().find("styio/native/v1"), std::string::npos);

  xdg_guard.set(" ");
  home_guard.unset();
  EXPECT_TRUE(native_cache_dir().empty());
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
    / ("styio-native-interop-internal-" + stable_hash_hex(std::to_string(::getpid())));
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
    / ("styio-native-interop-sources-" + stable_hash_hex(std::to_string(::getpid())));
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
