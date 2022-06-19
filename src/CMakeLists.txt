cmake_minimum_required(VERSION 3.20.0 FATAL_ERROR)
if(TARGET wbfs_utils)
	return()
endif()

set(WBFS_UTILS_ROOT ${CMAKE_CURRENT_LIST_DIR} CACHE STRING "Root for wbfs_utils")
include_directories(${CMAKE_CURRENT_LIST_DIR})

add_library(wbfs_utils 
	wbfs.c
	wbfs_helper.c
)

target_sources(wbfs_utils PUBLIC ${WBFS_UTILS_ROOT}/../include/wbfs.h)

set_property(TARGET wbfs_utils PROPERTY C_STANDARD 90)