cmake_minimum_required (VERSION 2.6)
project (hop)

# Platform specific
if(UNIX)
   set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -Wall -Wextra -pedantic" )
   set( HOP_WRAP_SRC wrap_library/hop_wrap_unix.cpp )
else()
   set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -Wall" )
   set( HOP_WRAP_SRC wrap_library/hop_wrap_windows.cpp )
endif()


# includes cmake/FindSDL2.cmake
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)
find_package(SDL2 REQUIRED)

#set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g ")
#set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -ffast-math ")

# Mains executable
add_definitions( -DHOP_ENABLED )
file(GLOB hop_server_src
    "*.h"
    "*.cpp"
    "imgui/*.h"
    "imgui/*.cpp"
)
include_directories(${SDL2_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR})
add_executable(hop ${hop_server_src})

target_link_libraries( hop PUBLIC -lpthread -lGL -lrt ${SDL2_LIBRARY} -ldl )

# Library for function wrapping

add_library(hop_wrap SHARED ${HOP_WRAP_SRC})
set (CMAKE_SHARED_LINKER_FLAGS "-ldl")
set_property(TARGET hop_wrap PROPERTY POSITION_INDEPENDENT_CODE ON)

set( CMAKE_VERBOSE_MAKEFILE on )

# Test client
file(GLOB hop_client_src
    "test_client/*.h"
    "test_client/*.cpp"
)
add_executable(hop_client ${hop_client_src})
target_link_libraries( hop_client PUBLIC -lpthread -lrt -ldl )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})

#windows build : cmake -G "Visual Studio 15 2017 Win64" -DSDL2_INCLUDE_DIR:STRING=C:\Users\XXXX\source\repos\SDL2\include -DSDL2_LIBRARY:STRING=C:\Users\XXXX\source\repos\SDL2\lib\x64\SDL2.dll .. 