cmake_minimum_required(VERSION 3.14)
project(d64lib)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

#unittests
add_subdirectory(unittests)

# Add library
add_library(d64lib d64.cpp d64.h d64_types.h)

# Export the include directory
target_include_directories(d64lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_directories(d64lib PUBLIC ${CMAKE_CURRENT_LINK_DIR})

# Install rules (optional for packaging)
install(TARGETS d64lib DESTINATION lib)
install(FILES d64.h DESTINATION include)
