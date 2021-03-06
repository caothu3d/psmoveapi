cmake_minimum_required(VERSION 2.8.12)
set(ROOT_DIR ${CMAKE_CURRENT_LIST_DIR}/../..)
include(${ROOT_DIR}/contrib/common.cmake)

# Unit Testing Suite
include_directories(${MSVC_INCLUDES})
include_directories(${ROOT_DIR}/src)
include_directories(${ROOT_DIR}/src/tracker)
include_directories(${ROOT_DIR}/src/math)
add_executable(unit_test_suite ${CMAKE_CURRENT_LIST_DIR}/unit_test_suite.cpp
	${CMAKE_CURRENT_LIST_DIR}/alignment_unit_test.cpp
	${CMAKE_CURRENT_LIST_DIR}/kalman_filter_unit_test.cpp
	${CMAKE_CURRENT_LIST_DIR}/matrix_unit_test.cpp
	${CMAKE_CURRENT_LIST_DIR}/quaternion_unit_test.cpp
	${CMAKE_CURRENT_LIST_DIR}/vector_unit_test.c)
    
target_link_libraries(unit_test_suite psmoveapi_static)
target_link_libraries(unit_test_suite psmoveapi_tracker_static)
set_target_properties(unit_test_suite PROPERTIES
	COMPILE_FLAGS -DUSING_STATIC_LIBRARY)
list(APPEND PSMOVEAPI_INSTALL_TARGETS unit_test_suite)