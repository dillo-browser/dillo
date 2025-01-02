set(BASE_LIBS
  lout
)

function(add_unit_test_executable exe_name sources extra_libs)
  add_executable(${exe_name} ${sources})
  target_include_directories(${exe_name} PRIVATE ${PROJECT_SOURCE_DIR} ${CMAKE_BINARY_DIR})  
  target_link_libraries(${exe_name} PRIVATE ${BASE_LIBS} ${extra_libs} ${FLTK_LIBRARIES})
  target_compile_definitions(${exe_name} PRIVATE 
    CUR_SRC_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    CUR_WORKING_DIR="${BASE_CUR_WORKING_DIR}/test/unit"
  )
  if(DEFINED FLTK_CFLAGS)
    target_compile_options(${exe_name} PRIVATE ${FLTK_CFLAGS})
  endif()
  if(DEFINED FLTK_CXXFLAGS)
    target_compile_options(${exe_name} PRIVATE ${FLTK_CXXFLAGS})
  endif()
endfunction()

add_unit_test_executable(containers
  containers.cc
  "Dlib"
)

add_unit_test_executable(identity
  identity.cc
  "Dlib"
)

add_unit_test_executable(notsosimplevector
  notsosimplevector.cc
  ""
)

add_unit_test_executable(shapes
  shapes.cc
  "Dw-core;Dlib"
)

## unicode_test
add_unit_test_executable(unicode_test
  unicode_test.cc
  "${LIBFLTK_LIBS};${LIBX11_LIBS}"
)

add_unit_test_executable(liang
  liang.cc
  "Dw-widgets;Dw-fltk;Dw-core;Dlib;${LIBFLTK_LIBS};${LIBX11_LIBS}"
)

add_unit_test_executable(cookies
  cookies.c
  "Dpip;Dlib"
)

add_unit_test_executable(trie
  trie.cc
  "Dw-widgets;Dw-fltk;Dw-core;Dlib;${LIBFLTK_LIBS};${LIBX11_LIBS}"
)

foreach(tst IN ITEMS containers identity liang notsosimplevector shapes unicode_test)
  add_test(NAME ${tst} COMMAND $<TARGET_FILE:${tst}>)
endforeach()

install(FILES hyph-en-us.pat hyph-de.pat DESTINATION share/dillo/tests/unit)
