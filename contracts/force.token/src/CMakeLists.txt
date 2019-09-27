project(force.token)

set(CODEX_WASM_OLD_BEHAVIOR "Off")
find_package(codex.cdt)

add_contract( force.token force.token force.token.cpp )
target_include_directories( force.token PUBLIC ${CMAKE_SOURCE_DIR}/../include )
target_ricardian_directory( force.token ${CMAKE_SOURCE_DIR}/../ricardian )