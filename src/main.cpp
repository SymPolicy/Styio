#include <type_traits>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>
#include <list>
#include <map>
#include <unordered_map>

#include "include/StyioException/Exception.hpp"
#include "include/StyioToken/Token.hpp"
#include "include/StyioUtil/Util.hpp"
#include "include/StyioAST/AST.hpp"
#include "include/StyioParser/Parser.hpp"

int main() {
  parse_program();
  
  return 0;
}