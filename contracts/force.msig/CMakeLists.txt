add_contract(force.msig force.msig ${CMAKE_CURRENT_SOURCE_DIR}/src/force.msig.cpp)

target_include_directories(force.msig
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(force.msig
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
