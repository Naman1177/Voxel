#include "ai.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <filesystem>
using namespace std;
namespace fs = std::filesystem;
string config_path = ".voxel/config";


bool ai::save_config(const ai::VoxelConfig& config) {
    if(!fs::exists(".voxel")){
        cout << "\033[1;31mError: .voxel directory does not exist. Please run 'voxel init' first.\033[0m\n";
        return false;
    }
    ofstream file(config_path);
    if (!file.is_open()) {
        cout << "\033[31m[Config Error] Cannot open registry file for synchronization.\033[0m\n";
        return false;  
    }
    file << "username=" << config.username << "\n";
    file << "email=" << config.email << "\n";
    file << "provider=" << config.provider << "\n";
    file << "model=" << config.model << "\n";
    file << "api_key=" << config.api_key << "\n";
    file << "is_connected=" << config.is_connected << "\n";
        
    return true;
}
bool ai::load_config(ai::VoxelConfig& config) {
    
}
