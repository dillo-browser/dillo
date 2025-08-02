set(DRIVER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/driver.sh")

set(HTML_TEST_FILES
    render/b-div.html
    render/div-100-percent-with-padding.html
    render/float-img-justify.html
    render/github-infinite-loop.html
    render/hackernews.html
    render/img-aspect-ratio-absolute.html
    render/img-aspect-ratio-div.html
    render/img-aspect-ratio-mix-border.html
    render/img-aspect-ratio-mix-margin.html
    render/img-aspect-ratio-mix.html
    render/img-aspect-ratio-relative-inside-fixed.html
    render/img-aspect-ratio.html
    render/main-style.html
    render/margin-auto.html
    render/max-width-body.html
    render/max-width-div-clamp.html
    render/max-width-div.html
    render/max-width-html.html
    render/max-width-nested-div.html
    render/meta-refresh-0-no-url.html
    render/min-width-body.html
    render/min-width-div-extend.html
    render/min-width-div.html
    render/min-width-html.html
    render/min-width-nested-div.html
    render/span-padding.html
    render/svg-current-color.html
    render/table-max-width.html
    render/table-missing-width-in-one-column.html
    render/table-td-width-percent-img.html
    render/table-td-width-percent.html
    render/table-thead-tfoot.html
    render/table-thead-tfoot-open-tag.html
    render/white-space.html
)

# 3) Tests that we EXPECT TO FAIL (XFAIL_TESTS)
set(XFAIL_TESTS
    render/div-100-percent-with-padding.html
    render/float-img-justify.html
    render/margin-auto.html
    render/max-width-html.html
    render/min-width-html.html
    render/span-padding.html
    render/table-td-width-percent.html
)

function(add_html_test test_file)
  get_filename_component(test_name "${test_file}" NAME)

  add_test(NAME "${test_name}"
    COMMAND
      "${DRIVER_SCRIPT}" "${CMAKE_CURRENT_SOURCE_DIR}/${test_file}"
  )

  set_tests_properties("${test_name}" PROPERTIES
    ENVIRONMENT
      "TOP_BUILDDIR=${CMAKE_BINARY_DIR}"
      "TOP_SRCDIR=${CMAKE_SOURCE_DIR}"
      "BUILDDIR=${CMAKE_CURRENT_BINARY_DIR}"
  )

  # If it's in the XFAIL list, mark it as WILL_FAIL
  list(FIND XFAIL_TESTS "${test_file}" xfail_index)
  if(xfail_index GREATER -1)
    # Means found in XFAIL_TESTS
    set_tests_properties("${test_name}" PROPERTIES WILL_FAIL TRUE)
  endif()
endfunction()

foreach(test_file IN LISTS HTML_TEST_FILES)
  add_html_test("${test_file}")
endforeach()

install(FILES driver.sh DESTINATION share/dillo/test/html)
install(DIRECTORY manual render DESTINATION share/dillo/test/html)
