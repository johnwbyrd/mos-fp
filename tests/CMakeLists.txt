# Unit tests
add_executable(test_type_selection
    unit/test_type_selection.cpp
)

target_link_libraries(test_type_selection PRIVATE opine)

# Add as a test
add_test(NAME type_selection COMMAND test_type_selection)

# Pack/unpack tests
add_executable(test_pack_unpack
    unit/test_pack_unpack.cpp
)

target_link_libraries(test_pack_unpack PRIVATE opine)

# Add as a test
add_test(NAME pack_unpack COMMAND test_pack_unpack)

# Rounding logic tests
add_executable(test_rounding_logic
    unit/test_rounding_logic.cpp
)

target_link_libraries(test_rounding_logic PRIVATE opine)
target_include_directories(test_rounding_logic PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Add as a test
add_test(NAME rounding_logic COMMAND test_rounding_logic)

# Rounding oracle tests (native float comparison)
add_executable(test_rounding_oracle
    unit/test_rounding_oracle.cpp
)

target_link_libraries(test_rounding_oracle PRIVATE opine)
target_include_directories(test_rounding_oracle PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Add as a test
add_test(NAME rounding_oracle COMMAND test_rounding_oracle)
