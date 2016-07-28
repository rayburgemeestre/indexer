cmake_minimum_required(VERSION 2.6.2)
project(indexer)

set (COMPILE_FLAGS "-O3 -Wall -funsigned-char -std=c++1y -fno-omit-frame-pointer -pedantic")
add_definitions(${COMPILE_FLAGS})

if (MSVC)
set(Boost_USE_STATIC_LIBS "On")
find_package( Boost 1.52 COMPONENTS date_time filesystem system thread regex REQUIRED )
else()
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=c++1y")
find_package( Boost 1.52 COMPONENTS date_time filesystem system thread REQUIRED )
endif()

include_directories("./libs/crow/include")

file(GLOB_RECURSE indexer_SOURCES "./src/**.cpp")
add_executable(indexer ${indexer_SOURCES})
target_link_libraries(indexer ${Boost_LIBRARIES})

install (TARGETS indexer DESTINATION bin)
