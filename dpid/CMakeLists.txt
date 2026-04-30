# dpid daemon and client
add_definitions(-DEXEEXT="${CMAKE_EXECUTABLE_SUFFIX}")

# dpid daemon
set(DPID_SOURCES
    dpi.h
    dpi_socket_dir.h
    dpid.h
    dpid_common.h
    misc_new.h
    dpi.c
    dpi_socket_dir.c
    dpid.c
    dpid_common.c
    main.c
    misc_new.c
)

add_executable(dpid_bin ${DPID_SOURCES})
target_link_libraries(dpid_bin 
    Dpip
    Dlib
    ${CMAKE_THREAD_LIBS_INIT}
)
set_target_properties(dpid_bin PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# dpidc client
add_executable(dpidc dpidc.c)
target_link_libraries(dpidc 
    Dpip
    Dlib
)
set_target_properties(dpidc PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Generate dpidrc configuration file
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/dpidrc.in"
    "${CMAKE_CURRENT_BINARY_DIR}/dpidrc"
    @ONLY
)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/dpidrc" 
        DESTINATION ${CMAKE_INSTALL_SYSCONFDIR})

install(TARGETS dpid_bin dpidc DESTINATION ${CMAKE_INSTALL_BINDIR})
install(PROGRAMS ${CMAKE_BINARY_DIR}/dpid_bin DESTINATION ${CMAKE_INSTALL_BINDIR} RENAME dpid)
