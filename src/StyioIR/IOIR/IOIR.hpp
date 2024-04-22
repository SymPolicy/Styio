#pragma once
#ifndef STYIO_IO_IR_H_
#define STYIO_IO_IR_H_

class SIOPath : public StyioIRTraits<SIOPath>
{
public:
  std::string path;

  SIOPath(std::string path) :
      path(path) {
  }

  static SIOPath* Create(std::string path) {
    return new SIOPath(path);
  }
};

class SIOPrint : public StyioIRTraits<SIOPrint>
{
public:
  std::vector<StyioIR*> expr;

  SIOPrint(std::vector<StyioIR*> expr) :
      expr(expr) {
  }

  static SIOPrint* Create(std::vector<StyioIR*> expr) {
    return new SIOPrint(expr);
  }
};

class SIORead : public StyioIRTraits<SIORead>
{
public:
  SIOPath* file_path;

  SIORead(SIOPath* file_path) :
      file_path(file_path) {
  }

  static SIORead* Create(SIOPath* file_path) {
    return new SIORead(file_path);
  }
};

#endif