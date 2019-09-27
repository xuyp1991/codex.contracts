project(force.relay)

set(CODEX_WASM_OLD_BEHAVIOR "Off")
find_package(codex.cdt)

add_contract( force.relay force.relay force.relay.cpp )
target_include_directories( force.relay PUBLIC ${CMAKE_SOURCE_DIR}/../include )
#target_ricardian_directory( hello ${CMAKE_SOURCE_DIR}/../ricardian )