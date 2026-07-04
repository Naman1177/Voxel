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
string Commands::get_current_branch_name() {
    if (!fs::exists(".voxel/HEAD")) return "main";
    
    std::ifstream ifs(".voxel/HEAD");
    string line;
    std::getline(ifs, line);
    ifs.close();

    // line will look like: "ref: refs/heads/exp@feat"
    string prefix = "ref: refs/heads/";
    if (line.find(prefix) == 0) {
        return line.substr(prefix.length()); // Returns "exp@feat"
    }
    return "main";
}

void Commands::track_all_files(){
    string index_path = ".voxel/index";
    string objects_dir = ".voxel/objects";
    fs::create_directories(objects_dir);
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
        string object_file_path = objects_dir + "/" + file_hash;
        if (!fs::exists(object_file_path)) {
            ofstream object_file(object_file_path, ios::trunc);
            if (object_file.is_open()) {
                object_file << content; // <─── THIS writes your raw text code into the vault!
                object_file.close();
            }
        }
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
    string tree_object_path = ".voxel/objects/" + tree_hash;
    if(!fs::exists(tree_object_path)){
        ofstream tree_file(tree_object_path);
        tree_file << index_content;
        tree_file.close();
    }

    string parent_hash = "0000000000000000000000000000000000000000000000000000000000000000"; // default zero string
    string current_branch = get_current_branch_name();
    string current_branch_path = ".voxel/refs/heads/" + current_branch;
    if(fs::exists(current_branch_path)){
        string saved_parent = FileSystem::read_file_to_string(current_branch_path);
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
    commit_stream << "Message - " << message << "\n";
    string commit_content = commit_stream.str();
    string commit_hash = Hashing::generate_sha256(commit_content);
    string commit_object_path = ".voxel/objects/" + commit_hash;
    ofstream commit_file(commit_object_path);
    commit_file << commit_content;
    commit_file.close();
    ofstream current_branch_file(current_branch_path, ios::trunc);
    current_branch_file << commit_hash;
    current_branch_file.close();
    cout << "\033[1;32mCommit successful!\033[0m\n";
    cout << "  Commit ID: [" << commit_hash.substr(0, 8) << "...]\n";
    cout << "  Message:   " << message << "\n";

}
void Commands::create_branch(const string& branch_name){
    if (branch_name.empty() || branch_name.find(' ') != std::string::npos) {
        std::cout << "\033[31mError: Branch name cannot be empty or contain spaces.\033[0m\n";
        return;
    }
    string current_parent_branch = get_current_branch_name();
    string current_parent_path = ".voxel/refs/heads/" + current_parent_branch;
    string active_hash = FileSystem::read_file_to_string(current_parent_path);
    if (active_hash.empty()) {
        std::cout << "\033[31mError: Parent timeline has no commit history yet.\033[0m\n";
        return;
    }
    string full_new_branch;
    if(current_parent_branch == "main"){
        full_new_branch = branch_name;
    } 
    else {
        full_new_branch = current_parent_branch + "@" + branch_name;
    }
    string target_file_path = ".voxel/refs/heads/" + full_new_branch;
    if (fs::exists(target_file_path)) {
        std::cout << "\033[31mError: Timeline cluster path already allocated.\033[0m\n";
        return;
    }
    std::ofstream ofs(target_file_path);
    ofs << active_hash; 
    ofs.close();
    string printing_wala = full_new_branch;
    replace(printing_wala.begin(), printing_wala.end(), '@', '/');



    std::cout << "\033[1;32mBranch '" << printing_wala << "' successfully created at commit [" << active_hash.substr(0, 8) << "...]!\033[0m\n";

} //create branch

void Commands::switch_branch(const string& target_branch){
    if(target_branch.empty() || target_branch.find(' ') != std::string::npos) {
        std::cout << "\033[31mError: Branch name cannot be empty or contain spaces.\033[0m\n";
        return;
    }
    string internal_name = target_branch;
    replace(internal_name.begin(), internal_name.end(), '/', '@');
    string branch_path = ".voxel/refs/heads/" + internal_name;
    if (!fs::exists(branch_path) || fs::is_directory(branch_path)) {
        std::cout << "\033[31mError: Branch '" << target_branch << "' does not exist.\033[0m\n";
        return;
    }
    ofstream head_file(".voxel/HEAD", ios::trunc);
    head_file << "ref: refs/heads/" << internal_name;
    head_file.close();
    std::cout << "\033[1;32mSwitched to branch '" << target_branch << "' successfully!\033[0m\n";

    
}





