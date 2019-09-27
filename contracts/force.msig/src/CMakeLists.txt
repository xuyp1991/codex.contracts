project(force.msig)

set(CODEX_WASM_OLD_BEHAVIOR "Off")
find_package(codex.cdt)

add_contract( force.msig force.msig force.msig.cpp )
target_include_directories( force.msig PUBLIC ${CMAKE_SOURCE_DIR}/../include )
#target_ricardian_directory( hello ${CMAKE_SOURCE_DIR}/../ricardian )