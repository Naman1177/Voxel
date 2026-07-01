#include "FileSystem.hpp"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;
std::vector<std::string> FileSystem::list_workspace_files() {
    std::vector<std::string> file_list;
    fs::path current_dir = "."; 

    try {
        for (const auto& entry : fs::recursive_directory_iterator(current_dir)) {
            if (fs::is_regular_file(entry.path())) {
                
                // 💡 TRICK: Make the path relative to the current folder to strip the "./"
                fs::path relative_path = entry.path().lexically_relative(current_dir);
                std::string path_str = relative_path.string();

                // Skip the hidden .voxel database and system garbage
                if (path_str.find(".voxel") != std::string::npos || 
                    path_str.find("voxel") == 0 || 
                    path_str.find(".DS_Store") != std::string::npos) {
                    continue;
                }

                file_list.push_back(path_str);
            }
        }
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Directory Scan Error: " << e.what() << "\n";
    }

    return file_list;
}

std::string FileSystem::read_file_to_string(const std::string& path) {
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        return "";
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf(); // Read entire file buffer directly into the stream
    return buffer.str();
}
std::unordered_map<std::string, std::string> FileSystem::read_index() {
    std::unordered_map<std::string, std::string> index_map;
    std::ifstream index_file(".voxel/index");
    
    // If no index file exists yet, simply return an empty map
    if (!index_file.is_open()) {
        return index_map; 
    }

    std::string filename, file_hash;
    // Loop through the file line by line extracting the text blocks
    while (index_file >> filename >> file_hash) {
        index_map[filename] = file_hash;
    }

    index_file.close();
    return index_map;
}
