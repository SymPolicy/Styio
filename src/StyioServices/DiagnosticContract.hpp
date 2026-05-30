#pragma once

#ifndef STYIO_SERVICES_DIAGNOSTIC_CONTRACT_HPP_
#define STYIO_SERVICES_DIAGNOSTIC_CONTRACT_HPP_

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

namespace styio::services::diagnostics {

inline constexpr std::string_view kPhaseLex = "lex";
inline constexpr std::string_view kPhaseParse = "parse";
inline constexpr std::string_view kPhaseSema = "sema";
inline constexpr std::string_view kPhaseType = "type";
inline constexpr std::string_view kPhaseLowering = "lowering";
inline constexpr std::string_view kPhaseIrVerify = "ir_verify";
inline constexpr std::string_view kPhaseCodegen = "codegen";
inline constexpr std::string_view kPhaseRuntime = "runtime";
inline constexpr std::string_view kPhaseNativeInterop = "native_interop";
inline constexpr std::string_view kPhaseService = "service";

inline constexpr std::string_view kLexInvalidToken = "STYIO_LEX_INVALID_TOKEN";
inline constexpr std::string_view kLexUnterminatedString = "STYIO_LEX_UNTERMINATED_STRING";
inline constexpr std::string_view kLexUnterminatedBlockComment = "STYIO_LEX_UNTERMINATED_BLOCK_COMMENT";

inline constexpr std::string_view kParseUnexpectedToken = "STYIO_PARSE_UNEXPECTED_TOKEN";
inline constexpr std::string_view kParseUnsupportedSyntax = "STYIO_PARSE_UNSUPPORTED_SYNTAX";
inline constexpr std::string_view kParseShadowMismatch = "STYIO_PARSE_SHADOW_MISMATCH";

inline constexpr std::string_view kSemaImmutableBinding = "STYIO_SEMA_IMMUTABLE_BINDING";
inline constexpr std::string_view kSemaUndeclaredSymbol = "STYIO_SEMA_UNDECLARED_SYMBOL";
inline constexpr std::string_view kSemaCallArityMismatch =
  "STYIO_SEMA_CALL_ARITY_MISMATCH";
inline constexpr std::string_view kSemaResourceCapabilityMismatch =
  "STYIO_SEMA_RESOURCE_CAPABILITY_MISMATCH";
inline constexpr std::string_view kSemaResourcePressureObserverUnsupported =
  "STYIO_SEMA_RESOURCE_PRESSURE_OBSERVER_UNSUPPORTED";
inline constexpr std::string_view kSemaResourceMethodUnsupportedBody =
  "STYIO_SEMA_RESOURCE_METHOD_UNSUPPORTED_BODY";
inline constexpr std::string_view kTypeError = "STYIO_TYPE_ERROR";
inline constexpr std::string_view kTypeResourceEffectFallbackMismatch =
  "STYIO_TYPE_RESOURCE_EFFECT_FALLBACK_MISMATCH";
inline constexpr std::string_view kTypeCallArgumentMismatch =
  "STYIO_TYPE_CALL_ARGUMENT_MISMATCH";
inline constexpr std::string_view kTypeUnsupportedTupleReturn =
  "STYIO_TYPE_UNSUPPORTED_TUPLE_RETURN";
inline constexpr std::string_view kTypeStreamHashTagRouteUnsupported =
  "STYIO_TYPE_STREAM_HASH_TAG_ROUTE_UNSUPPORTED";
inline constexpr std::string_view kTypeStreamZipUnsupportedSource =
  "STYIO_TYPE_STREAM_ZIP_UNSUPPORTED_SOURCE";
inline constexpr std::string_view kLowerUnsupportedAst = "STYIO_LOWER_UNSUPPORTED_AST";
inline constexpr std::string_view kCodegenError = "STYIO_CODEGEN_ERROR";
inline constexpr std::string_view kRuntimeError = "STYIO_RUNTIME_ERROR";
inline constexpr std::string_view kNativeUnsupportedAbi = "STYIO_NATIVE_UNSUPPORTED_ABI";
inline constexpr std::string_view kNativeSourceReadFailed = "STYIO_NATIVE_SOURCE_READ_FAILED";
inline constexpr std::string_view kNativeSignatureNotFound = "STYIO_NATIVE_SIGNATURE_NOT_FOUND";
inline constexpr std::string_view kNativeUnsupportedSignature = "STYIO_NATIVE_UNSUPPORTED_SIGNATURE";
inline constexpr std::string_view kNativeHostCompileFailed = "STYIO_NATIVE_HOST_COMPILE_FAILED";
inline constexpr std::string_view kNativeLoadFailed = "STYIO_NATIVE_LOAD_FAILED";
inline constexpr std::string_view kNativeSymbolMissing = "STYIO_NATIVE_SYMBOL_MISSING";
inline constexpr std::string_view kNativeToolchainUnavailable = "STYIO_NATIVE_TOOLCHAIN_UNAVAILABLE";
inline constexpr std::string_view kNativeInteropError = "STYIO_NATIVE_INTEROP_ERROR";

inline constexpr std::string_view kServiceInvalidArgument = "STYIO_SERVICE_INVALID_ARGUMENT";
inline constexpr std::string_view kServiceReadFailed = "STYIO_SERVICE_READ_FAILED";
inline constexpr std::string_view kServiceCompilePlanInvalid = "STYIO_SERVICE_COMPILE_PLAN_INVALID";
inline constexpr std::string_view kServiceCompilePlanCliConflict = "STYIO_SERVICE_COMPILE_PLAN_CLI_CONFLICT";
inline constexpr std::string_view kServiceEditorSyntax = "STYIO_SERVICE_EDITOR_SYNTAX";
inline constexpr std::string_view kServiceLspResyncRequired = "STYIO_SERVICE_LSP_RESYNC_REQUIRED";

inline bool
contains(std::string_view text, std::string_view needle) {
  return text.find(needle) != std::string_view::npos;
}

inline bool
starts_with(std::string_view text, std::string_view prefix) {
  return text.rfind(prefix, 0) == 0;
}

inline std::string
to_upper_ascii(std::string value) {
  std::transform(
    value.begin(),
    value.end(),
    value.begin(),
    [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
  return value;
}

inline std::string
diagnostic_phase_for_code(std::string_view code) {
  if (starts_with(code, "STYIO_LEX_")) {
    return std::string(kPhaseLex);
  }
  if (starts_with(code, "STYIO_PARSE_")) {
    return std::string(kPhaseParse);
  }
  if (starts_with(code, "STYIO_SEMA_")) {
    return std::string(kPhaseSema);
  }
  if (starts_with(code, "STYIO_TYPE_")) {
    return std::string(kPhaseType);
  }
  if (starts_with(code, "STYIO_LOWER_")) {
    return std::string(kPhaseLowering);
  }
  if (starts_with(code, "STYIO_IR_VERIFY_")) {
    return std::string(kPhaseIrVerify);
  }
  if (starts_with(code, "STYIO_CODEGEN_")) {
    return std::string(kPhaseCodegen);
  }
  if (starts_with(code, "STYIO_RUNTIME_")) {
    return std::string(kPhaseRuntime);
  }
  if (starts_with(code, "STYIO_NATIVE_")) {
    return std::string(kPhaseNativeInterop);
  }
  return std::string(kPhaseService);
}

inline std::string
classify_lex_code(std::string_view message) {
  if (contains(message, "unterminated string")) {
    return std::string(kLexUnterminatedString);
  }
  if (contains(message, "unterminated block comment") || contains(message, "Unterminated block comment")) {
    return std::string(kLexUnterminatedBlockComment);
  }
  return std::string(kLexInvalidToken);
}

inline std::string
classify_parse_code(std::string_view message) {
  if (contains(message, "unsupported syntax") || contains(message, "not supported")) {
    return std::string(kParseUnsupportedSyntax);
  }
  if (contains(message, "shadow parser")) {
    return std::string(kParseShadowMismatch);
  }
  return std::string(kParseUnexpectedToken);
}

inline std::string
classify_service_code(std::string_view subcode, std::string_view message) {
  if (subcode == "compile_plan_cli_conflict") {
    return std::string(kServiceCompilePlanCliConflict);
  }
  if (subcode == "compile_plan_invalid") {
    return std::string(kServiceCompilePlanInvalid);
  }
  if (contains(message, "cannot open file") || contains(message, "file not found")
      || contains(message, "failed to read file")) {
    return std::string(kServiceReadFailed);
  }
  return std::string(kServiceInvalidArgument);
}

inline bool
looks_like_native_interop_message(std::string_view message) {
  return contains(message, "native") || contains(message, "@extern")
      || contains(message, "toolchain") || contains(message, "STYIO_NATIVE");
}

inline std::string
classify_native_interop_code(std::string_view message) {
  if (contains(message, "unsupported @extern ABI")) {
    return std::string(kNativeUnsupportedAbi);
  }
  if (contains(message, "native @extern source file not found or unreadable")) {
    return std::string(kNativeSourceReadFailed);
  }
  if (contains(message, "@extern binding does not declare native function")
      || (contains(message, "@extern(") && contains(message, "block does not declare any callable function"))
      || contains(message, "@export does not match any @extern")) {
    return std::string(kNativeSignatureNotFound);
  }
  if (contains(message, "unsupported native parameter type")
      || contains(message, "unsupported native return type")
      || contains(message, "variadic native functions are not supported")
      || contains(message, "empty native parameter in @extern block")
      || contains(message, "cannot parse native parameter")
      || (contains(message, "native parameter `") && contains(message, "cannot have type void"))) {
    return std::string(kNativeUnsupportedSignature);
  }
  if ((contains(message, "native @extern(") && contains(message, "compile failed with command"))
      || contains(message, "native @extern artifact compile failed")) {
    return std::string(kNativeHostCompileFailed);
  }
  if (contains(message, "native @extern(") && contains(message, "dlopen failed")) {
    return std::string(kNativeLoadFailed);
  }
  if (contains(message, "could not resolve exported symbol") || contains(message, "has no loaded address")) {
    return std::string(kNativeSymbolMissing);
  }
  if (contains(message, "requires a bundled clang toolchain")
      || contains(message, "invalid STYIO_NATIVE_TOOLCHAIN_MODE")) {
    return std::string(kNativeToolchainUnavailable);
  }
  return std::string(kNativeInteropError);
}

inline std::string
classify_type_or_lowering_code(std::string_view message) {
  if (contains(message, "Unsupported AST")
      || contains(message, "unsupported AST")
      || contains(message, "unsupported node")) {
    return std::string(kLowerUnsupportedAst);
  }
  if (contains(message, "compound assignment requires a mutable binding")
      || contains(message, "immutable binding cannot be reassigned")
      || contains(message, "immutable binding cannot be redefined")
      || contains(message, "parallel assignment cannot rebind final slot")
      || (contains(message, "task pull target") && contains(message, "is final and cannot be reassigned"))
      || contains(message, "resource clone cannot rebind final slot")
      || contains(message, "final resource bind cannot redefine")
      || (contains(message, "flow bind target") && contains(message, "is final and cannot be reassigned"))
      || contains(message, "is final and cannot be overridden")) {
    return std::string(kSemaImmutableBinding);
  }
  if (contains(message, "unknown function `")
      || contains(message, "unknown resource `")
      || contains(message, "unknown resource: @")) {
    return std::string(kSemaUndeclaredSymbol);
  }
  if ((contains(message, "function `") || contains(message, "resource method @"))
      && contains(message, "expects") && contains(message, "argument(s), got")) {
    return std::string(kSemaCallArityMismatch);
  }
  if (contains(message, "must be a writable resource")
      || contains(message, "must have push capability")
      || contains(message, "must have pull capability")
      || contains(message, "must have iter capability")
      || contains(message, "does not have read capability")
      || contains(message, "is not indexable")
      || contains(message, "does not support snapshot selection")
      || contains(message, "read-only stream; cannot")
      || contains(message, "write-only stream; cannot")) {
    return std::string(kSemaResourceCapabilityMismatch);
  }
  if (contains(message, "does not expose pressure stream")
      || contains(message, "pressure observer requires a resource family")) {
    return std::string(kSemaResourcePressureObserverUnsupported);
  }
  if (contains(message, "resource method return currently requires")) {
    return std::string(kSemaResourceMethodUnsupportedBody);
  }
  if (contains(message, "resource-effect fallback expects")) {
    return std::string(kTypeResourceEffectFallbackMismatch);
  }
  if (contains(message, "function argument type mismatch for parameter '")
      || contains(message, "resource method argument type mismatch for parameter '")) {
    return std::string(kTypeCallArgumentMismatch);
  }
  if (contains(message, "tuple function return annotations require tuple value IR")) {
    return std::string(kTypeUnsupportedTupleReturn);
  }
  if (contains(message, "iterator sequence hash-tag routing is not implemented")) {
    return std::string(kTypeStreamHashTagRouteUnsupported);
  }
  if (contains(message, "zip requires iterable inputs on both sides")) {
    return std::string(kTypeStreamZipUnsupportedSource);
  }
  if (looks_like_native_interop_message(message)) {
    return classify_native_interop_code(message);
  }
  return std::string(kTypeError);
}

inline std::string
classify_runtime_or_native_code(std::string_view subcode, std::string_view message) {
  if (starts_with(subcode, "STYIO_RUNTIME_") || starts_with(subcode, "STYIO_NATIVE_")) {
    return std::string(subcode);
  }
  if (looks_like_native_interop_message(message)) {
    return classify_native_interop_code(message);
  }
  return std::string(kRuntimeError);
}

}  // namespace styio::services::diagnostics

#endif
