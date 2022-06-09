cmake_minimum_required(VERSION 3.20.0 FATAL_ERROR)
if(TARGET wbfs_utils)
	return()
endif()

set(WBFS_UTILS_ROOT ${CMAKE_CURRENT_LIST_DIR} CACHE STRING "Root for wbfs_utils")
include_directories(${CMAKE_CURRENT_LIST_DIR})

add_library(wbfs_utils 
	wbfs.h
	wbfs.c
	wbfs_helper.c
)

add_subdirectory(examples)
