set(STYIO_SEMANTIC_IDENTITY_SOURCES
  StyioUtil/SemanticIdentity.cpp
)

set(STYIO_SESSION_SOURCES
  StyioSession/SymbolInterner.cpp
  StyioSession/TypeTable.cpp
)

set(STYIO_FRONTEND_SEMA_IR_SOURCES
  StyioResourceTopology/ResourceTopology.cpp
  StyioToString/ToString.cpp
  StyioIR/Verifier.cpp
  StyioIR/PortableCallableBody.cpp
  StyioLowering/PortableCallableBody.cpp
  StyioSema/CallableInterface.cpp
  StyioSema/CallableModuleLoader.cpp
  StyioSema/CallableSpecializationGraph.cpp
  StyioSema/SemanticAnalysis.cpp
  StyioSema/TypeInfer.cpp
  StyioLowering/AstToStyioIR.cpp
  StyioLowering/AstToStyioIRStage.cpp
  StyioLowering/StyioIROptimizer.cpp
)
