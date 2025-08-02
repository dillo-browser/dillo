set(DW_LIBS
    lout
    Dw-widgets
    Dw-fltk
    Dw-core
    Dlib
    ${DILLO_FLTK_LIBS}
)

function(add_dw_test_executable exe_name)
  add_executable(${exe_name} ${ARGN})
  target_include_directories(${exe_name} PRIVATE ${PROJECT_SOURCE_DIR} ${CMAKE_BINARY_DIR} ${DILLO_FLTK_INCLUDE_DIR})
  target_compile_definitions(${exe_name} PRIVATE CUR_WORKING_DIR="${BASE_CUR_WORKING_DIR}/test")
  target_link_libraries(${exe_name} PRIVATE ${DW_LIBS})
endfunction()

add_dw_test_executable(dw-anchors-test    dw_anchors_test.cc)
add_dw_test_executable(dw-border-test     dw_border_test.cc)
add_dw_test_executable(dw-example         dw_example.cc)
add_dw_test_executable(dw-find-test       dw_find_test.cc)
add_dw_test_executable(dw-float-test      dw_float_test.cc)
add_dw_test_executable(dw-links           dw_links.cc)
add_dw_test_executable(dw-links2          dw_links2.cc)
add_dw_test_executable(dw-image-background dw_image_background.cc)
add_dw_test_executable(dw-images-simple   dw_images_simple.cc)
add_dw_test_executable(dw-images-scaled   dw_images_scaled.cc)
add_dw_test_executable(dw-images-scaled2  dw_images_scaled2.cc)
add_dw_test_executable(dw-lists           dw_lists.cc)
add_dw_test_executable(dw-resource-test   dw_resource_test.cc)

add_dw_test_executable(dw-simple-container-test
  dw_simple_container.cc
  dw_simple_container.hh
  dw_simple_container_test.cc
)

add_dw_test_executable(dw-table           dw_table.cc)
add_dw_test_executable(dw-table-aligned   dw_table_aligned.cc)
add_dw_test_executable(dw-ui-test
  form.cc
  dw_ui_test.cc
  form.hh
)

add_dw_test_executable(dw-imgbuf-mem-test dw_imgbuf_mem_test.cc)

add_test(NAME dw-imgbuf-mem-test
         COMMAND $<TARGET_FILE:dw-imgbuf-mem-test>)