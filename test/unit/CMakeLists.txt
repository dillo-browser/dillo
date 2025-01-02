set(BASE_LIBS
  lout
)

function(add_unit_test_executable exe_name sources extra_libs)
  add_executable(${exe_name} ${sources})
  target_include_directories(${exe_name} PRIVATE ${PROJECT_SOURCE_DIR} ${CMAKE_BINARY_DIR})  
  target_link_libraries(${exe_name} PRIVATE ${BASE_LIBS} ${extra_libs})
  target_compile_definitions(${exe_name} PRIVATE 
    CUR_SRC_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    CUR_WORKING_DIR="${BASE_CUR_WORKING_DIR}/test/unit"
  )
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

add_unit_test_executable(unicode_test
  unicode_test.cc
  "Dw-fltk"
)

add_unit_test_executable(liang
  liang.cc
  "Dw-widgets;Dw-fltk;Dw-core;Dlib"
)

add_unit_test_executable(cookies
  cookies.c
  "Dpip;Dlib"
)

add_unit_test_executable(trie
  trie.cc
  "Dw-widgets;Dw-fltk;Dw-core;Dlib"
)

foreach(tst IN ITEMS containers identity liang notsosimplevector shapes unicode_test)
  add_test(NAME ${tst} COMMAND $<TARGET_FILE:${tst}>)
endforeach()

install(FILES hyph-en-us.pat hyph-de.pat DESTINATION share/dillo/tests/unit)
