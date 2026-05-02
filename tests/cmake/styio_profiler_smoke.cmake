if(NOT DEFINED STYIO_BIN)
  message(FATAL_ERROR "STYIO_BIN is required")
endif()
if(NOT DEFINED STYIO_SOURCE)
  message(FATAL_ERROR "STYIO_SOURCE is required")
endif()
if(NOT DEFINED STYIO_PROFILE_OUT)
  message(FATAL_ERROR "STYIO_PROFILE_OUT is required")
endif()

get_filename_component(_profile_dir "${STYIO_PROFILE_OUT}" DIRECTORY)
file(MAKE_DIRECTORY "${_profile_dir}")
file(REMOVE "${STYIO_PROFILE_OUT}")

execute_process(
  COMMAND "${STYIO_BIN}"
          --parser-engine nightly
          --profile-frontend
          --profile-out "${STYIO_PROFILE_OUT}"
          --file "${STYIO_SOURCE}"
  RESULT_VARIABLE _result
  OUTPUT_VARIABLE _stdout
  ERROR_VARIABLE _stderr
)

if(NOT _result EQUAL 0)
  message(FATAL_ERROR "styio profiler smoke failed with ${_result}\nstdout:\n${_stdout}\nstderr:\n${_stderr}")
endif()

if(NOT EXISTS "${STYIO_PROFILE_OUT}")
  message(FATAL_ERROR "styio profiler did not write ${STYIO_PROFILE_OUT}")
endif()

file(READ "${STYIO_PROFILE_OUT}" _profile_json)
set(_needles
  "\"tool\": \"styio-profiler\""
  "\"scope\": \"frontend\""
  "\"name\": \"tokenize\""
  "\"name\": \"parse\""
  "\"name\": \"type_infer\""
  "\"name\": \"lower_styio_ir\""
  "\"token_histogram\""
  "\"parser_route\""
  "\"nightly_subset_statements\""
)

foreach(_needle IN LISTS _needles)
  string(FIND "${_profile_json}" "${_needle}" _needle_pos)
  if(_needle_pos EQUAL -1)
    message(FATAL_ERROR "styio profiler JSON missing ${_needle}\n${_profile_json}")
  endif()
endforeach()
