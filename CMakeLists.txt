cmake_minimum_required(VERSION 2.8)

project(filewatcherpp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g -ggdb")
set(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O2 -Wall")

IF (CMAKE_SYSTEM_NAME MATCHES "Linux")
    link_libraries(pthread)
ENDIF(CMAKE_SYSTEM_NAME MATCHES "Linux")

set(SOURCES
        filewatcherpp.hpp)
include_directories(./Test ./Src)

aux_source_directory(./Test DIR_SRC)

add_executable(filewatcherpp ${DIR_SRC} ${SOURCES})