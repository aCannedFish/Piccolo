# Link the production runtime so tests exercise the same animation and physics code.
add_executable(PiccoloAnimationTests animation_tests.cpp)
target_link_libraries(PiccoloAnimationTests PRIVATE PiccoloRuntime)
add_test(NAME PiccoloAnimationTests COMMAND PiccoloAnimationTests)
set_tests_properties(PiccoloAnimationTests PROPERTIES WORKING_DIRECTORY "${PICCOLO_ROOT_DIR}/engine")
