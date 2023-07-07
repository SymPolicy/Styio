#ifndef STYIO_EXCEPTION_H_
#define STYIO_EXCEPTION_H_

class StyioBaseException : public std::exception
{
  private:
    std::string message;

  public:
    StyioBaseException() : message("Styio.BaseException: Undefined."){}

    StyioBaseException(std::string msg) : message("Styio.BaseException: " + msg) {}

    ~StyioBaseException() throw () {}

    virtual const char* what() const throw () {
      return message.c_str();
    }
};

class StyioSyntaxError : public StyioBaseException 
{
  private:
    std::string message;

  public:
    StyioSyntaxError() : message("Styio.SyntaxError: Undefined."){}

    StyioSyntaxError(std::string msg) : message("Styio.SyntaxError: " + msg) {}

    ~StyioSyntaxError() throw () {}

    virtual const char* what() const throw () {
      return message.c_str();
    }
};

class StyioNotImplemented : public StyioBaseException 
{
  private:
    std::string message;

  public:
    StyioNotImplemented() : message("Styio.NotImplemented: Undefined."){}

    StyioNotImplemented(std::string msg) : message("Styio.NotImplemented: " + msg) {}

    ~StyioNotImplemented() throw () {}

    virtual const char* what() const throw () {
      return message.c_str();
    }
};

#endif