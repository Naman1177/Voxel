# ==========================================
# VOXEL BUILD & INSTALL SYSTEM (Mac & Linux)
# ==========================================

# 1. OS Detection & Compiler Settings
UNAME_S := $(shell uname -s)

# Default to GCC on Linux
ifeq ($(UNAME_S),Linux)
    CXX ?= g++
    CC ?= gcc
endif

# Default to Clang on macOS (Darwin)
ifeq ($(UNAME_S),Darwin)
    CXX ?= clang++
    CC ?= clang
endif

# Compiler Flags
CXXFLAGS = -std=c++17 -Wall -O3 \
           -Wno-c++11-narrowing \
           -Wno-deprecated \
           -Iinc \
           -Ithird_party_lib \
           -Ithird_party_lib/zstd \
           -Ithird_party_lib/mbedtls/inc \
           -Ithird_party_lib/ai_parser \
           -Ithird_party_lib/dtl

CFLAGS = -Wall -O3 \
         -Ithird_party_lib \
         -Ithird_party_lib/zstd \
         -Ithird_party_lib/mbedtls/inc

# 2. Target Executable & Global Path
TARGET = voxel
INSTALL_DIR = /usr/local/bin

# 3. Source Files
CPP_SRCS = $(wildcard src/*.cpp)
C_SRCS = $(wildcard third_party_lib/zstd/common/*.c) \
         $(wildcard third_party_lib/zstd/compress/*.c) \
         $(wildcard third_party_lib/zstd/decompress/*.c) \
         $(wildcard third_party_lib/mbedtls/src/*.c)

# 4. Object Files
CPP_OBJS = $(CPP_SRCS:.cpp=.o)
C_OBJS = $(C_SRCS:.c=.o)
OBJS = $(CPP_OBJS) $(C_OBJS)

# 5. Build Rules
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# 6. Global Install Rule
install: $(TARGET)
	@echo "Installing Voxel to $(INSTALL_DIR)..."
	cp $(TARGET) $(INSTALL_DIR)/$(TARGET)
	chmod +x $(INSTALL_DIR)/$(TARGET)
	@echo "Voxel is now globally accessible! Run 'voxel' from any directory."
	@echo "Removing local binary..."
	rm -f $(TARGET)

# 7. Clean Rule
clean:
	rm -f $(OBJS) $(TARGET)