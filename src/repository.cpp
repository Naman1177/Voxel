#include "repository.hpp"
#include <iostream>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

bool Repository::init_repository(){
    std::string root_dir = ".voxel";
    std::string objects_dir = root_dir + "/objects";
    std::string refs_dir = root_dir + "/refs/heads";
    if (fs::exists(root_dir)) {
        std::cout << "\033[31mError: A Voxel repository already exists in this directory.\033[0m\n";
        return false;
    }

    try {
        
        fs::create_directory(root_dir);
        fs::create_directories(objects_dir);
        fs::create_directories(refs_dir);
        std::ofstream head_file(root_dir + "/HEAD");
        if (head_file.is_open()) {
            head_file << "ref: refs/heads/main\n";
            head_file.close();
        }

        std::cout << "\033[32mInitialized empty Voxel repository inside .voxel/\033[0m\n";
        return true;
    }
    catch (const fs::filesystem_error& e) {
        std::cerr << "\033[31mError: " << e.what() << "\033[0m\n";
        return false;
    }
}
