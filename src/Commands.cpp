#include "Commands.hpp"
#include "FileSystem.hpp"

#include "Hashing.hpp"

#include <iostream>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <vector>
using namespace std;
namespace fs = std::filesystem;

string get_current_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

void Commands::track_all_files(){
    string index_path = ".voxel/index";
    vector<string> files = FileSystem::list_workspace_files();
    ofstream index_file(index_path, ios::trunc);
    if (!index_file.is_open()) {
        cerr << "\033[31mError: Could not open or create the database index map.\033[0m\n";
        return;
    }
    cout << "\033[36m--- Staging Workspace State to Index ---\033[0m\n";
    int tracked_count = 0;
    for (const auto& file : files) {
        string content = FileSystem::read_file_to_string(file);
        string file_hash = Hashing::generate_sha256(content);
        index_file << file << " " << file_hash << "\n";
        cout << "  Tracked: " << file << " -> [" << file_hash.substr(0, 8) << "...]\n";
        tracked_count++;
    }
    index_file.close();
    cout << "\033[32m--- Staging Complete: " << tracked_count << " files tracked to index ---\033[0m\n";

}

void Commands::commit_changes(const string& message){
    if(!fs::exists(".voxel/index")){
        cerr << "\033[31mError: No tracked files found. Please run 'voxel track' before committing.\033[0m\n";
        return;
    }
    string index_content = FileSystem::read_file_to_string(".voxel/index");
    if(index_content.empty()){
        cout << "\033[31mError: Tracking area is empty. Nothing to commit.\033[0m\n";
        return;
    }
    string tree_hash = Hashing::generate_sha256(index_content);
    string tree_object_path = ".voxel/object" + tree_hash;
    if(!fs::exists(tree_object_path)){
        ofstream tree_file(tree_object_path);
        tree_file << index_content;
        tree_file.close();
    }

    string parent_hash = "0000000000000000000000000000000000000000000000000000000000000000"; // default zero string
    string main_branch_path = ".voxel/refs/heads/main";
    if(fs::exists(main_branch_path)){
        string saved_parent = FileSystem::read_file_to_string(main_branch_path);
        if(!saved_parent.empty()){
            parent_hash = saved_parent;
        }   
    }
    string author = "Naman Malhotra";
    string timestamp = get_current_timestamp();
    stringstream commit_stream;
    commit_stream << "tree - " << tree_hash << "\n";
    commit_stream << "parent - " << parent_hash << "\n";
    commit_stream << "author - " << author << "\n";
    commit_stream << "timestamp - " << timestamp << "\n\n";
    commit_stream << message << "\n";
    string commit_content = commit_stream.str();
    string commit_hash = Hashing::generate_sha256(commit_content);
    string commit_object_path = ".voxel/objects/" + commit_hash;
    ofstream commit_file(commit_object_path);
    commit_file << commit_content;
    commit_file.close();
    ofstream main_branch_file(main_branch_path, ios::trunc);
    main_branch_file << commit_hash;
    main_branch_file.close();
    cout << "\033[1;32mCommit successful!\033[0m\n";
    cout << "  Commit ID: [" << commit_hash.substr(0, 8) << "...]\n";
    cout << "  Message:   " << message << "\n";

}
void Commands::create_branch(const string& branch_name){
    if (branch_name.empty() || branch_name.find(' ') != std::string::npos) {
        std::cout << "\033[31mError: Branch name cannot be empty or contain spaces.\033[0m\n";
        return;
    }

    std::string current_commit = "";
    std::string main_branch_path = ".voxel/refs/heads/main";

    // 2. Discover our current position by reading the main branch head
    if (fs::exists(main_branch_path)) {
        current_commit = FileSystem::read_file_to_string(main_branch_path);
    }

    if (current_commit.empty()) {
        std::cout << "\033[31mError: Cannot create branch. You must make at least one commit on 'main' first.\033[0m\n";
        return;
    }

    // 3. Create the new branch pointer file inside refs/heads/
    std::string new_branch_path = ".voxel/refs/heads/" + branch_name;
    
    if (fs::exists(new_branch_path)) {
        std::cout << "\033[31mError: A branch named '" << branch_name << "' already exists.\033[0m\n";
        return;
    }

    std::ofstream new_branch_file(new_branch_path);
    new_branch_file << current_commit;
    new_branch_file.close();

    std::cout << "\033[1;32mBranch '" << branch_name << "' successfully created at commit [" << current_commit.substr(0, 8) << "...]!\033[0m\n";

}





