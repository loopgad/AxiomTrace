if(NOT DEFINED AXIOMTRACE_CORE_ARCHIVE OR
   NOT EXISTS "${AXIOMTRACE_CORE_ARCHIVE}")
    message(FATAL_ERROR
        "AxiomTrace core archive is missing: ${AXIOMTRACE_CORE_ARCHIVE}")
endif()

set(AXIOMTRACE_NM_EXECUTABLE "${AXIOMTRACE_NM}")
if(NOT AXIOMTRACE_NM_EXECUTABLE OR
   NOT EXISTS "${AXIOMTRACE_NM_EXECUTABLE}")
    find_program(AXIOMTRACE_NM_EXECUTABLE NAMES llvm-nm nm)
endif()
if(NOT AXIOMTRACE_NM_EXECUTABLE)
    message(FATAL_ERROR "An nm-compatible tool is required for the port-free core check")
endif()

execute_process(
    COMMAND "${AXIOMTRACE_NM_EXECUTABLE}" --defined-only
            "${AXIOMTRACE_CORE_ARCHIVE}"
    RESULT_VARIABLE AXIOMTRACE_NM_RESULT
    OUTPUT_VARIABLE AXIOMTRACE_NM_OUTPUT
    ERROR_VARIABLE AXIOMTRACE_NM_ERROR
    OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT AXIOMTRACE_NM_RESULT EQUAL 0)
    message(FATAL_ERROR
        "Could not inspect AxiomTrace core archive: ${AXIOMTRACE_NM_ERROR}")
endif()

if(AXIOMTRACE_NM_OUTPUT MATCHES
   "[ \t][A-Za-z][ \t]+axiom_port_[A-Za-z0-9_]+")
    message(FATAL_ERROR
        "AxiomTrace core archive unexpectedly defines an axiom_port_* symbol")
endif()
