add_contract(relay.token relay.token ${CMAKE_CURRENT_SOURCE_DIR}/src/relay.token.cpp)

target_include_directories(relay.token
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(relay.token
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
