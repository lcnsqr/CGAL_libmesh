project( tetgen-libmesh )


cmake_minimum_required(VERSION 2.8.11)


# --- libMesh commands
# Check if there is a env. variable for the libMesh
set(LIBMESH_DIR $ENV{LIBMESH_DIR})

# If not, use the default path
if ( "${LIBMESH_DIR}" STREQUAL "" )
   set (LIBMESH_DIR "/usr/local" )
endif()

# Set the path to the libmesh-config binary
set(LIBMESH_CONFIG "${LIBMESH_DIR}/bin/libmesh-config")

# Get the libMesh's flags, includes, libs ...
execute_process(COMMAND ${LIBMESH_CONFIG} --cppflags OUTPUT_VARIABLE LIBMESH_CPPFLAGS OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND ${LIBMESH_CONFIG} --cxxflags OUTPUT_VARIABLE LIBMESH_CXXFLAGS OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND ${LIBMESH_CONFIG} --include OUTPUT_VARIABLE LIBMESH_INCLUDE OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND ${LIBMESH_CONFIG} --libs OUTPUT_VARIABLE LIBMESH_LIBS OUTPUT_STRIP_TRAILING_WHITESPACE)

execute_process(COMMAND ${LIBMESH_CONFIG} --version OUTPUT_VARIABLE LIBMESH_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE)

# Append them to the CMake flags and linkers
set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${LIBMESH_CPPFLAGS} ${LIBMESH_CXXFLAGS} ${LIBMESH_INCLUDE}" )
set( CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${LIBMESH_LIBS}" ) 

message(STATUS "libMesh version: ${LIBMESH_VERSION}")
message(STATUS "libMesh dir: ${LIBMESH_DIR}")


# Creating entries for target: tetgen-libmesh
# ############################

add_executable( tetgen-libmesh tetgen-libmesh.cxx )
