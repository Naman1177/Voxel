#include "ai.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <filesystem>
#include <cstdlib>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
using namespace std;
namespace fs = std::filesystem;
const string CONFIG_PATH = ".voxel/config";
bool ai::save_config(const VoxelConfig& config) {
        
    if (!fs::exists(".voxel")) {
        std::cerr << "\033[31m[Config Error] Cannot save configuration. Repository not initialized.\033[0m\n";
        return false;
    }

    std::ofstream file(CONFIG_PATH);
    if (!file.is_open()) {
        std::cerr << "\033[31m[Config Error] Cannot open registry file for synchronization.\033[0m\n";
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

bool ai::load_config(VoxelConfig& config) {
    std::ifstream file(CONFIG_PATH);
    if (!file.is_open()) {
        return false; 
    }

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream line_stream(line);
        std::string key, value;
            
        if (std::getline(line_stream, key, '=') && std::getline(line_stream, value)) {
            if (key == "username")       config.username = value;
            else if (key == "email")     config.email = value;
            else if (key == "provider")  config.provider = value;
            else if (key == "model")     config.model = value;
            else if (key == "api_key")   config.api_key = value;
            else if (key == "is_connected") config.is_connected = value;
        }
    }
    return true;
}

    
bool ai::create_default_config() {
    VoxelConfig default_config;
        

    default_config.username = "voxel_user";
    default_config.email = "unknown@voxel.internal";
    default_config.provider = "none";
    default_config.model = "none";
    default_config.api_key = "none";
    default_config.is_connected = "false";

    return save_config(default_config);
}

   
bool ai::init_ai() {
    std::cout << "\033[1;36m----------Upgrading Voxel Environment with AI Services----------\033[0m\n";
        
    VoxelConfig active_config;
        
        
    if (!load_config(active_config)) {
        std::cout << "\033[1;33m[Notice] No existing base configuration found. Run 'voxel init' first.\033[0m\n";
        return false;
    }

    std::cout << "Retained Base Local Identity: " << active_config.username << "\n";
    
    cout << "Enter Your Username: "; cin >> active_config.username;
    cout << "Enter Your Email: "; cin >> active_config.email;
    cout << "Enter Your API Key: "; cin>> active_config.api_key;
    
    //active_config.username = "namanmalhotra"; 
    //active_config.email = "namanmalhotra451@gmail.com";
    active_config.provider = "gemini";
    active_config.model = "gemini-2.5-flash";
    //active_config.api_key = "AIzaSyFakeKey_GeminiVerified";
    active_config.is_connected = "true";

    if (save_config(active_config)) {
        std::cout << "\033[1;32mAI Integration successfully authorized!\033[0m\n";
        std::cout << "Updated Profile Saved: " << active_config.username << " (Connected to " << active_config.provider << ")\n";
        return true;
    }
    return false;
}

