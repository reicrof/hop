UNAME := $(shell uname)
TARGET_SERVER = multiproc_server
TARGET_CLIENT = multiproc_client
TARGET_PTHREAD_WRAP = vdbg_functions_wrap.so

CXXFLAGS = -std=c++14 -Wall -Wextra -pedantic
ENABLED_WARNINGS = -Wconversion-null -Wempty-body -Wignored-qualifiers -Wsign-compare -Wtype-limits -Wuninitialized
DISABLED_WARNINGS = -Wno-missing-braces

CXXFLAGS += $(ENABLED_WARNINGS) $(DISABLED_WARNINGS)
IMGUI_SOURCES = $(wildcard imgui/*.cpp)
SERVER_SOURCES = $(wildcard *server.cpp) imdbg.cpp $(IMGUI_SOURCES)
CLIENT_SOURCES = main_client.cpp
PTHREAD_WRAP_SOURCES =

COMMON_INCLUDES = -isystem.
CLIENT_DEFINE = -DVDBG_ENABLED

PLATFORM_LD_FLAGS = 
ifeq ($(UNAME), Linux)
   CXX = clang++
   PLATFORM_LD_FLAGS = -lGL
   PTHREAD_WRAP_SOURCES = vdbg_functions_wrap.cpp
else ifeq ($(UNAME), Darwin)
   CXX = clang++
   PLATFORM_LD_FLAGS = -framework OpenGL -framework Quartz -framework Cocoa -Wl,-undefined,dynamic_lookup
endif

#includes
INC = $(COMMON_INCLUDES)
LDFLAGS = -lpthread -lrt ./SDL2/libSDL2.a -ldl $(PLATFORM_LD_FLAGS)

.PHONY: all

# server target
$(TARGET_SERVER): $(SERVER_SOURCES)
	$(CXX) $(CXXFLAGS) $(SERVER_SOURCES) $(INC) $(LDFLAGS) -o $(TARGET_SERVER) -ffast-math -DVDBG_ENABLED

# client target
$(TARGET_CLIENT): $(CLIENT_SOURCES)
	$(CXX) $(CXXFLAGS) $(CLIENT_SOURCES) $(INC) $(LDFLAGS) -o $(TARGET_CLIENT) $(CLIENT_DEFINE) -rdynamic

# pthread wrapper library
$(TARGET_PTHREAD_WRAP): $(PTHREAD_WRAP_SOURCES)
	$(CXX) -std=c++14 -fPIC -shared $(PTHREAD_WRAP_SOURCES) -ldl $(INC) -DVDBG_ENABLED -o $(TARGET_PTHREAD_WRAP) -g

.PHONY : clean
clean:
	find . -type f -name '*.o' -delete
	rm $(TARGET_SERVER) $(TARGET_CLIENT) $(TARGET_PTHREAD_WRAP)

.PHONY: debug
debug :  CXXFLAGS += -g -D_DEBUG -Werror -pg
debug : $(TARGET_SERVER) $(TARGET_CLIENT) $(TARGET_PTHREAD_WRAP)

.PHONY: release
release :   CXXFLAGS += -O3 -DNDEBUG
release : $(TARGET_SERVER) $(TARGET_CLIENT) $(TARGET_PTHREAD_WRAP)

.PHONY: asan
asan :  CXXFLAGS += -g -pg -fsanitize=address -fno-omit-frame-pointer 
asan : $(TARGET_SERVER) $(TARGET_CLIENT) $(TARGET_PTHREAD_WRAP)