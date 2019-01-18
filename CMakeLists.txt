cmake_minimum_required(VERSION 2.6.2)
project(indexer)

set (COMPILE_FLAGS "-O3 -Wall -funsigned-char -std=c++14 -fno-omit-frame-pointer -pedantic")
add_definitions(${COMPILE_FLAGS})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=c++14")
find_package( Boost 1.64 COMPONENTS date_time filesystem system thread REQUIRED )
find_package( Threads )

include_directories("./libs/crow/include")

file(GLOB_RECURSE indexer_SOURCES "./src/**.cpp")
add_executable(indexer ${indexer_SOURCES})
target_link_libraries(indexer ${Boost_LIBRARIES})
target_link_libraries(indexer ${CMAKE_THREAD_LIBS_INIT})

install (TARGETS indexer DESTINATION bin)
