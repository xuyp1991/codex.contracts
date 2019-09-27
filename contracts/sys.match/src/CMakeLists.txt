project(sys.match)

set(CODEX_WASM_OLD_BEHAVIOR "Off")
find_package(codex.cdt)

add_contract( sys.match sys.match sys.match.cpp )
target_include_directories( sys.match PUBLIC ${CMAKE_SOURCE_DIR}/../include )
#target_ricardian_directory( hello ${CMAKE_SOURCE_DIR}/../ricardian )