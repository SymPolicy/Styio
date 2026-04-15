#pragma once
#ifndef STYIO_TOKENIZER_H_
#define STYIO_TOKENIZER_H_

// [C++ STL]
#include <string>
#include <vector>

// [Styio]
#include "../StyioToken/Token.hpp"

class StyioTokenizer
{
public:
  static std::vector<StyioToken *> tokenize(std::string code);
};

#endif