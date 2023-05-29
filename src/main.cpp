#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <list>
#include <map>
#include <unordered_map>

#include "include/StyioException/Exception.hpp"
#include "include/StyioToken/Token.hpp"
#include "include/StyioAST/AST.hpp"
#include "include/StyioParser/Parser.hpp"

int main() {

  while (1) 
  {
    fprintf(stderr, "Styio/> ");

    parseProgram();
  }; 

  return 0;
}