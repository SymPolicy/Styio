#pragma once

#ifndef STYIO_SERVICES_DIAGNOSTIC_CONTRACT_HPP_
#define STYIO_SERVICES_DIAGNOSTIC_CONTRACT_HPP_

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <string>
#include <string_view>

namespace styio::services::diagnostics {

// ---------------------------------------------------------------------------
// Structured diagnostic codes — replaces substring-based classify_*()
// ---------------------------------------------------------------------------
enum class DiagnosticCode : std::uint16_t {
  InternalError = 0,

  // Lex
  LexInvalidToken,
  LexUnterminatedString,
  LexUnterminatedBlockComment,

  // Parse
  ParseUnexpectedToken,
  ParseUnsupportedSyntax,
  ParseShadowMismatch,

  // Sema
  SemaImmutableBinding,
  SemaUndeclaredSymbol,
  SemaCallArityMismatch,
  SemaResourceCapabilityMismatch,
  SemaResourcePressureObserverUnsupported,
  SemaResourceMethodUnsupportedBody,

  // Type
  TypeError,
  TypeResourceEffectFallbackMismatch,
  TypeCallArgumentMismatch,
  TypeMatrixLiteralInvalid,
  TypeTupleContract,
  TypeFunctionMissingReturn,
  TypeStreamHashTagRouteUnsupported,
  TypeStreamZipUnsupportedSource,
  TypeStreamDuplicateDriverUnsupported,
  TypeIterationUnsupportedSource,
  TypeStdinUnsupportedTarget,

  // Lowering
  LowerUnsupportedAst,

  // IR Verify
  IrVerifyContract,
  IrVerifyInactiveNode,

  // Codegen
  CodegenError,

  // Runtime
  RuntimeError,

  // Native Interop
  NativeUnsupportedAbi,
  NativeSourceReadFailed,
  NativeSignatureNotFound,
  NativeUnsupportedSignature,
  NativeHostCompileFailed,
  NativeLoadFailed,
  NativeSymbolMissing,
  NativeToolchainUnavailable,
  NativeInteropError,

  // Resource Topology (TASK-07)
  ResourceTopologyCycle,

