cmake_minimum_required (VERSION 3.0)
project (hop)

#####################
### Platform specific
#####################
if(UNIX)
   if(APPLE)
        set( HOP_WRAP_SRC wrap_library/hop_wrap_macos.cpp )
   else()
        set( HOP_WRAP_SRC wrap_library/hop_wrap_linux.cpp )
        set( HOP_LIBRT_FLAG "-lrt" )
   endif()
   set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic" )
else()
   set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall" )
   set( HOP_WRAP_SRC wrap_library/hop_wrap_windows.cpp )
   set( HOP_WINDOWS_LIB "Shlwapi.lib" ) # Used for StrStrI
endif()

set( default_build_type "Release" )
set( CMAKE_CXX_STANDARD 14 )

# includes cmake/FindSDL2.cmake
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)
find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)

set(CMAKE_CXX_FLAGS "-ffast-math -fno-exceptions -fno-rtti ${CMAKE_CXX_FLAGS}")

#####################
### Asan
#####################
SET(ASAN OFF CACHE BOOL "Enable Address Sanitizer")
if(ASAN)
   set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer -fsanitize=address")
   set (CMAKE_LINKER_FLAGS "${CMAKE_STATIC_LINKER_FLAGS} -fno-omit-frame-pointer -fsanitize=address")
endif()

#####################
### Main executable
#####################
add_definitions( -DHOP_ENABLED )
file(GLOB hop_server_src
    "*.h"
    "*.cpp"
    "*.c"
    "imgui/*.h"
    "imgui/*.cpp"
)
add_executable(hop ${hop_server_src})

target_include_directories( hop SYSTEM PRIVATE  ${OPENGL_INCLUDE_DIRS} ${SDL2_INCLUDE_DIR} )
target_link_libraries( hop PUBLIC -lpthread ${HOP_LIBRT_FLAG} ${HOP_WINDOWS_LIB} ${OPENGL_LIBRARIES} ${SDL2_LIBRARY} -ldl )

#####################
### Library for function wrapping
#####################
set(CMAKE_MACOSX_RPATH 1)
add_library(hop_wrap SHARED ${HOP_WRAP_SRC})
set (CMAKE_SHARED_LINKER_FLAGS "-ldl")

# With clang on MacOs, we need to ignore the missing symbols as they are going to be resolved
# at runtime when linking against the executable that uses hop.
if(${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -undefined dynamic_lookup")
endif()

set_property(TARGET hop_wrap PROPERTY POSITION_INDEPENDENT_CODE ON)

#####################
### Test client
#####################
file(GLOB hop_client_src
    "test_client/*.h"
    "test_client/*.cpp"
)
add_executable(hop_client ${hop_client_src})
target_link_libraries( hop_client PUBLIC -lpthread ${HOP_LIBRT_FLAG} -ldl )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})

set( CMAKE_VERBOSE_MAKEFILE on )

#windows build : cmake -G "Visual Studio 15 2017 Win64" -DSDL2_INCLUDE_DIR:STRING=C:\\Users\\XXXX\\source\\repos\\SDL2\\include -DSDL2_LIBRARY:STRING=C:\\Users\\XXXX\\source\\repos\\SDL2\\lib\\x64\\SDL2.lib .. 