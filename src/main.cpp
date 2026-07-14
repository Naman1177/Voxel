#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include "repository.hpp"
#include "FileSystem.hpp"
#include "Hashing.hpp"
#include "Commands.hpp"
#include "ai.hpp"
namespace fs = std::filesystem;
using namespace std;


int main(int argc, char *argv[])
{
    if (argc < 2)
    {

        cout << "\033[1;36m------ VOXEL VERSION CONTROL SYSTEM --------\033[0m\n";
        cout << "Usage: voxel <command> [options]\n\n";
        cout << "Available Commands:\n";
        cout << "  init               Initialize an empty local storage database\n";
        cout << "  commit -m \"msg\"    Save your workspace state manually\n";
        cout << "  commit --ai        Save your workspace state with an AI summary\n";
        cout << "  log                See your visual history tree nodes\n";
        cout << "  status             Scan for untracked files in the workspace\n";
        cout << "  branch <name>      Create a new branch from the current timeline\n";
        cout << "  switch <name>      Switch to a different branch\n";
        cout << "  where              Show the current branch name\n";
        cout << "  track              Track all files in the workspace\n";
        cout << "  graph              Display a visual commit graph of the repository\n";
        return 1;
    }

    string command = argv[1];
    if (command == "init"){
        
        Repository::init_repository();
    }
    else if (command == "status")
    { // 2. Add a status option to test file printing
        std::cout << "\033[1;35m--- Scanning Untracked Workspace Files ---\033[0m\n";

        std::vector<std::string> files = FileSystem::list_workspace_files();
        std::unordered_map<std::string, std::string> index_map = FileSystem::read_index();

        if (files.empty())
        {
            std::cout << "Workspace is completely empty (or contains no trackable files).\n";
        }
        else
        {
            for (const auto &file : files)
            {
                std::string content = FileSystem::read_file_to_string(file);
                std::string file_hash = Hashing::generate_sha256(content);
                if (index_map.find(file) == index_map.end())
                {
                    // File is not tracked at all
                    std::cout << "  \033[1;31m[Untracked]\033[0m " << file << "\n";
                }
                else if (index_map[file] != file_hash)
                {
                    // File is tracked, but the code content has changed!
                    std::cout << "  \033[1;33m[Modified]\033[0m  " << file << "\n";
                }
                else
                {
                    // File is tracked and matches the staging area database perfectly
                    std::cout << "  \033[1;32m[Tracked]\033[0m    " << file << "\n";
                }

                std::cout << "    Hash: " << file_hash << "\n";
            }
        }
    }
    else if (command == "commit"){
        std::string message;

        if (argc == 2)
        {
            std::cout << "\033[1;36m┌───[ Voxel Commit Message Prompt ]───┐\033[0m\n";
            std::cout << "│ Enter commit message: ";

            getline(std::cin, message);
            cout << "\033[1;36m+-------------------------------------+\033[0m\n";

            if (message.empty())
            {
                std::cerr << "\033[31mError: Commit aborted due to empty message.\033[0m\n";
                return 1;
            }
        }

        else if (argc == 3){
            if (string(argv[2]) == "ai") {
                ai::commit_with_ai();
                return 0;
            }
            message = argv[2];
        }
        else{
            cerr << "\033[31mError: Invalid commit command usage. Use 'voxel commit -m \"message\"' or 'voxel commit ai'.\033[0m\n";
        }

        Commands::commit_changes(message);
        Commands::clear_snapshot_silent();
    }
    else if (command == "where")
    {
        string current_branch = Commands::get_current_branch_name();
        cout << "\033[1;36mCurrent Branch: " << current_branch << "\033[0m\n";
    }
    else if (command == "branch")
    {
        std::string branch_name;

        if (argc == 2)
        {
            std::cout << "\033[1;36m+---[ Voxel Branch Name Prompt ]---+\033[0m\n";
            std::cout << "| Enter Branch name: ";

            std::getline(std::cin, branch_name);
            std::cout << "\033[1;36m+----------------------------------+\033[0m\n";

            if (branch_name.empty())
            {
                std::cerr << "\033[31mError: Branch creation aborted. Name cannot be empty.\033[0m\n";
                return 1;
            }

            if (branch_name.find(' ') != std::string::npos)
            {
                std::cerr << "\033[31mError: Branch name cannot contain spaces.\033[0m\n";
                return 1;
            }
        }
        else if (argc >= 3)
        {
            branch_name = argv[2];
        }

        Commands::create_branch(branch_name);
    }
    else if (command == "switch")
    {
        if (argc < 3)
        {
            std::cerr << "\033[31mError: Missing branch name.\nUsage: voxel switch <branch_name>\033[0m\n";
            return 1;
        }
        std::string target_branch = argv[2];
        Commands::switch_branch(target_branch);
    }
    else if (command == "track")
    {
        Commands::track_all_files();
    }
    else if (command == "log")
    {
        Commands::display_basic_log();
    }
    else if (command == "graph")
    {
        Commands::display_graph_log();
    }
    else if (command == "export")
    {
        Commands::export_repository_pdf();
    }
    else if (command == "restore") {
        std::string expression = "";
        if (argc > 2) {
            expression = argv[2]; // Capture options like: exp2/exp-3, head, or a hash
        }
        Commands::restore_workspace_state(expression);
        Commands::clear_snapshot_silent();
        Commands::create_snapshot();
    }
    else if(command == "snapshot"){
        Commands::create_snapshot();
    }
    else if(command == "snapback"){
        Commands::restore_snapshot();
    }
    else if(command == "snapclear"){
        Commands::clear_snapshot_silent();
        cout << "\033[1;32m Last Snapshot cleared successfully. You may now make another snapshot.\033[0m\n";
    }
    else if (command == "login") {
        ai::init_ai();
    }
    else if(command == "review"){
        vector<string> target;
        string optional_note = "";

        if (argc == 2){
            target.push_back(FileSystem::get_current_active_file());
        }
        else if (argc == 3){
            string arg2 = argv[2];
            if (arg2 == "all") {
                target = FileSystem::get_all_files_in_repo();
            }
            else if (fs::exists(arg2)) {
                target.push_back(arg2);
            }
            else {
                target.push_back(FileSystem::get_current_active_file());
                optional_note = arg2;
            }
        }
        else if(argc == 4){
            string arg2 = argv[2];
            string arg3 = argv[3];
            if (arg2 == "all") {
                target = FileSystem::get_all_files_in_repo();
            }
            else if (fs::exists(arg2)) {
                target.push_back(arg2);
            }
            else {
                std::cout << "Error: Target file '" << arg2 << "' not found.\n";
                return 1;
            }
        }
        
        ai::execute_voxel_review(target, optional_note);
            
    }
    
    
    else
    {
        cout << "Error: Unknown command '" << command << "'\n";
        return 1;
    }

    return 0;
}

    
