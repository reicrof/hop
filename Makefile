UNAME := $(shell uname)
TARGET_SERVER = multiproc_server
TARGET_CLIENT = multiproc_client

CXXFLAGS = -std=c++14 -Wall -Werror
IMGUI_SOURCES = $(wildcard imgui/*.cpp)
SERVER_SOURCES = $(wildcard *server.cpp) imdbg.cpp $(IMGUI_SOURCES)
CLIENT_SOURCES = $(wildcard *client.cpp)
COMMON_INCLUDES = -isystem. -lGL

ifeq ($(UNAME), Linux)
   CXX = g++
else ifeq ($(UNAME), Darwin)
   CXX = clang++
endif

#includes
INC = $(COMMON_INCLUDES)
LDFLAGS = -lpthread ./SDL2/libSDL2.a -ldl

.PHONY: all

# server target
$(TARGET_SERVER): $(SERVER_SOURCES)
	$(CXX) $(CXXFLAGS) $(SERVER_SOURCES) $(INC) $(LDFLAGS) -o $(TARGET_SERVER)

# client target
$(TARGET_CLIENT): $(CLIENT_SOURCES)
	$(CXX) $(CXXFLAGS) $(CLIENT_SOURCES) $(INC) $(LDFLAGS) -o $(TARGET_CLIENT)

.PHONY : clean
clean:
	find . -type f -name '*.o' -delete
	rm $(TARGET_SERVER) $(TARGET_CLIENT)


.PHONY: debug
debug :  CXXFLAGS += -g -D_DEBUG
debug : $(TARGET_SERVER) $(TARGET_CLIENT)

.PHONY: release
release :   CXXFLAGS += -O3 -pg -DNDEBUG
release : $(TARGET_SERVER) $(TARGET_CLIENT)