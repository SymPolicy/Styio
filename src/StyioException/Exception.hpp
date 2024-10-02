#pragma once
#ifndef STYIO_EXCEPTION_H_
#define STYIO_EXCEPTION_H_

class StyioBaseException : public std::exception
{
private:
  std::string message;

public:
  StyioBaseException() :
      message("Styio.BaseException: Undefined.") {}

  StyioBaseException(std::string msg) :
      message("Styio.BaseException: " + msg) {}

  ~StyioBaseException() throw() {}

  virtual const char* what() const throw() {
    return message.c_str();
  }
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

  StyioSyntaxError(std::string msg) :
      message("\nStyio.SyntaxError:\n" + msg) {}

  ~StyioSyntaxError() throw() {}

  virtual const char* what() const throw() {
    return message.c_str();
  }
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

  ~StyioParseError() throw() {}

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