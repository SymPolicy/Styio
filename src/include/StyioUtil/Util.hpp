#pragma once
#ifndef STYIO_UTILITY_H_
#define STYIO_UTILITY_H_

inline std::string make_padding(int indent, std::string endswith = "")
{
  return std::string("|") 
    + std::string(2 * indent, '-') 
    + std::string("|")  
    + endswith;
}

#endif