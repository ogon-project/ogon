function(THRIFT_GEN VAR)
  IF (NOT ARGN)
    MESSAGE(SEND_ERROR "Error: THRIFT_GEN called without any src files")
    RETURN()
  ENDIF(NOT ARGN)

  set(${VAR})
  foreach(FIL ${ARGN})
    # Get full path
    get_filename_component(ABS_FIL ${FIL} ABSOLUTE)
    # Get basename
    get_filename_component(FIL_WE ${FIL} NAME_WE)

    set(OUTPUT_THRIFT_FILE "${THRIFT_OUTPUT_DIR}/${FIL_WE}_types.cpp")
    set(OUTPUT_THRIFT_FILE ${OUTPUT_THRIFT_FILE} "${THRIFT_OUTPUT_DIR}/${FIL_WE}_types.h")
    set(OUTPUT_THRIFT_FILE ${OUTPUT_THRIFT_FILE} "${THRIFT_OUTPUT_DIR}/${FIL_WE}_constants.cpp")
    set(OUTPUT_THRIFT_FILE ${OUTPUT_THRIFT_FILE} "${THRIFT_OUTPUT_DIR}/${FIL_WE}_constants.h")
    set(OUTPUT_THRIFT_FILE ${OUTPUT_THRIFT_FILE} "${THRIFT_OUTPUT_DIR}/${FIL_WE}.cpp")

    list(APPEND ${VAR} ${OUTPUT_THRIFT_FILE})

    set(CPP_ARGS ${THRIFT_INCLUDE_DIR_OPTION} --gen cpp -out ${THRIFT_OUTPUT_DIR})

    add_custom_command(
        OUTPUT ${OUTPUT_THRIFT_FILE}
        COMMAND ${THRIFT_COMPILER} ${CPP_ARGS} ${FIL}
        DEPENDS ${ABS_FIL}
        COMMENT "Running thrift compiler on ${FIL}"
        VERBATIM
    )
  endforeach(FIL)

  set(${VAR} ${${VAR}} PARENT_SCOPE)
endfunction(THRIFT_GEN)
