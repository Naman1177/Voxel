#include "FileSystem.hpp"
#include "Commands.hpp"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_set>

namespace fs = std::filesystem;
using namespace std;
std::unordered_set<std::string> load_voxelignore()
{
    std::unordered_set<std::string> ignore_set;
    std::string ignore_file_path = ".voxelignore";

    if (!fs::exists(ignore_file_path))
    {
        return ignore_set; // Return empty set if file doesn't exist yet
    }

    std::ifstream file(ignore_file_path);
    std::string line;

    while (std::getline(file, line))
    {
        // 1. Trim leading/trailing whitespace
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);

        // 2. Ignore empty lines and comments
        if (line.empty() || line[0] == '#')
        {
            continue;
        }

        // 3. Normalize: strip trailing slashes for directory matching consistency (e.g., "node_modules/" -> "node_modules")
        if (line.back() == '/' || line.back() == '\\')
        {
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
        for (auto it = fs::recursive_directory_iterator(current_dir); it != fs::recursive_directory_iterator(); ++it)
        {
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
                if (user_ignores.count(item_name) || user_ignores.count(path_str))
                {
                    it.disable_recursion_pending(); // Block kernel from entering!
                    continue;
                }
            }

            // 2. Process physical target files cleanly
            if (it->is_regular_file())
            {

                // 🔒 SAFE PROTECTION: Exact match for the binary, substring match for garbage artifacts
                if (path_str == "voxel" || path_str == "./voxel" || path_str == ".voxelignore" ||
                    path_str.find(".DS_Store") != std::string::npos || path_str == ".env")
                {
                    continue;
                }
                if (user_ignores.count(item_name) ||
                    user_ignores.count(file_ext) ||
                    user_ignores.count(path_str))
                {
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

string FileSystem::get_current_active_file()
{
    string latest_path = "";
    auto latest_time = fs::file_time_type::min();

    try
    {
        for (const auto &entry : fs::recursive_directory_iterator(fs::current_path()))
        {
            if (entry.is_regular_file())
            {
                std::string path_str = entry.path().string();

                if (path_str.find("/.") != std::string::npos || path_str.find("\\.") != std::string::npos)
                {
                    continue;
                }
                auto write_time = fs::last_write_time(entry);
                if (write_time > latest_time)
                {
                    latest_time = write_time;
                    latest_path = path_str;
                }
            }
        }
    }
    catch (...)
    {
        return "";
    }
    return latest_path;
}

vector<string> FileSystem::get_all_files_in_repo()
{
    vector<string> all_files;
    unordered_set<std::string> ignore_set = load_voxelignore();
    ignore_set.insert(".git");
    ignore_set.insert(".voxel");
    ignore_set.insert(".vscode");
    ignore_set.insert(".DS_Store");
    ignore_set.insert(".env");
    ignore_set.insert(".voxelignore");
    ignore_set.insert("voxel");
    try
    {
        // 2. Begin scanning the current directory recursively
        for (auto it = fs::recursive_directory_iterator("."); it != fs::recursive_directory_iterator(); ++it)
        {
            fs::path file_path = it->path();
            std::string current_path = file_path.string();

            // Clean up the string by removing the leading "./" prefix if present
            if (current_path.substr(0, 2) == "./")
            {
                current_path = current_path.substr(2);
            }

            // 3. Check if the current file path matches any pattern in the ignore set
            bool is_ignored = false;
            for (const auto &pattern : ignore_set)
            {
                if (current_path.find(pattern) != std::string::npos)
                {
                    is_ignored = true;
                    break;
                }
            }

            if (is_ignored)
            {

                if (it->is_directory())
                {
                    it.disable_recursion_pending();
                }
                continue; // Skip processing this path entirely
            }

            // 4. Verify it's a regular file and pass its extension to your custom check
            if (fs::is_regular_file(it->status()))
            {
                std::string extension = file_path.extension().string();

                if (!Commands::should_ignore_extension(extension))
                {
                    all_files.push_back(current_path);
                }
            }
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "\033[1;31mError crawling workspace: " << e.what() << "\033[0m\n";
    }

    return all_files;
}