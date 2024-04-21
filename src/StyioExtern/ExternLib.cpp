#include <iostream>

extern "C" int something() {
  std::cout << "link with something()" << std::endl;
  return 0;
}