#include "Platform.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <system_error>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <cerrno>
#include <dlfcn.h>
#include <fcntl.h>
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace styio::platform {
namespace {

#if defined(_WIN32)
std::wstring
wide_from_utf8(const std::string& text) {
  if (text.empty()) {
    return {};
  }
  int size = ::MultiByteToWideChar(
    CP_UTF8,
    MB_ERR_INVALID_CHARS,
    text.data(),
    static_cast<int>(text.size()),
    nullptr,
    0);
  UINT code_page = CP_UTF8;
  DWORD flags = MB_ERR_INVALID_CHARS;
  if (size == 0) {
    code_page = CP_ACP;
    flags = 0;
    size = ::MultiByteToWideChar(
      code_page,
      flags,
      text.data(),
      static_cast<int>(text.size()),
      nullptr,
      0);
  }
  if (size <= 0) {
    return std::wstring(text.begin(), text.end());
  }
  std::wstring out(static_cast<size_t>(size), L'\0');
  (void)::MultiByteToWideChar(
    code_page,
    flags,
    text.data(),
    static_cast<int>(text.size()),
    out.data(),
    size);
  return out;
}

std::string
utf8_from_wide(const std::wstring& text) {
  if (text.empty()) {
    return {};
  }
  const int size = ::WideCharToMultiByte(
    CP_UTF8,
    0,
    text.data(),
    static_cast<int>(text.size()),
    nullptr,
    0,
    nullptr,
    nullptr);
  if (size <= 0) {
    std::string fallback;
    fallback.reserve(text.size());
    for (wchar_t ch : text) {
      fallback.push_back(ch >= 0 && ch <= 0x7f ? static_cast<char>(ch) : '?');
    }
    return fallback;
  }
  std::string out(static_cast<size_t>(size), '\0');
  (void)::WideCharToMultiByte(
    CP_UTF8,
    0,
    text.data(),
    static_cast<int>(text.size()),
    out.data(),
    size,
    nullptr,
    nullptr);
  return out;
}

std::string
windows_error_message(DWORD error_code) {
  if (error_code == 0) {
    return "unknown Windows error";
  }
  LPWSTR raw = nullptr;
  const DWORD size = ::FormatMessageW(
    FORMAT_MESSAGE_ALLOCATE_BUFFER
      | FORMAT_MESSAGE_FROM_SYSTEM
      | FORMAT_MESSAGE_IGNORE_INSERTS,
    nullptr,
    error_code,
    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
    reinterpret_cast<LPWSTR>(&raw),
    0,
    nullptr);
  if (size == 0 || raw == nullptr) {
    return "Windows error " + std::to_string(error_code);
  }
  std::wstring wide(raw, raw + size);
  ::LocalFree(raw);
  while (!wide.empty() && (wide.back() == L'\n' || wide.back() == L'\r' || wide.back() == L' ')) {
    wide.pop_back();
  }
  return utf8_from_wide(wide);
}

std::wstring
quote_windows_arg(const std::wstring& arg) {
  if (arg.empty()) {
    return L"\"\"";
  }
  const bool needs_quotes =
    arg.find_first_of(L" \t\n\v\"") != std::wstring::npos;
  if (!needs_quotes) {
    return arg;
  }

  std::wstring out = L"\"";
  size_t backslashes = 0;
  for (wchar_t ch : arg) {
    if (ch == L'\\') {
      ++backslashes;
      continue;
    }
    if (ch == L'"') {
      out.append(backslashes * 2 + 1, L'\\');
      out.push_back(ch);
      backslashes = 0;
      continue;
    }
    out.append(backslashes, L'\\');
    backslashes = 0;
    out.push_back(ch);
  }
  out.append(backslashes * 2, L'\\');
  out.push_back(L'"');
  return out;
}

std::wstring
command_line_from_argv(const std::vector<std::string>& argv) {
  std::wstring command_line;
  for (const auto& arg : argv) {
    if (!command_line.empty()) {
      command_line.push_back(L' ');
    }
    command_line += quote_windows_arg(wide_from_utf8(arg));
  }
  return command_line;
}

HANDLE
open_inherited_file(
  const std::filesystem::path& path,
  DWORD desired_access,
  DWORD creation_disposition,
  std::string& error_message
) {
  SECURITY_ATTRIBUTES security{};
  security.nLength = sizeof(security);
  security.bInheritHandle = TRUE;
  HANDLE handle = ::CreateFileW(
    path.wstring().c_str(),
    desired_access,
    FILE_SHARE_READ | FILE_SHARE_WRITE,
    &security,
    creation_disposition,
    FILE_ATTRIBUTE_NORMAL,
    nullptr);
  if (handle == INVALID_HANDLE_VALUE) {
    error_message = "cannot open process log `" + path.string() + "`: "
      + windows_error_message(::GetLastError());
  }
  return handle;
}

HANDLE
open_inherited_null(DWORD desired_access) {
  SECURITY_ATTRIBUTES security{};
  security.nLength = sizeof(security);
  security.bInheritHandle = TRUE;
  return ::CreateFileW(
    L"NUL",
    desired_access,
    FILE_SHARE_READ | FILE_SHARE_WRITE,
    &security,
    OPEN_EXISTING,
    FILE_ATTRIBUTE_NORMAL,
    nullptr);
}

HANDLE
duplicate_inherited_handle(HANDLE handle) {
  if (handle == nullptr || handle == INVALID_HANDLE_VALUE) {
    return INVALID_HANDLE_VALUE;
  }
  HANDLE inherited = INVALID_HANDLE_VALUE;
  const BOOL ok = ::DuplicateHandle(
    ::GetCurrentProcess(),
    handle,
    ::GetCurrentProcess(),
    &inherited,
    0,
    TRUE,
    DUPLICATE_SAME_ACCESS);
  return ok ? inherited : INVALID_HANDLE_VALUE;
}

ProcessResult
run_process_with_handles(
  const std::vector<std::string>& argv,
  HANDLE stdin_handle,
  HANDLE stdout_handle,
  HANDLE stderr_handle
) {
  if (argv.empty() || argv[0].empty()) {
    return ProcessResult{127, "native command argv is empty"};
  }

  HANDLE owned_stdin = INVALID_HANDLE_VALUE;
  HANDLE child_stdin = stdin_handle;
  if (child_stdin == INVALID_HANDLE_VALUE) {
    owned_stdin = duplicate_inherited_handle(::GetStdHandle(STD_INPUT_HANDLE));
    child_stdin = owned_stdin;
  }
  if (child_stdin == INVALID_HANDLE_VALUE) {
    owned_stdin = open_inherited_null(GENERIC_READ);
    child_stdin = owned_stdin;
  }

  STARTUPINFOW startup{};
  startup.cb = sizeof(startup);
  startup.dwFlags = STARTF_USESTDHANDLES;
  startup.hStdInput = child_stdin;
  startup.hStdOutput = stdout_handle;
  startup.hStdError = stderr_handle;

  PROCESS_INFORMATION process{};
  std::wstring command_line = command_line_from_argv(argv);
  const BOOL created = ::CreateProcessW(
    nullptr,
    command_line.data(),
    nullptr,
    nullptr,
    TRUE,
    0,
    nullptr,
    nullptr,
    &startup,
    &process);

  if (owned_stdin != nullptr && owned_stdin != INVALID_HANDLE_VALUE) {
    ::CloseHandle(owned_stdin);
  }

  if (!created) {
    return ProcessResult{
      127,
      "cannot launch native command: " + windows_error_message(::GetLastError())};
  }

  const DWORD wait_result = ::WaitForSingleObject(process.hProcess, INFINITE);
  DWORD exit_code = 127;
  if (wait_result == WAIT_FAILED) {
    const std::string message =
      "cannot wait for native command: " + windows_error_message(::GetLastError());
    ::CloseHandle(process.hThread);
    ::CloseHandle(process.hProcess);
    return ProcessResult{127, message};
  }
  if (!::GetExitCodeProcess(process.hProcess, &exit_code)) {
    const std::string message =
      "cannot read native command exit code: " + windows_error_message(::GetLastError());
    ::CloseHandle(process.hThread);
    ::CloseHandle(process.hProcess);
    return ProcessResult{127, message};
  }

  ::CloseHandle(process.hThread);
  ::CloseHandle(process.hProcess);
  return ProcessResult{static_cast<int>(exit_code), ""};
}
#else
void
close_fd(int fd) {
  if (fd >= 0) {
    (void)::close(fd);
  }
}

void
close_fds(int stdin_fd, int stdout_fd, int stderr_fd) {
  close_fd(stdin_fd);
  if (stdout_fd != stdin_fd) {
    close_fd(stdout_fd);
  }
  if (stderr_fd != stdout_fd && stderr_fd != stdin_fd) {
    close_fd(stderr_fd);
  }
}

ProcessResult
run_process_with_fds(
  const std::vector<std::string>& argv,
  int stdin_fd,
  int stdout_fd,
  int stderr_fd
) {
  if (argv.empty() || argv[0].empty()) {
    close_fds(stdin_fd, stdout_fd, stderr_fd);
    return ProcessResult{127, "native command argv is empty"};
  }

  std::vector<char*> exec_argv;
  exec_argv.reserve(argv.size() + 1);
  for (const std::string& arg : argv) {
    exec_argv.push_back(const_cast<char*>(arg.c_str()));
  }
  exec_argv.push_back(nullptr);

  const bool command_has_path_separator =
    argv[0].find('/') != std::string::npos;
  const pid_t pid = ::fork();
  if (pid < 0) {
    close_fds(stdin_fd, stdout_fd, stderr_fd);
    return ProcessResult{
      127,
      std::string("cannot fork native command: ") + std::strerror(errno)};
  }

  if (pid == 0) {
    if (stdin_fd >= 0 && ::dup2(stdin_fd, STDIN_FILENO) < 0) {
      _exit(126);
    }
    if (stdout_fd >= 0 && ::dup2(stdout_fd, STDOUT_FILENO) < 0) {
      _exit(126);
    }
    if (stderr_fd >= 0 && ::dup2(stderr_fd, STDERR_FILENO) < 0) {
      _exit(126);
    }
    if (stdin_fd > STDERR_FILENO) {
      (void)::close(stdin_fd);
    }
    if (stdout_fd > STDERR_FILENO) {
      (void)::close(stdout_fd);
    }
    if (stderr_fd > STDERR_FILENO && stderr_fd != stdout_fd) {
      (void)::close(stderr_fd);
    }

    if (command_has_path_separator) {
      ::execv(argv[0].c_str(), exec_argv.data());
    }
    else {
      ::execvp(argv[0].c_str(), exec_argv.data());
    }
    const int exec_errno = errno;
    const char message[] = "styio: failed to exec native command\n";
    (void)::write(STDERR_FILENO, message, sizeof(message) - 1);
    _exit(exec_errno == ENOENT ? 127 : 126);
  }

  close_fds(stdin_fd, stdout_fd, stderr_fd);

  int status = 0;
  pid_t waited = 0;
  do {
    waited = ::waitpid(pid, &status, 0);
  } while (waited < 0 && errno == EINTR);

  if (waited < 0) {
    return ProcessResult{
      127,
      std::string("cannot wait for native command: ") + std::strerror(errno)};
  }
  if (WIFEXITED(status)) {
    return ProcessResult{WEXITSTATUS(status), ""};
  }
  if (WIFSIGNALED(status)) {
    return ProcessResult{
      128 + WTERMSIG(status),
      "native command terminated by signal " + std::to_string(WTERMSIG(status))};
  }
  return ProcessResult{127, "native command did not exit normally"};
}
#endif

std::string
safe_prefix(std::string prefix) {
  if (prefix.empty()) {
    return "styio";
  }
  for (char& ch : prefix) {
    const unsigned char uch = static_cast<unsigned char>(ch);
    if (!std::isalnum(uch) && ch != '-' && ch != '_') {
      ch = '-';
    }
  }
  return prefix;
}

}  // namespace

DynamicLibraryHandle
load_dynamic_library(
  const std::filesystem::path& path,
  std::string& error_message
) {
#if defined(_WIN32)
  HMODULE handle = ::LoadLibraryW(path.wstring().c_str());
  if (handle == nullptr) {
    error_message = "LoadLibraryW failed: " + windows_error_message(::GetLastError());
    return nullptr;
  }
  return reinterpret_cast<DynamicLibraryHandle>(handle);
#else
  void* handle = ::dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
  if (handle == nullptr) {
    const char* err = ::dlerror();
    error_message = std::string("dlopen failed: ") + (err != nullptr ? err : "unknown dlopen error");
    return nullptr;
  }
  return handle;
#endif
}

void*
lookup_dynamic_symbol(
  DynamicLibraryHandle handle,
  const std::string& name,
  std::string& error_message
) {
  if (handle == nullptr) {
    error_message = "dynamic library handle is null";
    return nullptr;
  }
#if defined(_WIN32)
  FARPROC symbol = ::GetProcAddress(reinterpret_cast<HMODULE>(handle), name.c_str());
  if (symbol == nullptr) {
    error_message = "GetProcAddress failed: " + windows_error_message(::GetLastError());
    return nullptr;
  }
  return reinterpret_cast<void*>(symbol);
#else
  ::dlerror();
  void* symbol = ::dlsym(handle, name.c_str());
  const char* err = ::dlerror();
  if (err != nullptr || symbol == nullptr) {
    error_message = err != nullptr ? err : "dlsym returned null";
    return nullptr;
  }
  return symbol;
#endif
}

void
unload_dynamic_library(DynamicLibraryHandle handle) {
  if (handle == nullptr) {
    return;
  }
#if defined(_WIN32)
  (void)::FreeLibrary(reinterpret_cast<HMODULE>(handle));
#else
  (void)::dlclose(handle);
#endif
}

ProcessResult
run_process_to_log(
  const std::vector<std::string>& argv,
  const std::filesystem::path& log_path,
  bool capture_stdout
) {
#if defined(_WIN32)
  std::string error_message;
  HANDLE log_handle = open_inherited_file(
    log_path,
    GENERIC_WRITE,
    CREATE_ALWAYS,
    error_message);
  if (log_handle == INVALID_HANDLE_VALUE) {
    return ProcessResult{127, error_message};
  }
  HANDLE stdout_handle = log_handle;
  HANDLE owned_stdout = nullptr;
  if (!capture_stdout) {
    owned_stdout = open_inherited_null(GENERIC_WRITE);
    stdout_handle = owned_stdout != INVALID_HANDLE_VALUE ? owned_stdout : log_handle;
  }
  ProcessResult result =
    run_process_with_handles(argv, INVALID_HANDLE_VALUE, stdout_handle, log_handle);
  if (owned_stdout != nullptr && owned_stdout != INVALID_HANDLE_VALUE) {
    ::CloseHandle(owned_stdout);
  }
  ::CloseHandle(log_handle);
  return result;
#else
  const int log_fd = ::open(log_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
  if (log_fd < 0) {
    return ProcessResult{
      127,
      "cannot open native command log `" + log_path.string() + "`: " + std::strerror(errno)};
  }
  return run_process_with_fds(argv, -1, capture_stdout ? log_fd : -1, log_fd);
#endif
}

ProcessResult
run_process_to_logs(
  const std::vector<std::string>& argv,
  const std::filesystem::path& stdout_log_path,
  const std::filesystem::path& stderr_log_path
) {
#if defined(_WIN32)
  std::string error_message;
  HANDLE stdout_handle = open_inherited_file(
    stdout_log_path,
    GENERIC_WRITE,
    CREATE_ALWAYS,
    error_message);
  if (stdout_handle == INVALID_HANDLE_VALUE) {
    return ProcessResult{127, error_message};
  }
  HANDLE stderr_handle = open_inherited_file(
    stderr_log_path,
    GENERIC_WRITE,
    CREATE_ALWAYS,
    error_message);
  if (stderr_handle == INVALID_HANDLE_VALUE) {
    ::CloseHandle(stdout_handle);
    return ProcessResult{127, error_message};
  }
  ProcessResult result =
    run_process_with_handles(argv, INVALID_HANDLE_VALUE, stdout_handle, stderr_handle);
  ::CloseHandle(stdout_handle);
  ::CloseHandle(stderr_handle);
  return result;
#else
  const int stdout_fd = ::open(stdout_log_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
  if (stdout_fd < 0) {
    return ProcessResult{
      127,
      "cannot open native command stdout log `" + stdout_log_path.string() + "`: "
        + std::strerror(errno)};
  }
  const int stderr_fd = ::open(stderr_log_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
  if (stderr_fd < 0) {
    close_fd(stdout_fd);
    return ProcessResult{
      127,
      "cannot open native command stderr log `" + stderr_log_path.string() + "`: "
        + std::strerror(errno)};
  }
  return run_process_with_fds(argv, -1, stdout_fd, stderr_fd);
#endif
}

ProcessResult
run_process_to_logs(
  const std::vector<std::string>& argv,
  const std::filesystem::path& stdin_path,
  const std::filesystem::path& stdout_log_path,
  const std::filesystem::path& stderr_log_path
) {
#if defined(_WIN32)
  std::string error_message;
  HANDLE stdin_handle = open_inherited_file(
    stdin_path,
    GENERIC_READ,
    OPEN_EXISTING,
    error_message);
  if (stdin_handle == INVALID_HANDLE_VALUE) {
    return ProcessResult{127, error_message};
  }
  HANDLE stdout_handle = open_inherited_file(
    stdout_log_path,
    GENERIC_WRITE,
    CREATE_ALWAYS,
    error_message);
  if (stdout_handle == INVALID_HANDLE_VALUE) {
    ::CloseHandle(stdin_handle);
    return ProcessResult{127, error_message};
  }
  HANDLE stderr_handle = open_inherited_file(
    stderr_log_path,
    GENERIC_WRITE,
    CREATE_ALWAYS,
    error_message);
  if (stderr_handle == INVALID_HANDLE_VALUE) {
    ::CloseHandle(stdin_handle);
    ::CloseHandle(stdout_handle);
    return ProcessResult{127, error_message};
  }
  ProcessResult result =
    run_process_with_handles(argv, stdin_handle, stdout_handle, stderr_handle);
  ::CloseHandle(stdin_handle);
  ::CloseHandle(stdout_handle);
  ::CloseHandle(stderr_handle);
  return result;
#else
  const int stdin_fd = ::open(stdin_path.c_str(), O_RDONLY);
  if (stdin_fd < 0) {
    return ProcessResult{
      127,
      "cannot open native command stdin `" + stdin_path.string() + "`: "
        + std::strerror(errno)};
  }
  const int stdout_fd = ::open(stdout_log_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
  if (stdout_fd < 0) {
    close_fd(stdin_fd);
    return ProcessResult{
      127,
      "cannot open native command stdout log `" + stdout_log_path.string() + "`: "
        + std::strerror(errno)};
  }
  const int stderr_fd = ::open(stderr_log_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
  if (stderr_fd < 0) {
    close_fd(stdin_fd);
    close_fd(stdout_fd);
    return ProcessResult{
      127,
      "cannot open native command stderr log `" + stderr_log_path.string() + "`: "
        + std::strerror(errno)};
  }
  return run_process_with_fds(argv, stdin_fd, stdout_fd, stderr_fd);
#endif
}

std::filesystem::path
create_temp_directory(
  const std::string& prefix,
  std::string& error_message
) {
  std::error_code ec;
  const std::filesystem::path base = std::filesystem::temp_directory_path(ec);
  if (ec) {
    error_message = "cannot resolve temporary directory: " + ec.message();
    return {};
  }

  const std::string clean_prefix = safe_prefix(prefix);
  const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
  for (int attempt = 0; attempt < 256; ++attempt) {
    const std::filesystem::path candidate =
      base
      / (clean_prefix + "-"
         + std::to_string(static_cast<unsigned long long>(process_id()))
         + "-"
         + std::to_string(static_cast<long long>(now))
         + "-"
         + std::to_string(attempt));
    ec.clear();
    if (std::filesystem::create_directory(candidate, ec)) {
      return candidate;
    }
    std::error_code exists_ec;
    const bool already_exists = std::filesystem::exists(candidate, exists_ec);
    if (ec && !already_exists) {
      error_message = "cannot create temporary directory: "
        + candidate.string() + ": " + ec.message();
      return {};
    }
  }

  error_message = "cannot allocate a unique temporary directory";
  return {};
}

std::filesystem::path
current_executable_path() {
#if defined(_WIN32)
  std::wstring buffer(1024, L'\0');
  for (;;) {
    const DWORD len = ::GetModuleFileNameW(
      nullptr,
      buffer.data(),
      static_cast<DWORD>(buffer.size()));
    if (len == 0) {
      return {};
    }
    if (len < buffer.size() - 1) {
      buffer.resize(len);
      return std::filesystem::path(buffer);
    }
    buffer.resize(buffer.size() * 2);
  }
#elif defined(__linux__)
  std::string buffer(4096, '\0');
  const ssize_t len = ::readlink("/proc/self/exe", buffer.data(), buffer.size() - 1);
  if (len > 0) {
    buffer.resize(static_cast<size_t>(len));
    return std::filesystem::path(buffer);
  }
  return {};
#elif defined(__APPLE__)
  uint32_t required_size = 0;
  if (::_NSGetExecutablePath(nullptr, &required_size) != -1 || required_size == 0) {
    return {};
  }

  std::vector<char> buffer(required_size);
  for (;;) {
    uint32_t buffer_size = static_cast<uint32_t>(buffer.size());
    if (::_NSGetExecutablePath(buffer.data(), &buffer_size) == 0) {
      std::filesystem::path path(buffer.data());
      std::error_code ec;
      if (path.is_relative()) {
        path = std::filesystem::absolute(path, ec);
        if (ec) {
          return {};
        }
      }

      const std::filesystem::path canonical =
        std::filesystem::weakly_canonical(path, ec);
      return ec ? path.lexically_normal() : canonical;
    }
    if (buffer_size <= buffer.size()) {
      return {};
    }
    buffer.resize(buffer_size);
  }
#else
  return {};
#endif
}

std::filesystem::path
current_executable_dir() {
  const std::filesystem::path path = current_executable_path();
  return path.empty() ? std::filesystem::path() : path.parent_path();
}

const char*
executable_suffix() {
#if defined(_WIN32)
  return ".exe";
#else
  return "";
#endif
}

const char*
shared_library_prefix() {
#if defined(_WIN32)
  return "";
#else
  return "lib";
#endif
}

const char*
shared_library_suffix() {
#if defined(_WIN32)
  return ".dll";
#elif defined(__APPLE__)
  return ".dylib";
#else
  return ".so";
#endif
}

const char*
object_suffix() {
#if defined(_WIN32)
  return ".obj";
#else
  return ".o";
#endif
}

char
path_list_separator() {
#if defined(_WIN32)
  return ';';
#else
  return ':';
#endif
}

bool
is_executable_file(const std::filesystem::path& path) {
  std::error_code ec;
  const auto status = std::filesystem::status(path, ec);
  if (ec || !std::filesystem::is_regular_file(status)) {
    return false;
  }
#if defined(_WIN32)
  return true;
#else
  const auto perms = status.permissions();
  return (perms & std::filesystem::perms::owner_exec) != std::filesystem::perms::none
    || (perms & std::filesystem::perms::group_exec) != std::filesystem::perms::none
    || (perms & std::filesystem::perms::others_exec) != std::filesystem::perms::none;
#endif
}

std::vector<std::filesystem::path>
executable_name_candidates(const std::filesystem::path& path) {
  std::vector<std::filesystem::path> candidates;
#if defined(_WIN32)
  if (path.extension().empty()) {
    candidates.push_back(path.string() + executable_suffix());
  }
#endif
  candidates.push_back(path);
  return candidates;
}

bool
find_executable(const std::string& name, std::string& out_command) {
  if (name.empty()) {
    return false;
  }

  const std::filesystem::path direct(name);
  if (direct.has_parent_path() || direct.is_absolute()) {
    for (const auto& candidate : executable_name_candidates(direct)) {
      if (is_executable_file(candidate)) {
        out_command = candidate.string();
        return true;
      }
    }
    return false;
  }

  const char* path_env = std::getenv("PATH");
  if (path_env == nullptr || path_env[0] == '\0') {
    return false;
  }
  std::stringstream paths(path_env);
  std::string dir;
  while (std::getline(paths, dir, path_list_separator())) {
    const std::filesystem::path base =
      dir.empty() ? std::filesystem::path(".") : std::filesystem::path(dir);
    for (const auto& candidate : executable_name_candidates(base / name)) {
      if (is_executable_file(candidate)) {
        out_command = candidate.string();
        return true;
      }
    }
  }
  return false;
}

std::uint64_t
process_id() {
#if defined(_WIN32)
  return static_cast<std::uint64_t>(::GetCurrentProcessId());
#else
  return static_cast<std::uint64_t>(::getpid());
#endif
}

}  // namespace styio::platform
