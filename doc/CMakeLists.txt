set(DILLO_MAN_PAGE "${CMAKE_CURRENT_BINARY_DIR}/dillo.1")

add_custom_command(
    OUTPUT  "${DILLO_MAN_PAGE}"
    COMMAND ${CMAKE_COMMAND} -E echo "Generating dillo.1 with prefix replaced"
    COMMAND sed "s%/usr/local%${CMAKE_INSTALL_PREFIX}%g" 
            < "${CMAKE_CURRENT_SOURCE_DIR}/dillo.1.in"
            > "${DILLO_MAN_PAGE}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/dillo.1.in"
    COMMENT "Creating man page dillo.1"
)

set(DILLO_HTML_HELP "${CMAKE_CURRENT_BINARY_DIR}/user_help.html")

add_custom_command(
    OUTPUT  "${DILLO_HTML_HELP}"
    COMMAND ${CMAKE_COMMAND} -E echo "Generating user_help.html with project version"
    COMMAND sed "s/__VERSION__/${PROJECT_VERSION}/g" 
            < "${CMAKE_CURRENT_SOURCE_DIR}/user_help.in.html"
            > "${DILLO_HTML_HELP}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/user_help.in.html"
    COMMENT "Creating user_help.html"
)

add_custom_target(doc ALL
    DEPENDS "${DILLO_MAN_PAGE}" "${DILLO_HTML_HELP}"
)

install(FILES
    "${DILLO_MAN_PAGE}"
    DESTINATION share/man/man1
)

install(FILES
    "${DILLO_HTML_HELP}"
    DESTINATION share/dillo
)

install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/README"
    "${CMAKE_CURRENT_SOURCE_DIR}/Cookies.txt"
    "${CMAKE_CURRENT_SOURCE_DIR}/install.md"
    DESTINATION share/dillo/doc
)