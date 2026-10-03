#pragma once

#include <array>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <sys/wait.h>
#endif

namespace styio::util {

// These runners deliberately accept POSIX shell syntax. Never send it to cmd.exe.
struct ProcessCapture {
  std::string stdout_text;
  int raw_status = -1;
  int exit_code = -1;
};

inline std::string shell_quote(const std::string& value) {
  std::string result = "'";
  for (char ch : value) {
    result += ch == '\'' ? "'\\''" : std::string(1, ch);
  }
  return result + "'";
}

inline std::string shell_path(const std::filesystem::path& path) {
  // Git Bash accepts C:/... paths for both executables and redirections.
  const auto utf8 = path.generic_u8string();
  return shell_quote(std::string(utf8.begin(), utf8.end()));
}

// For existing command builders which supply their own surrounding double quotes.
inline std::string shell_path_contents(const std::filesystem::path& path) {
  const auto utf8 = path.generic_u8string();
  std::string result;
  for (char ch : utf8) {
    if (ch == '\\' || ch == '"' || ch == '$' || ch == '`') {
      result += '\\';
    }
    result += ch;
  }
  return result;
}

#ifdef _WIN32
namespace process_detail {

struct Handle {
  HANDLE value = nullptr;
  Handle() = default;
  explicit Handle(HANDLE handle) : value(handle) {}
  Handle(const Handle&) = delete;
  Handle& operator=(const Handle&) = delete;
  ~Handle() { reset(); }
  void reset() {
    if (value != nullptr && value != INVALID_HANDLE_VALUE) {
      CloseHandle(value);
    }
    value = nullptr;
  }
};

[[noreturn]] inline void win32_error(const char* action) {
  throw std::runtime_error(std::string(action) + " (Win32 error "
    + std::to_string(GetLastError()) + ")");
}

inline std::wstring widen(const std::string& utf8) {
  if (utf8.find('\0') != std::string::npos) {
    throw std::runtime_error("shell command contains a NUL byte");
  }
  if (utf8.empty()) {
    return {};
  }
  const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
    utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
  if (size == 0) {
    win32_error("cannot decode UTF-8 shell command");
  }
  std::wstring wide(size, L'\0');
  if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(),
      static_cast<int>(utf8.size()), wide.data(), size) == 0) {
    win32_error("cannot decode UTF-8 shell command");
  }
  return wide;
}

// Quote one Windows argv element, including backslashes preceding quotes/end.
inline std::wstring quote_argument(const std::wstring& value) {
  std::wstring result = L"\"";
  size_t slashes = 0;
  for (wchar_t ch : value) {
    if (ch == L'\\') {
      ++slashes;
      continue;
    }
    result.append(slashes * (ch == L'"' ? 2 : 1), L'\\');
    slashes = 0;
    if (ch == L'"') {
      result += L'\\';
    }
    result += ch;
  }
  result.append(slashes * 2, L'\\');
  return result + L'"';
}

inline std::wstring test_bash_path() {
  const DWORD size = GetEnvironmentVariableW(L"STYIO_TEST_BASH", nullptr, 0);
  if (size == 0) {
    throw std::runtime_error("STYIO_TEST_BASH must name the verified Git for Windows bash.exe");
  }
  std::wstring path(size, L'\0');
  const DWORD copied = GetEnvironmentVariableW(L"STYIO_TEST_BASH", path.data(), size);
  if (copied == 0 || copied >= size) {
    win32_error("cannot read STYIO_TEST_BASH");
  }
  path.resize(copied);
  if (!std::filesystem::path(path).is_absolute()
      || !std::filesystem::is_regular_file(path)) {
    throw std::runtime_error("STYIO_TEST_BASH must be an absolute path to bash.exe");
  }
  // No PATH fallback: Windows' WSL bash.exe is not a compatible substitute.
  return path;
}

