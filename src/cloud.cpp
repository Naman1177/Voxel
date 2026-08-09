#include "Commands.hpp"
#include "FileSystem.hpp"
#include "Zstd.hpp"
#include "Hashing.hpp"
#include "diff_merge.hpp"
#include "cloud.hpp"
#include "ai.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <vector>
#include <algorithm>
#include <string>
#include <map>
#include <set>
#include <queue>
#include <sys/ioctl.h>
#include <unistd.h>
#include <cstdint>
using namespace std;
namespace fs = std::filesystem;
#define RESET       "\033[0m"
#define RED         "\033[31m"
#define GREEN       "\033[32m"
#define YELLOW      "\033[33m"
#define CYAN        "\033[36m"
#define BOLD        "\033[1m"
#define DIM         "\033[2m"



void Cloud::pack_repository(const vector<string>& args) {
    if(!fs::exists(".voxel")){
        cerr << "\033[1;31mError: Not a Voxel repository.\033[0m\n";
        return;
    }
    string output_filename = fs::current_path().filename().string() + ".vxlpack";
    if (!args.empty()) {
        output_filename = args[0] + ".vxlpack";
    }
    cout << "\033[1;36m[Voxel Pack] Assembling universal payload...\033[0m\n";
    ofstream out(output_filename, std::ios::binary);
    if (!out) {
        cerr << "\033[1;31mError: Could not create payload archive " << output_filename << "\033[0m\n";
        return;
    }
    out.write("VXLPAK01", 8);
    auto write_file_to_pack = [&](const std::string& filepath, const std::string& custom_content = ""){
        string content = custom_content.empty() ? FileSystem::read_file_to_string(filepath) : custom_content;
        uint32_t path_len = filepath.size();
        out.write(reinterpret_cast<const char*>(&path_len), sizeof(path_len));
        out.write(filepath.c_str(), path_len);
        uint32_t content_len = content.size();
        out.write(reinterpret_cast<const char*>(&content_len), sizeof(content_len));
        out.write(content.c_str(), content_len);
    };
    int file_count = 0;
    for (const auto& entry : fs::recursive_directory_iterator(".voxel")) {
        if (entry.is_regular_file()) {
            string path = entry.path().string();
            if (path == ".voxel/config") {
                continue;
            }
            write_file_to_pack(path);
            file_count++;
        }
    }
    string default_config = 
        "username=voxel_user\n"
        "email=unknown@voxel.internal\n"
        "provider=none\n"
        "model=none\n"
        "api_key=none\n"
        "is_connected=false\n";
    write_file_to_pack(".voxel/config", default_config);
    file_count++;
    if (fs::exists(".voxelignore")) {
        write_file_to_pack(".voxelignore");
        file_count++;
    }
    out.close();
    cout << "\033[1;32m[Voxel Pack] Successfully packed " << file_count << " files into " << output_filename << "\033[0m\n";
        
}
void Cloud::unpack_repository(const string& filename){
    if (filename.empty()) {
        cerr << "\033[1;31mError: Please provide the payload filename (e.g., voxel unpack test.vxlpack).\033[0m\n";
        return;
    }
    string input_filename = filename;
    if (!fs::exists(input_filename)) {
        std::cerr << "\033[1;31mError: Payload file '" << input_filename << "' not found.\033[0m\n";
        return;
    }
    if (fs::exists(".voxel")) {
        cerr << "\033[1;31mError: A Voxel repository already exists in this directory.\033[0m\n";
        cerr << "Please run unpack inside a fresh, empty directory.\n";
        return;
    }
    ifstream in(input_filename, std::ios::binary);
    if (!in) {
        cerr << "\033[1;31mError: Cannot open payload stream.\033[0m\n";
        return;
    }
    char magic[9] = {0};
    in.read(magic, 8);
    if (string(magic) != "VXLPAK01") {
        cerr << "\033[1;31mError: Invalid or corrupted Voxel payload. Signature mismatch.\033[0m\n";
        return;
    }
    cout << "\033[1;36m[Voxel Unpack] Extracting timeline and object vault...\033[0m\n";
    int file_count = 0;
    while (in.peek() != EOF) {
        uint32_t path_len = 0;
        if (!in.read(reinterpret_cast<char*>(&path_len), sizeof(path_len))) break;

        string filepath(path_len, '\0');
        in.read(&filepath[0], path_len);

        uint32_t content_len = 0;
        in.read(reinterpret_cast<char*>(&content_len), sizeof(content_len));

        string content(content_len, '\0');
        in.read(&content[0], content_len);
        fs::path dest_path(filepath);
        if (dest_path.has_parent_path()) {
            std::filesystem::create_directories(dest_path.parent_path());
        }

        ofstream out_file(filepath, std::ios::binary);
        out_file.write(content.c_str(), content_len);
        out_file.close();
        
        file_count++;
    }
    in.close();
    cout << "\033[1;32m✔ Vault extraction complete (" << file_count << " core files restored).\033[0m\n";
    Commands::restore_workspace_state("");
    fs::remove(input_filename);
    cout << "\033[1;33mRun 'voxel login' to stamp your identity on this cloned workspace.\033[0m\n";

}
