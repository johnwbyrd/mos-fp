# Unit tests
add_executable(test_type_selection
    unit/test_type_selection.cpp
)

target_link_libraries(test_type_selection PRIVATE opine)

# Add as a test
add_test(NAME type_selection COMMAND test_type_selection)
