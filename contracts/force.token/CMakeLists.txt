

project(force.token)

include(ExternalProject)
# if no cdt root is given use default path
if(CODEX_CDT_ROOT STREQUAL "" OR NOT CODEX_CDT_ROOT)
   find_package(codex.cdt)
endif()

ExternalProject_Add(
   force.token_project
   SOURCE_DIR ${PROJECT_SOURCE_DIR}/src
   BINARY_DIR ${CMAKE_BINARY_DIR}/contracts/force.token
   CMAKE_ARGS -DCMAKE_TOOLCHAIN_FILE=${CODEX_CDT_ROOT}/lib/cmake/codex.cdt/EosioWasmToolchain.cmake
   UPDATE_COMMAND ""
   PATCH_COMMAND ""
   TEST_COMMAND ""
   INSTALL_COMMAND ""
   BUILD_ALWAYS 1
)
