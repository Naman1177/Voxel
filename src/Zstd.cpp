#include "../third_party_lib/zstd/zstd.h"
#include "Zstd.hpp"
#include <vector>
#include <fstream>
#include <iostream>
using namespace std;
bool Zstd::compress_file(const std::string& src_path, const std::string& dest_path) {
    std::ifstream src(src_path, std::ios::binary | std::ios::ate);
    if (!src.is_open()) return false;
    
    std::streamsize src_size = src.tellg();
    src.seekg(0, std::ios::beg);
    
    std::vector<char> src_buf(src_size);
    if (!src.read(src_buf.data(), src_size)) return false;
    src.close();
    
    size_t const max_dst_size = ZSTD_compressBound(src_size);
    std::vector<char> dst_buf(max_dst_size);
    
    size_t const c_size = ZSTD_compress(dst_buf.data(), max_dst_size, src_buf.data(), src_size, 1);
    if (ZSTD_isError(c_size)) {
        std::cerr << "Zstd Compression Error: " << ZSTD_getErrorName(c_size) << "\n";
        return false;
    }
    
    std::ofstream dest(dest_path, std::ios::binary | std::ios::trunc);
    if (!dest.is_open()) return false;
    dest.write(dst_buf.data(), c_size);
    return true;
}
bool Zstd::decompress_file(const std::string& src_path, const std::string& dest_path) {
    std::ifstream src(src_path, std::ios::binary | std::ios::ate);
    if (!src.is_open()) return false;
    
    std::streamsize src_size = src.tellg();
    src.seekg(0, std::ios::beg);
    
    std::vector<char> src_buf(src_size);
    if (!src.read(src_buf.data(), src_size)) return false;
    src.close();
    
    unsigned long long const decomp_size = ZSTD_getFrameContentSize(src_buf.data(), src_size);
    if (decomp_size == ZSTD_CONTENTSIZE_ERROR || decomp_size == ZSTD_CONTENTSIZE_UNKNOWN) {
        return false;
    }
    
    std::vector<char> dst_buf(decomp_size);
    size_t const d_size = ZSTD_decompress(dst_buf.data(), decomp_size, src_buf.data(), src_size);
    if (ZSTD_isError(d_size)) {
        return false;
    }
    
    std::ofstream dest(dest_path, std::ios::binary | std::ios::trunc);
    if (!dest.is_open()) return false;
    dest.write(dst_buf.data(), d_size);
    return true;
}
    