
include_directories(${ROOT_DIR})

if(UNIX)
   set( platform_src "${ROOT_DIR}/platform/Platform_Unix.cpp" )
else()
   set( platform_src "${ROOT_DIR}/platform/Platform_Windows.cpp" )
endif()

add_executable (Tsc_test Tsc_test.cpp ${platform_src} ${ROOT_DIR}/common/Utils.cpp)
target_compile_definitions( Tsc_test PUBLIC HOP_ENABLED )

add_executable (Pid_test Pid_test.cpp ${platform_src} )

add_test (NAME TscTest COMMAND Tsc_test)
add_test (NAME PidTest COMMAND Pid_test)