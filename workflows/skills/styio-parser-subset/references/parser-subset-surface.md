# Parser Subset Surface

**Purpose:** Map authoritative nightly parser coverage changes to files and required evidence.

**Last updated:** 2026-06-19

| Surface | Examples |
|---------|----------|
| token gates | stmt/expr token and start predicates |
| parser code | `src/StyioParser/NewParserExpr.cpp`, `src/StyioParser/Parser.cpp` |
| route stats | zero accepted-grammar fallback and internal bridge counts |
| tests | `StyioParserEngine.*`, security parser subset tests, shadow gates |
| docs | EBNF, symbol reference, compact syntax docs |
