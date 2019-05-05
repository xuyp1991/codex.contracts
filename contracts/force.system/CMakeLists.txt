add_contract(force.system force.system ${CMAKE_CURRENT_SOURCE_DIR}/src/force.system.cpp)

target_include_directories(force.system
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include
   ${CMAKE_CURRENT_SOURCE_DIR}/../force.token/include)

set_target_properties(force.system
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")


