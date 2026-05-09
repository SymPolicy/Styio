#pragma once
#ifndef STYIO_IO_INTRINSICS_H_
#define STYIO_IO_INTRINSICS_H_

#include <string>

#include "../StyioToken/Token.hpp"

inline bool
styio_stdin_list_elem_type_supported(const std::string& elem_type) {
  return elem_type == "i64"
    || elem_type == "i32"
    || elem_type == "int"
    || elem_type == "long"
    || elem_type == "f64"
    || elem_type == "f32"
    || elem_type == "float"
    || elem_type == "double"
    || elem_type == "string"
    || elem_type == "str";
}

inline const char*
styio_stdin_list_read_intrinsic_name_for_elem(const std::string& elem_type) {
  if (elem_type == "f64"
      || elem_type == "f32"
      || elem_type == "float"
      || elem_type == "double") {
    return "styio_list_f64_read_stdin";
  }
  if (elem_type == "string" || elem_type == "str") {
    return "styio_list_cstr_read_stdin";
  }
  return "styio_list_i64_read_stdin";
}

inline const char*
styio_stdin_list_read_intrinsic_name_for_type(const StyioDataType& type) {
  return styio_stdin_list_read_intrinsic_name_for_elem(styio_type_item_type_name(type));
}

#endif
