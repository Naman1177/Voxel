#pragma once
#include <string>

namespace Zstd {
    bool compress_file(const std::string& src_path, const std::string& dest_path);
    bool decompress_file(const std::string& src_path, const std::string& dest_path);
}