  // Service
  ServiceInvalidArgument,
  ServiceReadFailed,
  ServiceCompilePlanInvalid,
  ServiceCompilePlanCliConflict,
  ServiceEditorSyntax,
  ServiceLspResyncRequired,
};

/// Map a DiagnosticCode to its phase string.
inline std::string_view phase_for_code(DiagnosticCode code) {
  auto v = static_cast<std::uint16_t>(code);
  if (v <= static_cast<std::uint16_t>(DiagnosticCode::LexUnterminatedBlockComment))
    return "lex";
  if (v <= static_cast<std::uint16_t>(DiagnosticCode::ParseShadowMismatch))
    return "parse";
  if (v <= static_cast<std::uint16_t>(DiagnosticCode::SemaResourceMethodUnsupportedBody))
    return "sema";
  if (v <= static_cast<std::uint16_t>(DiagnosticCode::TypeStdinUnsupportedTarget))
    return "type";
  if (v <= static_cast<std::uint16_t>(DiagnosticCode::LowerUnsupportedAst))
    return "lowering";
  if (v <= static_cast<std::uint16_t>(DiagnosticCode::IrVerifyInactiveNode))
    return "ir_verify";
  if (v == static_cast<std::uint16_t>(DiagnosticCode::CodegenError)) return "codegen";
  if (v == static_cast<std::uint16_t>(DiagnosticCode::RuntimeError)) return "runtime";
  if (v <= static_cast<std::uint16_t>(DiagnosticCode::NativeInteropError)
      && v >= static_cast<std::uint16_t>(DiagnosticCode::NativeUnsupportedAbi))
    return "native_interop";
  if (v == static_cast<std::uint16_t>(DiagnosticCode::ResourceTopologyCycle)) return "resource_topology";
  return "service";
}

/// String name for a DiagnosticCode (uppercase constant style).
inline std::string_view diagnostic_code_name(DiagnosticCode code);

// Keep legacy string constants for backward compatibility during migration.
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
inline constexpr std::string_view kTypeMatrixLiteralInvalid =
  "STYIO_TYPE_MATRIX_LITERAL_INVALID";
inline constexpr std::string_view kTypeTupleContract =
  "STYIO_TYPE_TUPLE_CONTRACT";
inline constexpr std::string_view kTypeFunctionMissingReturn =
  "STYIO_TYPE_FUNCTION_MISSING_RETURN";
inline constexpr std::string_view kTypeStreamHashTagRouteUnsupported =
  "STYIO_TYPE_STREAM_HASH_TAG_ROUTE_UNSUPPORTED";
inline constexpr std::string_view kTypeStreamZipUnsupportedSource =
  "STYIO_TYPE_STREAM_ZIP_UNSUPPORTED_SOURCE";
inline constexpr std::string_view kTypeStreamDuplicateDriverUnsupported =
  "STYIO_TYPE_STREAM_DUPLICATE_DRIVER_UNSUPPORTED";
inline constexpr std::string_view kTypeIterationUnsupportedSource =
  "STYIO_TYPE_ITERATION_UNSUPPORTED_SOURCE";
inline constexpr std::string_view kTypeStdinUnsupportedTarget =
  "STYIO_TYPE_STDIN_UNSUPPORTED_TARGET";
inline constexpr std::string_view kLowerUnsupportedAst = "STYIO_LOWER_UNSUPPORTED_AST";
inline constexpr std::string_view kIrVerifyContract = "STYIO_IR_VERIFY_CONTRACT";
inline constexpr std::string_view kIrVerifyInactiveNode = "STYIO_IR_VERIFY_INACTIVE_NODE";
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
  if (contains(message, "StyioIR verifier failed")
      || contains(message, "missing StyioIR root")
      || contains(message, "missing required StyioIR child")
      || contains(message, "inactive StyioIR node reached codegen boundary")) {
    if (contains(message, "inactive StyioIR node reached codegen boundary")) {
      return std::string(kIrVerifyInactiveNode);
    }
    return std::string(kIrVerifyContract);
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
  if (contains(message, "matrix binding requires a nested list literal")
      || contains(message, "matrix literal requires at least one row")
      || contains(message, "matrix rows must be list literals")
      || contains(message, "matrix rows must not be empty")
      || contains(message, "matrix rows must have consistent length")
      || contains(message, "matrix elements must be numeric scalar values")) {
    return std::string(kTypeMatrixLiteralInvalid);
  }
  if (contains(message, "tuple return")
      || contains(message, "tuple projection")
      || contains(message, "tuple parameter")
      || contains(message, "runtime tuple literal")
      || contains(message, "nested tuple element")
      || contains(message, "tuple literal element")
      || contains(message, "tuple mutation")
      || contains(message, "tuple iteration")
      || contains(message, "tuple result")
      || contains(message, "tuple function return annotation")
      || contains(message, "tuple type")) {
    return std::string(kTypeTupleContract);
  }
  if (contains(message, "function body requires a return value")) {
    return std::string(kTypeFunctionMissingReturn);
  }
  if (contains(message, "iterator sequence hash-tag routing is not implemented")) {
    return std::string(kTypeStreamHashTagRouteUnsupported);
  }
  if (contains(message, "zip requires iterable inputs on both sides")) {
    return std::string(kTypeStreamZipUnsupportedSource);
  }
  if (contains(message, "zip over @stdin on both sides requires a distinct stream-driver decision")) {
    return std::string(kTypeStreamDuplicateDriverUnsupported);
  }
  if (contains(message, "iteration requires an iterable value")) {
    return std::string(kTypeIterationUnsupportedSource);
  }
  if (contains(message, "typed stdin pull supports i64, f64, string, or list[T] targets")
      || contains(message, "typed stdin list pull supports list[i64], list[f64], or list[string]")) {
    return std::string(kTypeStdinUnsupportedTarget);
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

// ---------------------------------------------------------------------------
// diagnostic_code_name — compile-time mapping from code to string constant
// ---------------------------------------------------------------------------
inline std::string_view diagnostic_code_name(DiagnosticCode code) {
  switch (code) {
    case DiagnosticCode::InternalError: return "STYIO_INTERNAL_ERROR";
    case DiagnosticCode::LexInvalidToken: return "STYIO_LEX_INVALID_TOKEN";
    case DiagnosticCode::LexUnterminatedString: return "STYIO_LEX_UNTERMINATED_STRING";
    case DiagnosticCode::LexUnterminatedBlockComment: return "STYIO_LEX_UNTERMINATED_BLOCK_COMMENT";
    case DiagnosticCode::ParseUnexpectedToken: return "STYIO_PARSE_UNEXPECTED_TOKEN";
    case DiagnosticCode::ParseUnsupportedSyntax: return "STYIO_PARSE_UNSUPPORTED_SYNTAX";
    case DiagnosticCode::ParseShadowMismatch: return "STYIO_PARSE_SHADOW_MISMATCH";
    case DiagnosticCode::SemaImmutableBinding: return "STYIO_SEMA_IMMUTABLE_BINDING";
    case DiagnosticCode::SemaUndeclaredSymbol: return "STYIO_SEMA_UNDECLARED_SYMBOL";
    case DiagnosticCode::SemaCallArityMismatch: return "STYIO_SEMA_CALL_ARITY_MISMATCH";
    case DiagnosticCode::SemaResourceCapabilityMismatch: return "STYIO_SEMA_RESOURCE_CAPABILITY_MISMATCH";
    case DiagnosticCode::SemaResourcePressureObserverUnsupported: return "STYIO_SEMA_RESOURCE_PRESSURE_OBSERVER_UNSUPPORTED";
    case DiagnosticCode::SemaResourceMethodUnsupportedBody: return "STYIO_SEMA_RESOURCE_METHOD_UNSUPPORTED_BODY";
    case DiagnosticCode::TypeError: return "STYIO_TYPE_ERROR";
    case DiagnosticCode::TypeResourceEffectFallbackMismatch: return "STYIO_TYPE_RESOURCE_EFFECT_FALLBACK_MISMATCH";
    case DiagnosticCode::TypeCallArgumentMismatch: return "STYIO_TYPE_CALL_ARGUMENT_MISMATCH";
    case DiagnosticCode::TypeMatrixLiteralInvalid: return "STYIO_TYPE_MATRIX_LITERAL_INVALID";
    case DiagnosticCode::TypeTupleContract: return "STYIO_TYPE_TUPLE_CONTRACT";
    case DiagnosticCode::TypeFunctionMissingReturn: return "STYIO_TYPE_FUNCTION_MISSING_RETURN";
    case DiagnosticCode::TypeStreamHashTagRouteUnsupported: return "STYIO_TYPE_STREAM_HASH_TAG_ROUTE_UNSUPPORTED";
    case DiagnosticCode::TypeStreamZipUnsupportedSource: return "STYIO_TYPE_STREAM_ZIP_UNSUPPORTED_SOURCE";
    case DiagnosticCode::TypeStreamDuplicateDriverUnsupported: return "STYIO_TYPE_STREAM_DUPLICATE_DRIVER_UNSUPPORTED";
    case DiagnosticCode::TypeIterationUnsupportedSource: return "STYIO_TYPE_ITERATION_UNSUPPORTED_SOURCE";
    case DiagnosticCode::TypeStdinUnsupportedTarget: return "STYIO_TYPE_STDIN_UNSUPPORTED_TARGET";
    case DiagnosticCode::LowerUnsupportedAst: return "STYIO_LOWER_UNSUPPORTED_AST";
    case DiagnosticCode::IrVerifyContract: return "STYIO_IR_VERIFY_CONTRACT";
    case DiagnosticCode::IrVerifyInactiveNode: return "STYIO_IR_VERIFY_INACTIVE_NODE";
    case DiagnosticCode::CodegenError: return "STYIO_CODEGEN_ERROR";
    case DiagnosticCode::RuntimeError: return "STYIO_RUNTIME_ERROR";
    case DiagnosticCode::NativeUnsupportedAbi: return "STYIO_NATIVE_UNSUPPORTED_ABI";
    case DiagnosticCode::NativeSourceReadFailed: return "STYIO_NATIVE_SOURCE_READ_FAILED";
    case DiagnosticCode::NativeSignatureNotFound: return "STYIO_NATIVE_SIGNATURE_NOT_FOUND";
    case DiagnosticCode::NativeUnsupportedSignature: return "STYIO_NATIVE_UNSUPPORTED_SIGNATURE";
    case DiagnosticCode::NativeHostCompileFailed: return "STYIO_NATIVE_HOST_COMPILE_FAILED";
    case DiagnosticCode::NativeLoadFailed: return "STYIO_NATIVE_LOAD_FAILED";
    case DiagnosticCode::NativeSymbolMissing: return "STYIO_NATIVE_SYMBOL_MISSING";
    case DiagnosticCode::NativeToolchainUnavailable: return "STYIO_NATIVE_TOOLCHAIN_UNAVAILABLE";
    case DiagnosticCode::NativeInteropError: return "STYIO_NATIVE_INTEROP_ERROR";
    case DiagnosticCode::ResourceTopologyCycle: return "STYIO_RESOURCE_TOPOLOGY_CYCLE";
    case DiagnosticCode::ServiceInvalidArgument: return "STYIO_SERVICE_INVALID_ARGUMENT";
    case DiagnosticCode::ServiceReadFailed: return "STYIO_SERVICE_READ_FAILED";
    case DiagnosticCode::ServiceCompilePlanInvalid: return "STYIO_SERVICE_COMPILE_PLAN_INVALID";
    case DiagnosticCode::ServiceCompilePlanCliConflict: return "STYIO_SERVICE_COMPILE_PLAN_CLI_CONFLICT";
    case DiagnosticCode::ServiceEditorSyntax: return "STYIO_SERVICE_EDITOR_SYNTAX";
    case DiagnosticCode::ServiceLspResyncRequired: return "STYIO_SERVICE_LSP_RESYNC_REQUIRED";
  }
  return "STYIO_INTERNAL_ERROR";
}

}  // namespace styio::services::diagnostics

#endif
