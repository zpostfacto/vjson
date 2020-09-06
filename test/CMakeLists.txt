# cmake script to build the vjson *TESTS* with google test.
# There is not makefile to build vjson itself.  Simply add
# vjson.cpp to your project.

cmake_minimum_required(VERSION 3.0)
project(test_jvson C CXX)

# Add in googletest.  We're including this as a submodule, so turn off some
# options
option(BUILD_GMOCK "Builds the googlemock subproject" OFF)
option(INSTALL_GTEST "Enable installation of googletest. (Projects embedding googletest may want to turn this OFF.)" OFF)
add_subdirectory( googletest )

add_executable(
	test_vjson
	test_vjson.cpp
	../vjson.cpp)
target_link_libraries(test_vjson gtest_main)
#add_test( FIXME )
