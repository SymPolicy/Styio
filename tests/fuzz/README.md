# Styio Fuzz Targets

## Build

```bash
cmake -S . -B build -DSTYIO_ENABLE_FUZZ=ON
cmake --build build --target styio_fuzz_lexer styio_fuzz_parser
```

## Smoke Run

```bash
ctest --test-dir build -L fuzz_smoke --output-on-failure
```

## Manual Run

```bash
./build/bin/styio_fuzz_lexer tests/fuzz/corpus/lexer -runs=10000
./build/bin/styio_fuzz_parser tests/fuzz/corpus/parser -runs=10000
```
