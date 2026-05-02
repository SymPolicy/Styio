set(STYIO_SYMBOL_SOURCES
  StyioParser/SymbolRegistry.cpp
)

set(STYIO_FRONTEND_FOUNDATION_SOURCES
  StyioToken/Token.cpp
  StyioUnicode/Unicode.cpp
  StyioParser/Parser.cpp
  StyioParser/ParserLookahead.cpp
  StyioParser/NewParserExpr.cpp
  StyioParser/Tokenizer.cpp
  StyioProfiler/FrontendProfiler.cpp
)

set(STYIO_FRONTEND_SEMA_IR_SOURCES
  StyioNative/NativeInterop.cpp
  StyioToString/ToString.cpp
  StyioSema/TypeInfer.cpp
  StyioLowering/AstToStyioIR.cpp
)

set(STYIO_FRONTEND_SOURCES
  ${STYIO_FRONTEND_FOUNDATION_SOURCES}
  ${STYIO_FRONTEND_SEMA_IR_SOURCES}
)
