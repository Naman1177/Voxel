#include <iostream>
#include <vector>
#include <string>
#include "repository.hpp"
#include "FileSystem.hpp"
#include "Hashing.hpp"
#include "Commands.hpp"
using namespace std;

int main(int argc, char* argv[]) {
    if(argc < 2) {
        
        cout << "\033[1;36m------ VOXEL VERSION CONTROL SYSTEM --------\033[0m\n";
        cout << "Usage: voxel <command> [options]\n\n";
        cout << "Available Commands:\n";
        cout << "  init               Initialize an empty local storage database\n";
        cout << "  commit -m \"msg\"    Save your workspace state manually\n";
        cout << "  commit --ai        Save your workspace state with an AI summary\n";
        cout << "  log                See your visual history tree nodes\n";
        cout << "  status             Scan for untracked files in the workspace\n";
        return 1;
    }

    string command = argv[1];
    if(command == "init") {
        Repository::init_repository();
        
    } 
    else if (command == "status") { // 2. Add a status option to test file printing
        std::cout << "\033[1;35m--- Scanning Untracked Workspace Files ---\033[0m\n";
        
        std::vector<std::string> files = FileSystem::list_workspace_files();
        std::unordered_map<std::string, std::string> index_map = FileSystem::read_index();
        
        if (files.empty()) {
            std::cout << "Workspace is completely empty (or contains no trackable files).\n";
        } else {
            for (const auto& file : files) {
                std::string content = FileSystem::read_file_to_string(file);
                std::string file_hash = Hashing::generate_sha256(content);
                if (index_map.find(file) == index_map.end()) {
                    // File is not tracked at all
                    std::cout << "  \033[1;31m[Untracked]\033[0m " << file << "\n";
                } 
                else if (index_map[file] != file_hash) {
                    // File is tracked, but the code content has changed!
                    std::cout << "  \033[1;33m[Modified]\033[0m  " << file << "\n";
                } 
                else {
                    // File is tracked and matches the staging area database perfectly
                    std::cout << "  \033[1;32m[Staged]\033[0m    " << file << "\n";
                }
                
                std::cout << "    Hash: " << file_hash << "\n";
            }
        }
    }
    else if (command == "track") { 
        Commands::track_all_files();
    }
    else if (command == "commit") {
        if (argc < 3) {
            cout << "Error: Commit message is required.\n";
            return 1;
        }
        string message = argv[2];
        cout << "Committing changes with message: " << message << "\n";
        
    } else if (command == "log") {
        cout << "Displaying visual history tree nodes...\n";
        
    } else {
        cout << "Error: Unknown command '" << command << "'\n";
        return 1;
    }














    return 0;
}