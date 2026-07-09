#include "repository.hpp"
#include <iostream>
#include <filesystem>
#include <fstream>
#include "ai.hpp"

namespace fs = std::filesystem;

bool Repository::init_repository(){
    std::string root_dir = ".voxel";
    std::string objects_dir = root_dir + "/objects";
    std::string refs_dir = root_dir + "/refs/heads";
    std::string snapshot_dir = root_dir + "/snapshot";
    if (fs::exists(root_dir)) {
        std::cout << "\033[31mError: A Voxel repository already exists in this directory.\033[0m\n";
        return false;
    }

    try {
        
        fs::create_directory(root_dir);
        fs::create_directories(objects_dir);
        fs::create_directories(refs_dir);
        fs::create_directories(snapshot_dir);
        std::ofstream head_file(root_dir + "/HEAD");
        if (head_file.is_open()) {
            head_file << "ref: refs/heads/main\n";
            head_file.close();
        }
        std::string ignore_path = ".voxelignore";
    
        // Only generate the file if the user doesn't already have one
        if (!fs::exists(ignore_path)) {
        std::ofstream default_ignore(ignore_path);
            if (default_ignore.is_open()) {
                default_ignore << "# =========================================================================\n"
                            << "#                   VOXEL VERSION CONTROL IGNORE SYSTEM                    \n"
                            << "# =========================================================================\n"
                            << "# Lines starting with '#' are comments and are ignored logically.\n"
                            << "# You can ignore raw folder names, exact paths, or file extensions.\n\n";
                            
                           
                default_ignore.close();
                std::cout << "\033[36mGenerated default template settings rules inside .voxelignore\033[0m\n";
            }
         
        }

        std::cout << "\033[32mInitialized empty Voxel repository inside .voxel/\033[0m\n";
        bool config_status = ai::create_default_config();
        return true;
    }
    catch (const fs::filesystem_error& e) {
        std::cerr << "\033[31mError: " << e.what() << "\033[0m\n";
        return false;
    }
}
