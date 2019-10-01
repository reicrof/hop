CMAKE_MINIMUM_REQUIRED(VERSION 3.0) 

add_executable(lock_owner "lock_owner.cpp" )
target_include_directories( lock_owner SYSTEM PRIVATE ${ROOT_DIR} )
TARGET_LINK_LIBRARIES( lock_owner PUBLIC -lpthread ${HOP_LIBRT_FLAG} )

add_executable(heavy_rec "heavy_rec.cpp" )
target_include_directories( heavy_rec SYSTEM PRIVATE ${ROOT_DIR} )
TARGET_LINK_LIBRARIES( heavy_rec PUBLIC -lpthread ${HOP_LIBRT_FLAG} )

add_executable(stats_client "stats_client.cpp" )
target_include_directories( stats_client SYSTEM PRIVATE ${ROOT_DIR} )
TARGET_LINK_LIBRARIES( stats_client PUBLIC -lpthread ${HOP_LIBRT_FLAG} )