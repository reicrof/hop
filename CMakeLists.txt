cmake_minimum_required (VERSION 3.0)
project (hop)

set( CMAKE_VERBOSE_MAKEFILE on )

# includes cmake/FindSDL2.cmake
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)
find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)

set( default_build_type "Release" )
set( CMAKE_CXX_STANDARD 14 )
set(ROOT_DIR ${CMAKE_SOURCE_DIR})

# Option
option( USE_OS_FILE_DIALOG "Use the OS provided file dialog" ON )

#####################
### Platform specific
#####################
if(UNIX)
   set( PLATFORM_LINK_FLAGS "-lpthread -ldl" )
   set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic -ffast-math -fno-exceptions -fno-rtti -Wstrict-aliasing=2 " )
   if(APPLE)
        set( CMAKE_OSX_DEPLOYMENT_TARGET 10.11 )
        set( NOC_IMPL_FILE "noc_file_dialog/noc_file_dialog.mm")
        set( NOC_IMPL_DEFINE "NOC_FILE_DIALOG_OSX" )
   else()
      set( PLATFORM_LINK_FLAGS "${PLATFORM_LINK_FLAGS} -lrt" )

      if( USE_OS_FILE_DIALOG )
         find_package( PkgConfig REQUIRED )
         pkg_check_modules( GTK3 REQUIRED gtk+-3.0 )
         if( GTK3_FOUND )
            set( NOC_IMPL_FILE "noc_file_dialog/noc_file_dialog.cpp")
            set( NOC_IMPL_DEFINE "NOC_FILE_DIALOG_GTK" )
            set( PLATFORM_EXTRA_INCLUDES ${GTK3_INCLUDE_DIRS} )
            set( PLATFORM_LINK_FLAGS "${PLATFORM_LINK_FLAGS} -l${GTK3_LIBRARIES}" )
         endif()
      endif()
   endif()   
else() # Windows
   set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /fp:fast" )
   set( HOP_WRAP_SRC wrap_library/hop_wrap_windows.cpp )
   set( NOC_IMPL_FILE "noc_file_dialog/noc_file_dialog.cpp")
   set( NOC_IMPL_DEFINE "NOC_FILE_DIALOG_WIN32" )
endif()

#####################
### Asan
#####################
SET(SAN CACHE STRING "Enable Specified Sanitizer")
if(SAN)
   set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer -fsanitize=${SAN}")
   set (CMAKE_LINKER_FLAGS "${CMAKE_STATIC_LINKER_FLAGS} -fno-omit-frame-pointer -fsanitize=${SAN}")
endif()

#####################
### HOP common
#####################
if(UNIX)
   set( viewer_platform_src "common/platform/Platform_Unix.cpp" )
else()
   set( viewer_platform_src "common/platform/Platform_Windows.cpp" )
endif()
file( GLOB common_src "common/*.cpp" "common/*.c" )
set( common_src ${common_src} ${viewer_platform_src} )

#####################
### HOP GUI
#####################
file(GLOB hop_gui_src
    "hop/*.cpp"
    "thirdparties/imgui/*.cpp"
)

if( USE_OS_FILE_DIALOG )
   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DHOP_USE_FILE_DIALOG=1")
   list(APPEND hop_gui_src "thirdparties/${NOC_IMPL_FILE}" )
else()
   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DHOP_USE_FILE_DIALOG=0")
endif()

add_executable(hop ${common_src} ${hop_gui_src} )
target_compile_definitions( hop PUBLIC HOP_ENABLED )

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DHOP_DEBUG")

target_compile_definitions( hop PUBLIC HOP_ENABLED ${NOC_IMPL_DEFINE} )
target_include_directories( hop PRIVATE ${ROOT_DIR} ${ROOT_DIR}/thirdparties ${OPENGL_INCLUDE_DIRS} ${SDL2_INCLUDE_DIR} ${PLATFORM_EXTRA_INCLUDES} )
target_link_libraries( hop PUBLIC ${PLATFORM_LINK_FLAGS} ${HOP_WINDOWS_LIB} ${OPENGL_LIBRARIES} ${SDL2_LIBRARY} )

#####################
### HOP CLI
#####################
add_executable( hopcli hopcli/hopCLI.cpp  ${common_src} )
target_compile_definitions( hopcli PUBLIC HOP_ENABLED )
target_include_directories( hopcli PRIVATE ${ROOT_DIR} )
target_link_libraries( hopcli PUBLIC ${PLATFORM_LINK_FLAGS} )

#####################
### Test clients
#####################
add_subdirectory(test_clients)

#####################
### Test executables
#####################
enable_testing ()
add_subdirectory (tests)
