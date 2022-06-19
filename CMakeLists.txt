cmake_minimum_required(VERSION 3.20.0 FATAL_ERROR)
if(TARGET wbfs_utils)
	return()
endif()

# All options
option(WBFS_HELPER_BUILD_EXAMPLES "Build examples for Wbfs helper" ON)

# Create the Wbfs_utils target
project(wbfs_utils VERSION 0.1.0 LANGUAGES C)

# Set the include path so the library can define symbols for the
# external headers and include the source for the library
include_directories(${CMAKE_CURRENT_LIST_DIR}/include)
add_subdirectory(src)

# Add in the aes helper library
add_subdirectory(external/aes_light)

# Add the examples
if(${WBFS_HELPER_BUILD_EXAMPLES})
	add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/examples)
endif()