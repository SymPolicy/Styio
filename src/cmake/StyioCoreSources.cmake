include("${CMAKE_CURRENT_LIST_DIR}/StyioBackendSources.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/StyioTestingSources.cmake")

set(STYIO_CORE_SOURCES
  ${STYIO_BACKEND_SOURCES}
  ${STYIO_TESTING_SUPPORT_SOURCES}
)
