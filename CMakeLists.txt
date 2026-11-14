cmake_minimum_required(VERSION 3.19 FATAL_ERROR)

project(Piccolo VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(BUILD_SHARED_LIBS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CMakeDependentOption)

# ---- Include guards ----
if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
  message(
    FATAL_ERROR
      "In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there."
  )
endif()

set(PICCOLO_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(CMAKE_INSTALL_PREFIX "${PICCOLO_ROOT_DIR}/bin")
set(BINARY_ROOT_DIR "${CMAKE_INSTALL_PREFIX}/$<CONFIG>")


add_subdirectory(engine)

# Runtime regression tests run without a window or a Vulkan device.
option(PICCOLO_BUILD_TESTS "Build Piccolo runtime regression tests" OFF)
if(PICCOLO_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
