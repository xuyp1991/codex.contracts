project(sys.bridge)

set(CODEX_WASM_OLD_BEHAVIOR "Off")
find_package(codex.cdt)

add_contract( sys.bridge sys.bridge sys.bridge.cpp )
target_include_directories( sys.bridge PUBLIC ${CMAKE_SOURCE_DIR}/../include )
#target_ricardian_directory( hello ${CMAKE_SOURCE_DIR}/../ricardian )