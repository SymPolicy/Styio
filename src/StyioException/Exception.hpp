#pragma once
#ifndef STYIO_EXCEPTION_H_
#define STYIO_EXCEPTION_H_

#include <cstddef>
#include <cstdint>
#include <exception>
#include <string>

#include "../StyioServices/DiagnosticContract.hpp"

class StyioBaseException : public std::exception
{
private:
  std::string message;
  styio::services::diagnostics::DiagnosticCode code_ =
    styio::services::diagnostics::DiagnosticCode::InternalError;
  std::size_t offset_ = static_cast<std::size_t>(-1);
  std::size_t length_ = 0;

public:
  StyioBaseException() :
      message("Styio.BaseException: Undefined.") {}

  StyioBaseException(std::string msg) :
      message("Styio.BaseException: " + msg) {}

  StyioBaseException(
    std::string msg,
    styio::services::diagnostics::DiagnosticCode code,
    std::size_t offset = static_cast<std::size_t>(-1),
    std::size_t length = 0
  ) :
      message("Styio.BaseException: " + msg),
      code_(code),
      offset_(offset),
      length_(length) {}

  ~StyioBaseException() throw() {}

  virtual const char* what() const throw() {
    return message.c_str();
  }

  styio::services::diagnostics::DiagnosticCode diagnostic_code() const noexcept {
    return code_;
  }

  std::size_t error_offset() const noexcept { return offset_; }
  std::size_t error_length() const noexcept { return length_; }

  void set_diagnostic_code(styio::services::diagnostics::DiagnosticCode c) noexcept {
    code_ = c;
  }
  void set_error_offset(std::size_t o) noexcept { offset_ = o; }
  void set_error_length(std::size_t l) noexcept { length_ = l; }
};

class StyioSyntaxError : public StyioBaseException
{
private:
  std::string message;

public:
  StyioSyntaxError() :
      message("\nStyio.SyntaxError: Undefined.") {}

  StyioSyntaxError(
    std::string meta_info,
    std::string msg
  ) :
      message("\n" + meta_info + "\nStyio.SyntaxError:\n" + msg + "\n") {}

  StyioSyntaxError(
    std::string meta_info,
    std::string msg,
    styio::services::diagnostics::DiagnosticCode code,
    std::size_t offset = static_cast<std::size_t>(-1),
    std::size_t length = 0
  ) :
      StyioBaseException("", code, offset, length),
      message("\n" + meta_info + "\nStyio.SyntaxError:\n" + msg + "\n") {}

  StyioSyntaxError(std::string msg) :
      message("\nStyio.SyntaxError:\n" + msg) {}

  ~StyioSyntaxError() throw() {}

  virtual const char* what() const throw() {
    return message.c_str();
  }
};

class StyioParserResourceLimitError : public StyioSyntaxError
{
public:
  explicit StyioParserResourceLimitError(std::string msg) :
      StyioSyntaxError(msg) {}

  StyioParserResourceLimitError(
    std::string meta_info,
    std::string msg
  ) :
      StyioSyntaxError(meta_info, msg) {}

  ~StyioParserResourceLimitError() throw() {}
};

class StyioParseError : public StyioBaseException
{
private:
  std::string message;

public:
  StyioParseError() :
      message("\nStyio.ParseError: Undefined.") {}

  StyioParseError(std::string msg) :
      message("\nStyio.ParseError:\n" + msg) {}

  StyioParseError(
    std::string msg,
    styio::services::diagnostics::DiagnosticCode code,
    std::size_t offset = static_cast<std::size_t>(-1),
    std::size_t length = 0
  ) :
      StyioBaseException("", code, offset, length),
      message("\nStyio.ParseError:\n" + msg) {}

  ~StyioParseError() throw() {}

  virtual const char* what() const throw() {
    return message.c_str();
  }
};

class StyioLexError : public StyioBaseException
{
private:
  std::string message;

public:
  StyioLexError() :
      message("\nStyio.LexError: Undefined.") {}

  StyioLexError(std::string msg) :
      message("\nStyio.LexError:\n" + msg) {}

  StyioLexError(
    std::string msg,
    styio::services::diagnostics::DiagnosticCode code,
    std::size_t offset = static_cast<std::size_t>(-1),
    std::size_t length = 0
  ) :
      StyioBaseException("", code, offset, length),
      message("\nStyio.LexError:\n" + msg) {}

  ~StyioLexError() throw() {}

  virtual const char* what() const throw() {
    return message.c_str();
  }
};

class StyioTypeError : public StyioBaseException
{
private:
  std::string message;

public:
  StyioTypeError() :
      message("\nStyio.TypeError: Undefined.") {}

  StyioTypeError(std::string msg) :
      message("\nStyio.TypeError:\n" + msg) {}

  StyioTypeError(
    std::string msg,
    styio::services::diagnostics::DiagnosticCode code,
    std::size_t offset = static_cast<std::size_t>(-1),
    std::size_t length = 0
  ) :
      StyioBaseException("", code, offset, length),
      message("\nStyio.TypeError:\n" + msg) {}

  ~StyioTypeError() throw() {}

  virtual const char* what() const throw() {
    return message.c_str();
  }
};

class StyioNotImplemented : public StyioBaseException
{
private:
  std::string message;

public:
  StyioNotImplemented() :
      message("\nStyio.NotImplemented: Undefined.") {}

  StyioNotImplemented(std::string msg) :
      message("\nStyio.NotImplemented:\n" + msg) {}

  ~StyioNotImplemented() throw() {}

  virtual const char* what() const throw() {
    return message.c_str();
  }
};

class StyioUndefinedBehaviour : public StyioBaseException
{
private:
  std::string message;

public:
  StyioUndefinedBehaviour() :
      message("\nStyio.UndefinedBehaviour: Undefined.") {}

  StyioUndefinedBehaviour(std::string msg) :
      message("\nStyio.UndefinedBehaviour: " + msg) {}

  StyioUndefinedBehaviour(char msg[]) :
      message(std::string("\nStyio.UndefinedBehaviour: ") + std::string(msg)) {}

  ~StyioUndefinedBehaviour() throw() {}

  virtual const char* what() const throw() {
    return message.c_str();
  }
};

#endif
