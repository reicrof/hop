cmake_minimum_required (VERSION 3.0)
project (hop)

#####################
### Platform specific
#####################
if(UNIX)
   if(APPLE)
        set( CMAKE_OSX_DEPLOYMENT_TARGET 10.11 )
   else()
        set( HOP_LIBRT_FLAG "-lrt" )
   endif()
   set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic" )
else()
   set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}" )
   set( HOP_WRAP_SRC wrap_library/hop_wrap_windows.cpp )
endif()

set( default_build_type "Release" )
set( CMAKE_CXX_STANDARD 14 )
set(ROOT_DIR ${CMAKE_SOURCE_DIR})

# includes cmake/FindSDL2.cmake
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)
find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)

set(CMAKE_CXX_FLAGS "-ffast-math -fno-exceptions -fno-rtti ${CMAKE_CXX_FLAGS}")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

set( CMAKE_VERBOSE_MAKEFILE on )

#####################
### Asan
#####################
SET(SAN CACHE STRING "Enable Specified Sanitizer")
if(SAN)
   set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer -fsanitize=${SAN}")
   set (CMAKE_LINKER_FLAGS "${CMAKE_STATIC_LINKER_FLAGS} -fno-omit-frame-pointer -fsanitize=${SAN}")
endif()

#####################
### Main executable
#####################
if(UNIX)
   set( viewer_platform_src "platform/Platform_Unix.cpp" )
else()
   set( viewer_platform_src "platform/Platform_Windows.cpp" )
endif()

add_definitions( -DHOP_ENABLED )
file(GLOB hop_viewer_src
    "*.h"
    "*.cpp"
    "*.c"
    "imgui/*.h"
    "imgui/*.cpp"
)
add_executable(hop ${hop_viewer_src} ${viewer_platform_src})

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DHOP_DEBUG")

target_include_directories( hop SYSTEM PRIVATE ${ROOT_DIR} ${OPENGL_INCLUDE_DIRS} ${SDL2_INCLUDE_DIR} )
target_link_libraries( hop PUBLIC -lpthread ${HOP_LIBRT_FLAG} ${HOP_WINDOWS_LIB} ${OPENGL_LIBRARIES} ${SDL2_LIBRARY} -ldl )

#####################
### Test clients
#####################
add_subdirectory(test_clients)

#####################
### Test executables
#####################

enable_testing ()
add_subdirectory (tests)

#windows build : cmake -G "Visual Studio 15 2017 Win64" -DSDL2_INCLUDE_DIR:STRING=C:\\Users\\XXXX\\source\\repos\\SDL2\\include -DSDL2_LIBRARY:STRING=C:\\Users\\XXXX\\source\\repos\\SDL2\\lib\\x64\\SDL2.lib .. 