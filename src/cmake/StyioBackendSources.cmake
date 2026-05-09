set(STYIO_RUNTIME_SUPPORT_SOURCES
  StyioExtern/ExternLib.cpp
)

set(STYIO_BACKEND_SOURCES
  StyioCodeGen/CodeGen.cpp
  StyioCodeGen/GetTypeG.cpp
  StyioCodeGen/CodeGenG.cpp
  StyioCodeGen/CodeGenPulse.cpp
  StyioCodeGen/GetTypeIO.cpp
  StyioCodeGen/CodeGenIO.cpp
)

set(STYIO_TESTING_SUPPORT_SOURCES
  StyioTesting/PipelineCheck.cpp
)

set(STYIO_CONTRACT_SOURCES
  StyioConfig/CompilePlanContract.cpp
  StyioConfig/SourceBuildInfo.cpp
)

set(STYIO_CORE_SOURCES
  ${STYIO_BACKEND_SOURCES}
  ${STYIO_TESTING_SUPPORT_SOURCES}
)
