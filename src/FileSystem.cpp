#include "FileSystem.hpp"
#include "Commands.hpp"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_set>
#include <regex>


namespace fs = std::filesystem;
using namespace std;
struct IgnoreRule {
    std::regex pattern;
    bool is_negated;
    bool directory_only;
    long long expires_at;
};
class VoxelIgnore {
private:
    std::vector<IgnoreRule> rules;

    std::string glob_to_regex(std::string glob) {
        std::string regex_str = "";
        
        // Handle Leading Slash (Root Anchor)
        if (!glob.empty() && glob[0] == '/') {
            glob = glob.substr(1);
            regex_str += "^"; 
        } else {
            regex_str += "(?:^|/)"; // Can match anywhere in the path
        }

        for (size_t i = 0; i < glob.length(); ++i) {
            char c = glob[i];
            if (c == '*' && i + 1 < glob.length() && glob[i+1] == '*') {
                regex_str += ".*"; 
                i++;
            } else if (c == '*') {
                regex_str += "[^/]*"; 
            } else if (c == '?') {
                regex_str += "[^/]";
            } else if (c == '.') {
                regex_str += "\\."; 
            } else {
                regex_str += c;
            }
        }
        regex_str += "$";
        return regex_str;
    }

public:
    VoxelIgnore() {
        if (!fs::exists(".voxelignore")) return;

        std::ifstream file(".voxelignore");
        std::string line;
        
        
        auto now = std::chrono::system_clock::now();
        long long current_time = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

        while (std::getline(file, line)) {
           
            line.erase(0, line.find_first_not_of(" \t\r\n"));
            line.erase(line.find_last_not_of(" \t\r\n") + 1);

            if (line.empty() || line[0] == '#') continue;

            IgnoreRule rule;
            rule.is_negated = false;
            rule.directory_only = false;
            rule.expires_at = 0;

           
            std::string ttl_prefix = "[expires:";
            if (line.find(ttl_prefix) == 0) {
                size_t end_bracket = line.find(']');
                if (end_bracket != std::string::npos) {
                    std::string ts_str = line.substr(ttl_prefix.length(), end_bracket - ttl_prefix.length());
                    try {
                        rule.expires_at = std::stoll(ts_str);
                    } catch (...) {}
                    
                   
                    line = line.substr(end_bracket + 1);
                    line.erase(0, line.find_first_not_of(" \t"));
                }
            }

            if (rule.expires_at > 0 && current_time > rule.expires_at) {
                continue; 
            }

           
            if (line[0] == '!') {
                rule.is_negated = true;
                line = line.substr(1);
            }

            
            if (line.back() == '/') {
                rule.directory_only = true;
                line.pop_back();
            }

            
            try {
                rule.pattern = std::regex(glob_to_regex(line));
                rules.push_back(rule);
            } catch (const std::regex_error& e) {
                std::cerr << "\033[1;33m[VoxelIgnore] Syntax Error in pattern: " << line << "\033[0m\n";
            }
        }
    }

    bool is_ignored(const std::string& path_str, bool is_directory) {
        bool ignored = false;
        
        
        std::string normalized_path = path_str;
        if (normalized_path.length() >= 2 && normalized_path.substr(0, 2) == "./") {
            normalized_path = normalized_path.substr(2);
        }

        
        for (const auto& rule : rules) {
            if (rule.directory_only && !is_directory) continue;

            if (std::regex_search(normalized_path, rule.pattern)) {
                ignored = !rule.is_negated; 
            }
        }
        return ignored;
    }
};
std::vector<std::string> FileSystem::list_workspace_files() {
    std::vector<std::string> files;
    std::string current_dir = ".";
    
    
    VoxelIgnore ignore_engine;

    try {
        for (auto it = fs::recursive_directory_iterator(current_dir); it != fs::recursive_directory_iterator(); ++it) {
            std::string path_str = it->path().string();
            std::string filename = it->path().filename().string();
            bool is_dir = it->is_directory();

           
            if (is_dir) {
                
                if (filename == ".voxel" || 
                    filename == ".git" || 
                    filename == "sandbox_merge") 
                {
                    it.disable_recursion_pending(); 
                    continue;
                }
            } else {
                
                if (filename == ".env" || 
                    filename == ".DS_Store" || 
                    filename == ".voxelignore" || 
                    path_str == "voxel" || 
                    path_str == "./voxel" || 
                    filename.find(".vxlpack") != std::string::npos) 
                {
                    continue;
                }
            }

       
            if (ignore_engine.is_ignored(path_str, is_dir)) {
                if (is_dir) {
                   
                    it.disable_recursion_pending(); 
                }
                continue;
            }

            
            if (it->is_regular_file()) {
                
                if (path_str.length() >= 2 && path_str.substr(0, 2) == "./") {
                    path_str = path_str.substr(2);
                }
                files.push_back(path_str);
            }
        }
    } catch (const fs::filesystem_error& e) {
        std::cerr << "\033[1;31m[FileSystem] Error scanning workspace: " << e.what() << "\033[0m\n";
    }
    
    return files;
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

   
    if (!index_file.is_open())
    {
        return index_map;
    }

    std::string filename, file_hash;

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

