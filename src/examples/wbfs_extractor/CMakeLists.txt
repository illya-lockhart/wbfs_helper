cmake_minimum_required(VERSION 3.20.0 FATAL_ERROR)
if(TARGET wbfs_extractor)
	return()
endif()

add_subdirectory(../../ wbfs_utils)

add_executable(wbfs_extractor
	${CMAKE_CURRENT_LIST_DIR}/main.c)

target_link_libraries(wbfs_extractor PRIVATE wbfs_utils)
set_property(TARGET wbfs_extractor PROPERTY C_STANDARD 11)
set_property(DIRECTORY ${WBFS_UTILS_ROOT} PROPERTY VS_STARTUP_PROJECT wbfs_extractor)

