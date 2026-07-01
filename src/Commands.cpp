#include "Commands.hpp"
#include "FileSystem.hpp"
#include <iostream>
#include "Hashing.hpp"

#include <fstream>
#include <vector>
using namespace std;

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