#ifndef STYIO_EXTERN_LIB_H
#define STYIO_EXTERN_LIB_H

#ifdef _WIN32
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

#include <cstdio>
#include <cstdlib>
#include <iostream>

// /// putchard - putchar that takes a double and returns 0.
// extern "C" DLLEXPORT double putchard(double X) {
//   std::fputc((char)X, stderr);
//   return 0;
// }

// /// printd - printf that takes a double prints it as "%f\n", returning 0.
// extern "C" DLLEXPORT double printd(double X) {
//   std::fprintf(stderr, "%f\n", X);
//   return 0;
// }

// extern "C" DLLEXPORT int where_are_we() {
//   std::cout << "We are in C++." << std::endl;
//   return 0;
// }

extern "C" int something();

#endif // STYIO_EXTERN_LIB_H