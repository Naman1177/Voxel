#include "repository.hpp"
#include <iostream>
#include <filesystem>
#include <fstream>
#include "ai.hpp"
#include "Commands.hpp"

namespace fs = std::filesystem;

bool Repository::init_repository(){

    std::string root_dir = ".voxel";
    std::string objects_dir = root_dir + "/objects";
    std::string refs_dir = root_dir + "/refs/heads";
    std::string snapshot_dir = root_dir + "/snapshot";
    Commands::setup_global_identity();
    if (fs::exists(root_dir)) {
        std::cout << "\033[31mError: A Voxel repository already exists in this directory.\033[0m\n";
        return false;
    }
    int x = 0;
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
        if (!fs::exists(ignore_path)) {
        std::ofstream default_ignore(ignore_path);
            if (default_ignore.is_open()) {
                default_ignore << "# ==================================================================\n"
                               << "#                VOXEL VERSION CONTROL IGNORE SYSTEM\n"
                               << "# ==================================================================\n"
                               << "# Lines starting with '#' are comments.\n"
                               << "# Directly Ignores file names, directories, or patterns.\n"
                               << "#\n"
                               << "# --- Supported Syntax ---\n"
                               << "# 1. Wildcards:      *.log          (Ignores all .log files)\n"
                               << "#                    **/*.tmp       (Ignores .tmp files in any directory)\n"
                               << "# 2. Directories:    build/         (Ignores the 'build' directory)\n"
                               << "# 3. Root Anchoring: /config.json   (Ignores config.json only at the root)\n"
                               << "# 4. Negation:       !main.cpp      (Forces tracking of main.cpp)\n"
                               << "# 4. File:           main.cpp       (Simply ignores main.cpp)\n"
                               << "#\n"
                               << "# --- Advanced Features ---\n"
                               << "# 5. Time-To-Live (TTL) Ignores:\n"
                               << "#    Use [expires:UNIX_TIMESTAMP] to ignore a file/folder temporarily.\n"
                               << "#    Once the system clock passes the timestamp, tracking resumes.\n"
                               << "#    Example: [expires:1786644500] temp_cache/\n"
                               << "#    (Hint: Generate a current timestamp in terminal using 'date +%s')\n"
                               << "#\n"
                               << "# Note: .env, .git, .DS_Store, and .vxlpack are hardcoded and always ignored.\n\n";
                default_ignore.close();
                x = 1;
                
            }
         
        }

        std::cout << "\033[32mInitialized empty Voxel repository inside .voxel/\033[0m\n";
        if (x == 1) {
            std::cout << "\033[36mGenerated default template settings rules inside .voxelignore\033[0m\n";
        }
        
        bool config_status = ai::create_default_config();
        return true;
    }
    catch (const fs::filesystem_error& e) {
        std::cerr << "\033[31mError: " << e.what() << "\033[0m\n";
        return false;
    }
}
