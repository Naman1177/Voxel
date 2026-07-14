cmake_minimum_required(VERSION 3.15)
project(Voxel VERSION 1.0.0 LANGUAGES C CXX)

# Force C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. Gather all third-party C source files (Zstd and MbedTLS)
file(GLOB ZSTD_SOURCES 
    "third_party_lib/zstd/common/*.c"
    "third_party_lib/zstd/compress/*.c"
    "third_party_lib/zstd/decompress/*.c"
)
file(GLOB MBEDTLS_SOURCES "third_party_lib/mbedtls/src/*.c")

# 2. Gather Voxel C++ source files
file(GLOB VOXEL_SOURCES "src/*.cpp")

# 3. Define the executable target
add_executable(voxel ${VOXEL_SOURCES} ${ZSTD_SOURCES} ${MBEDTLS_SOURCES})

# 4. Set Include Directories (Replaces your -I flags)
target_include_directories(voxel PRIVATE
    "inc"
    "third_party_lib"
    "third_party_lib/zstd"
    "third_party_lib/mbedtls/inc"
    "third_party_lib/ai_parser"
)

# 5. Compiler Flags (Replaces your -Wno flags)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(voxel PRIVATE 
        -Wno-c++11-narrowing 
        -Wno-deprecated
    )
endif()

# 6. Production Install Rules (Tells Homebrew where to place the binary)
include(GNUInstallDirs)
install(TARGETS voxel DESTINATION ${CMAKE_INSTALL_BINDIR})