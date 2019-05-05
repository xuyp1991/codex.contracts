add_contract(sys.bridge sys.bridge ${CMAKE_CURRENT_SOURCE_DIR}/src/sys.bridge.cpp)

target_include_directories(sys.bridge
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(sys.bridge
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
