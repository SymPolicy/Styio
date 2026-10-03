# Public contract target; runtime correlation is jointly owned with Runtime.
include("${CMAKE_CURRENT_LIST_DIR}/StyioRuntimeCorrelationSources.cmake")

set(STYIO_OBSERVABLE_PUBLIC_SOURCES
  StyioServices/StyioObservable/Snapshot.cpp
  StyioServices/StyioObservable/Delta.cpp
  StyioServices/StyioObservable/Query.cpp
  StyioServices/StyioObservable/Service.cpp
  ${STYIO_RUNTIME_CORRELATION_SOURCES}
)