inline void inherit_standard_handle(Handle& destination, DWORD id, DWORD access) {
  const HANDLE source = GetStdHandle(id);
  if (source != nullptr && source != INVALID_HANDLE_VALUE
      && DuplicateHandle(GetCurrentProcess(), source, GetCurrentProcess(),
        &destination.value, 0, TRUE, DUPLICATE_SAME_ACCESS)) {
    return;
  }
  SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
  destination.value = CreateFileW(L"NUL", access, FILE_SHARE_READ | FILE_SHARE_WRITE,
    &security, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (destination.value == INVALID_HANDLE_VALUE) {
    win32_error("cannot prepare shell standard handle");
  }
}

struct Attributes {
  std::vector<unsigned char> storage;
  LPPROC_THREAD_ATTRIBUTE_LIST list = nullptr;
  explicit Attributes(HANDLE (&handles)[3]) {
    SIZE_T size = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &size);
    storage.resize(size);
    list = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage.data());
    if (!InitializeProcThreadAttributeList(list, 1, 0, &size)) {
      win32_error("cannot initialize shell handle list");
    }
    if (!UpdateProcThreadAttribute(list, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
        handles, sizeof(handles), nullptr, nullptr)) {
      DeleteProcThreadAttributeList(list);
      win32_error("cannot restrict shell handle inheritance");
    }
  }
  ~Attributes() { DeleteProcThreadAttributeList(list); }
};

} // namespace process_detail
#endif

inline ProcessCapture capture_shell_stdout(const std::string& command) {
  ProcessCapture result;
  std::array<char, 4096> buffer{};
#ifdef _WIN32
  using namespace process_detail;
  const std::wstring bash = test_bash_path();
  std::wstring command_line = quote_argument(bash) + L" --noprofile --norc -c "
    + quote_argument(widen(command));
  SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
  Handle output_read, output_write, input, error;
  if (!CreatePipe(&output_read.value, &output_write.value, &security, 0)
      || !SetHandleInformation(output_read.value, HANDLE_FLAG_INHERIT, 0)) {
    win32_error("cannot create shell output pipe");
  }
  inherit_standard_handle(input, STD_INPUT_HANDLE, GENERIC_READ);
  inherit_standard_handle(error, STD_ERROR_HANDLE, GENERIC_WRITE);
  HANDLE inherited[] = {input.value, output_write.value, error.value};
  Attributes attributes(inherited);
  STARTUPINFOEXW startup{};
  startup.StartupInfo.cb = sizeof(startup);
  startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
  startup.StartupInfo.hStdInput = input.value;
  startup.StartupInfo.hStdOutput = output_write.value;
  startup.StartupInfo.hStdError = error.value;
  startup.lpAttributeList = attributes.list;
  PROCESS_INFORMATION process{};
  if (!CreateProcessW(bash.c_str(), command_line.data(), nullptr, nullptr, TRUE,
      EXTENDED_STARTUPINFO_PRESENT, nullptr, nullptr, &startup.StartupInfo, &process)) {
    win32_error("cannot launch STYIO_TEST_BASH");
  }
  Handle child(process.hProcess), thread(process.hThread);
  output_write.reset();
  for (;;) {
    DWORD received = 0;
    if (!ReadFile(output_read.value, buffer.data(), static_cast<DWORD>(buffer.size()),
        &received, nullptr)) {
      if (GetLastError() != ERROR_BROKEN_PIPE) {
        win32_error("cannot read shell output");
      }
      break;
    }
    if (received == 0) {
      break;
    }
    result.stdout_text.append(buffer.data(), received);
  }
  if (WaitForSingleObject(child.value, INFINITE) != WAIT_OBJECT_0) {
    win32_error("cannot wait for shell");
  }
  DWORD exit_code = 0;
  if (!GetExitCodeProcess(child.value, &exit_code)) {
    win32_error("cannot read shell exit status");
  }
  result.raw_status = result.exit_code = static_cast<int>(exit_code);
  // Match popen(..., "r") text-mode behavior for native Windows child output.
  std::string text;
  text.reserve(result.stdout_text.size());
  for (size_t i = 0; i < result.stdout_text.size(); ++i) {
    if (result.stdout_text[i] != '\r' || i + 1 == result.stdout_text.size()
        || result.stdout_text[i + 1] != '\n') {
      text += result.stdout_text[i];
    }
  }
  result.stdout_text = std::move(text);
#else
  FILE* pipe = popen(command.c_str(), "r");
  if (pipe == nullptr) {
    throw std::runtime_error("cannot start POSIX shell command");
  }
  size_t received = 0;
  while ((received = fread(buffer.data(), 1, buffer.size(), pipe)) != 0) {
    result.stdout_text.append(buffer.data(), received);
  }
  const bool failed = ferror(pipe) != 0;
  result.raw_status = pclose(pipe);
  if (failed) {
    throw std::runtime_error("cannot read POSIX shell output");
  }
  if (result.raw_status != -1) {
    result.exit_code = WIFEXITED(result.raw_status) ? WEXITSTATUS(result.raw_status)
      : WIFSIGNALED(result.raw_status) ? 128 + WTERMSIG(result.raw_status)
      : result.raw_status;
  }
#endif
  return result;
}

} // namespace styio::util
