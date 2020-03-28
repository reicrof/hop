
include_directories(${ROOT_DIR})

set( CMAKE_VERBOSE_MAKEFILE on )
set( CMAKE_CXX_STANDARD 14 )

if(UNIX)
   set( platform_src "${ROOT_DIR}/common/platform/Platform_Unix.cpp" )
else()
   set( platform_src "${ROOT_DIR}/common/platform/Platform_Windows.cpp" )
endif()

add_executable (Tsc_test Tsc_test.cpp ${platform_src} ${ROOT_DIR}/common/Utils.cpp)
target_compile_definitions( Tsc_test PUBLIC HOP_ENABLED )

add_executable (Pid_test Pid_test.cpp ${platform_src} )

add_executable (BlockAllocator_test BlockAllocator_test.cpp ${ROOT_DIR}/common/BlockAllocator.cpp ${platform_src} )

add_executable (Deque_test Deque_test.cpp ${ROOT_DIR}/common/BlockAllocator.cpp ${platform_src} )

add_test (NAME TscTest COMMAND Tsc_test)
add_test (NAME PidTest COMMAND Pid_test)
add_test (NAME BlockAllocatorTest COMMAND BlockAllocator_test)
add_test (NAME DequeTest COMMAND Deque_test)