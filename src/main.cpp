#include <iostream>
#include <vector>
#include <string>
#include "repository.hpp"
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
        return 1;
    }

    string command = argv[1];
    if(command == "init") {
        Repository::init_repository();
        
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