cmake_minimum_required( VERSION 3.5 )

project(contracts)

set(EOSIO_WASM_OLD_BEHAVIOR "Off")
find_package(forceio.cdt)

add_subdirectory(force.token)
add_subdirectory(relay.token)
add_subdirectory(sys.bridge)
#sys.match will add later
#add_subdirectory(sys.match)
add_subdirectory(force.msig)
add_subdirectory(force.relay)
add_subdirectory(force.system)
