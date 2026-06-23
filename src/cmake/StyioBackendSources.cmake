set(STYIO_RUNTIME_SUPPORT_SOURCES
  StyioExtern/ExternLib.cpp
  StyioRuntime/RuntimeState.cpp
)

set(STYIO_BACKEND_SOURCES
  StyioCodeGen/GetTypeG.cpp
  StyioCodeGen/CodeGenG.cpp
  StyioCodeGen/LLVMEmission.cpp
  StyioCodeGen/CodeGenPulse.cpp
  StyioCodeGen/GetTypeIO.cpp
  StyioCodeGen/CodeGenIO.cpp
)

set(STYIO_TESTING_SUPPORT_SOURCES
  StyioTesting/PipelineCheck.cpp
)

set(STYIO_CORE_SOURCES
  ${STYIO_BACKEND_SOURCES}
  ${STYIO_TESTING_SUPPORT_SOURCES}
)
