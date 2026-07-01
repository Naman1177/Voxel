#ifndef FILESYSTEM_HPP
#define FILESYSTEM_HPP
#include <unordered_map>
#include <string>
#include <vector>
#include <iostream>

class FileSystem {
public:
    static std::vector<std::string> list_workspace_files();
    static std::string read_file_to_string(const std::string& path);
    static std::unordered_map<std::string, std::string> read_index();

};

#endif // FILESYSTEM_HPP