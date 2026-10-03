include("${CMAKE_CURRENT_LIST_DIR}/StyioObservableProducerSources.cmake")

set(STYIO_CONTRACT_SOURCES
  StyioServices/StyioCLI/SyntaxCheck.cpp
  StyioServices/StyioCLI/RuntimeEventSession.cpp
  StyioServices/StyioConfig/CompilePlanContract.cpp
  StyioServices/StyioConfig/SourceBuildInfo.cpp
  ${STYIO_OBSERVABLE_PRODUCER_SOURCES}
)
