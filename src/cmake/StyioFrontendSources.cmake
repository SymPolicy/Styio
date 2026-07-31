set(STYIO_SYMBOL_SOURCES
  StyioParser/SymbolRegistry.cpp
)

set(STYIO_FRONTEND_FOUNDATION_SOURCES
  StyioPlatform/Platform.cpp
  StyioToken/Token.cpp
  StyioUnicode/Unicode.cpp
  StyioParser/Parser.cpp
  StyioParser/ParserLookahead.cpp
  StyioParser/NewParserExpr.cpp
  StyioParser/Tokenizer.cpp
  StyioProfiler/FrontendProfiler.cpp
  StyioUtil/SourceMap.cpp
  StyioSession/SymbolInterner.cpp
  StyioSession/TypeTable.cpp
)

set(STYIO_FRONTEND_SEMA_IR_SOURCES
  StyioNative/NativeInterop.cpp
  StyioResourceTopology/ResourceTopology.cpp
  StyioToString/ToString.cpp
  StyioIR/Verifier.cpp
  StyioSema/CallableInterface.cpp
  StyioSema/CallableModuleLoader.cpp
  StyioSema/SemanticAnalysis.cpp
  StyioSema/TypeInfer.cpp
  StyioLowering/AstToStyioIR.cpp
  StyioLowering/AstToStyioIRStage.cpp
  StyioLowering/StyioIROptimizer.cpp
)

set(STYIO_FRONTEND_SOURCES
  ${STYIO_FRONTEND_FOUNDATION_SOURCES}
  ${STYIO_FRONTEND_SEMA_IR_SOURCES}
)
