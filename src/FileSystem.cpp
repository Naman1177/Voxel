#include "FileSystem.hpp"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_set>

namespace fs = std::filesystem;
std::unordered_set<std::string> load_voxelignore() {
    std::unordered_set<std::string> ignore_set;
    std::string ignore_file_path = ".voxelignore";

    if (!fs::exists(ignore_file_path)) {
        return ignore_set; // Return empty set if file doesn't exist yet
    }

    std::ifstream file(ignore_file_path);
    std::string line;

    while (std::getline(file, line)) {
        // 1. Trim leading/trailing whitespace
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);

        // 2. Ignore empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }

        // 3. Normalize: strip trailing slashes for directory matching consistency (e.g., "node_modules/" -> "node_modules")
        if (line.back() == '/' || line.back() == '\\') {
            line.pop_back();
        }

        ignore_set.insert(line);
    }
    return ignore_set;
}
std::vector<std::string> FileSystem::list_workspace_files()
{
    std::vector<std::string> file_list;
    fs::path current_dir = ".";
    std::unordered_set<std::string> user_ignores = load_voxelignore();

    try
    {
        // 🔄 Switch to an explicit iterator to control kernel traversal depth
        for (auto it = fs::recursive_directory_iterator(current_dir); it != fs::recursive_directory_iterator(); ++it){
            fs::path relative_path = it->path().lexically_relative(current_dir);
            std::string path_str = relative_path.string();
            std::string item_name = it->path().filename().string();
            std::string file_ext = it->path().extension().string();
            // 1. OPTIMIZATION: Slap a brick wall in front of database subdirectories
            if (it->is_directory())
            {
                std::string dir_name = it->path().filename().string();
                if (dir_name == ".voxel" || dir_name == ".git" || dir_name == ".vscode")
                {
                    it.disable_recursion_pending(); // Tells the OS: Do not open or look inside this folder!
                    continue;
                }
                if (user_ignores.count(item_name) || user_ignores.count(path_str)) {
                    it.disable_recursion_pending(); // Block kernel from entering!
                    continue;
                }
            }


            // 2. Process physical target files cleanly
            if (it->is_regular_file())
            {

                // 🔒 SAFE PROTECTION: Exact match for the binary, substring match for garbage artifacts
                if (path_str == "voxel" || path_str == "./voxel" || path_str == ".voxelignore" ||
                    path_str.find(".DS_Store") != std::string::npos || path_str == ".env"){
                    continue;
                }
                if (user_ignores.count(item_name) || 
                    user_ignores.count(file_ext)  || 
                    user_ignores.count(path_str)) {
                    continue;
                }

                file_list.push_back(path_str);
            }
        }
    }
    catch (const fs::filesystem_error &e)
    {
        std::cerr << "Directory Scan Error: " << e.what() << "\n";
    }

    return file_list;
}

std::string FileSystem::read_file_to_string(const std::string &path)
{
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file.is_open())
    {
        return "";
    }

    std::stringstream buffer;
    buffer << file.rdbuf(); // Read entire file buffer directly into the stream
    return buffer.str();
}
std::unordered_map<std::string, std::string> FileSystem::read_index()
{
    std::unordered_map<std::string, std::string> index_map;
    std::ifstream index_file(".voxel/index");

    // If no index file exists yet, simply return an empty map
    if (!index_file.is_open())
    {
        return index_map;
    }

    std::string filename, file_hash;
    // Loop through the file line by line extracting the text blocks
    while (index_file >> filename >> file_hash)
    {
        index_map[filename] = file_hash;
    }

    index_file.close();
    return index_map;
}
