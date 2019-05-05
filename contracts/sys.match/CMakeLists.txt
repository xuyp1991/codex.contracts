add_contract(sys.match sys.match ${CMAKE_CURRENT_SOURCE_DIR}/src/sys.match.cpp)

target_include_directories(sys.match
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(sys.match
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
