set(STYIO_CONTRACT_SOURCES
  StyioServices/StyioCLI/SyntaxCheck.cpp
  StyioServices/StyioConfig/CompilePlanContract.cpp
  StyioServices/StyioConfig/SourceBuildInfo.cpp
)

set(STYIO_IDE_SOURCES
  StyioServices/StyioIDE/Common.cpp
  StyioServices/StyioIDE/VFS.cpp
  StyioServices/StyioIDE/CompilerBridge.cpp
  StyioServices/StyioIDE/Syntax.cpp
  StyioServices/StyioIDE/TreeSitterBackend.cpp
  StyioServices/StyioIDE/HIR.cpp
  StyioServices/StyioIDE/Project.cpp
  StyioServices/StyioIDE/Index.cpp
  StyioServices/StyioIDE/SemDB.cpp
  StyioServices/StyioIDE/Service.cpp
)

set(STYIO_LSP_SOURCES
  StyioServices/StyioLSP/Server.cpp
  StyioServices/StyioLSP/main.cpp
)
