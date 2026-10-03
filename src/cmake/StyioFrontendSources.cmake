# Composition shared by styio_frontend_core and the profile-pruned nano core.
# Edit an owned fragment for source membership; preserve this ordering.
include("${CMAKE_CURRENT_LIST_DIR}/StyioFrontendFoundationSources.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/StyioFrontendProfilerSources.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/StyioSemaIRSources.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/StyioNativeInteropSources.cmake")

set(STYIO_FRONTEND_FOUNDATION_SOURCES
  ${STYIO_FRONTEND_PARSER_SOURCES}
  ${STYIO_FRONTEND_PROFILER_SOURCES}
  ${STYIO_SEMANTIC_IDENTITY_SOURCES}
  ${STYIO_FRONTEND_SOURCE_MAP_SOURCES}
  ${STYIO_SESSION_SOURCES}
)

set(STYIO_FRONTEND_SOURCES
  ${STYIO_FRONTEND_FOUNDATION_SOURCES}
  ${STYIO_NATIVE_INTEROP_SOURCES}
  ${STYIO_FRONTEND_SEMA_IR_SOURCES}
)
