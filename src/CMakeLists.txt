# Target composition only. Source membership and target settings live beside
# their owners; do not reintroduce unrelated target edits in this entry point.
include("${CMAKE_CURRENT_LIST_DIR}/cmake/StyioTargetHelpers.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/targets/StyioSymbolCore.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/targets/StyioFrontendCore.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/targets/StyioRuntimeCore.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/targets/StyioCore.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/targets/StyioObservableCore.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/targets/StyioCLIContractCore.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/targets/StyioIDECore.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/targets/Styio.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/targets/StyioLSPD.cmake")

if(STYIO_BUILD_NANO)
  include("${CMAKE_CURRENT_LIST_DIR}/cmake/targets/StyioNanoCore.cmake")
  include("${CMAKE_CURRENT_LIST_DIR}/cmake/targets/StyioNano.cmake")
endif()
