# std.math

**Purpose:** Reserve the standard-library math module boundary for general numeric helpers.

**Last updated:** 2026-06-25

Status: planned. Compiler intrinsics require separate IM-D8 admission evidence.

Current matrix helper intrinsics such as `matmul`, `transpose`, `dot`, `norm`, and `mat_*` are compiler-owned implementation evidence. They do not activate `std.math`.
