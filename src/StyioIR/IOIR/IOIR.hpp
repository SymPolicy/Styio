class SIOPath : StyioIRTraits<SIOPath>
{
private:
  SIOPath(std::string path) :
      path(path) {
  }

public:
  std::string path;

  static SIOPath* Create(std::string path) {
    return new SIOPath(path);
  }
};

class SIOPrint : StyioIRTraits<SIOPrint>
{
private:
  SIOPrint(std::vector<StyioIR*> expr) :
      expr(expr) {
  }

public:
  std::vector<StyioIR*> expr;

  static SIOPrint* Create(std::vector<StyioIR*> expr) {
    return new SIOPrint(expr);
  }
};

class SIORead : StyioIRTraits<SIORead>
{
private:
  SIORead(SIOPath* file_path) :
      file_path(file_path) {
  }

public:
  SIOPath* file_path;

  static SIORead* Create(SIOPath* file_path) {
    return new SIORead(file_path);
  }
};