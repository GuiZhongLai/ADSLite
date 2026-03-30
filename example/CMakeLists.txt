
add_executable(example main.cpp)
add_executable(example_file main_file.cpp)

target_link_libraries(example PRIVATE ADSLite::ADSLite)
target_link_libraries(example_file PRIVATE ADSLite::ADSLite)