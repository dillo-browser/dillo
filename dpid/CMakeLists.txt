include(GNUInstallDirs)

set(DPIDRC_INPUT  "${PROJECT_SOURCE_DIR}/cmake/dpidrc.in")
set(DPIDRC_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/dpidrc")

if(NOT CMAKE_EXECUTABLE_SUFFIX)
  set(CMAKE_EXECUTABLE_SUFFIX "")
endif()

add_executable(dpid
    main.c
    dpid.c
    dpid_common.c
    dpi.c
    dpi_socket_dir.c
    misc_new.c
    dpi.h
    dpi_socket_dir.h
    dpid.h
    dpid_common.h
    misc_new.h
)

target_compile_definitions(dpid PRIVATE
    DILLO_SYSCONF="${CMAKE_INSTALL_SYSCONFDIR}" ${CMAKE_INSTALL}
    DILLO_DOCDIR="${CMAKE_INSTALL_DOCDIR}"
    CUR_WORKING_DIR="${BASE_CUR_WORKING_DIR}/src"
    EXEEXT="${CMAKE_EXECUTABLE_SUFFIX}"
    DPIDRC_SYS="${CMAKE_INSTALL_SYSCONFDIR}/dpidrc"
)


target_link_libraries(dpid PRIVATE
    Dpip
    Dlib
)

add_executable(dpidc
    dpidc.c
)

target_compile_definitions(dpidc PRIVATE
    EXEEXT="${CMAKE_EXECUTABLE_SUFFIX}"
    DPIDRC_SYS="${CMAKE_INSTALL_SYSCONFDIR}/dpidrc"
)

target_link_libraries(dpidc PRIVATE
    Dpip
    Dlib
)

configure_file(
    "${DPIDRC_INPUT}"
    "${DPIDRC_OUTPUT}"
    @ONLY
)

add_custom_target(dpidrc_gen ALL
    DEPENDS "${DPIDRC_OUTPUT}"
)


install(FILES "${DPIDRC_OUTPUT}"
    DESTINATION "${CMAKE_INSTALL_SYSCONFDIR}"
)

install(TARGETS dpid dpidc
    RUNTIME DESTINATION bin
)