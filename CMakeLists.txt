
if(CMAKE_BUILD_TYPE STREQUAL "")
	set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
endif()

add_executable(batt "batt.c")
target_link_libraries(batt "pthread")
