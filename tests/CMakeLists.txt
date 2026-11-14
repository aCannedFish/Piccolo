# Link the production runtime so tests exercise the same animation and physics code.
add_executable(PiccoloAnimationTests animation_tests.cpp)
target_link_libraries(PiccoloAnimationTests PRIVATE PiccoloRuntime)
add_test(NAME PiccoloAnimationTests COMMAND PiccoloAnimationTests)
set_tests_properties(PiccoloAnimationTests PROPERTIES WORKING_DIRECTORY "${PICCOLO_ROOT_DIR}/engine")

add_executable(PiccoloControllerTests controller_tests.cpp)
target_link_libraries(PiccoloControllerTests PRIVATE PiccoloRuntime)
add_test(NAME PiccoloControllerTests COMMAND PiccoloControllerTests)

add_executable(PiccoloMotorTests motor_tests.cpp)
target_link_libraries(PiccoloMotorTests PRIVATE PiccoloRuntime)
add_test(NAME PiccoloMotorTests COMMAND PiccoloMotorTests)

add_executable(PiccoloVignetteTests vignette_tests.cpp)
target_link_libraries(PiccoloVignetteTests PRIVATE PiccoloRuntime)
add_test(NAME PiccoloVignetteTests COMMAND PiccoloVignetteTests)

# This executable needs a display and Vulkan. Run it explicitly with an editor INI path.
add_executable(PiccoloGameplaySmoke gameplay_smoke.cpp)
target_link_libraries(PiccoloGameplaySmoke PRIVATE PiccoloRuntime)

add_executable(PiccoloVignetteGpuTests vignette_gpu_tests.cpp)
target_link_libraries(PiccoloVignetteGpuTests PRIVATE PiccoloRuntime)

add_executable(PiccoloPostProcessSmoke post_process_smoke.cpp)
target_link_libraries(PiccoloPostProcessSmoke PRIVATE PiccoloRuntime)
