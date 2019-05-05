add_contract(force.token force.token ${CMAKE_CURRENT_SOURCE_DIR}/src/force.token.cpp)

target_include_directories(force.token
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(force.token
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
