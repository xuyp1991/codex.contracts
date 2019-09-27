project(relay.token)

set(CODEX_WASM_OLD_BEHAVIOR "Off")
find_package(codex.cdt)

add_contract( relay.token relay.token relay.token.cpp )
target_include_directories( relay.token PUBLIC ${CMAKE_SOURCE_DIR}/../include )
target_ricardian_directory( relay.token ${CMAKE_SOURCE_DIR}/../ricardian )