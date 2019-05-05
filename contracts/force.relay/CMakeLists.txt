add_contract(force.relay force.relay ${CMAKE_CURRENT_SOURCE_DIR}/src/force.relay.cpp)

target_include_directories(force.relay
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(force.relay
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
