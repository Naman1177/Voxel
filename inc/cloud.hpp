#ifndef CLOUD_HPP
#define CLOUD_HPP
#include <filesystem>
#include <iostream>
#include <vector>
#include <string>
using namespace std;
namespace fs = std::filesystem;

class Cloud{

public:
    static void pack_repository(const vector<string>& args);
    static void unpack_repository(const string& filename);
    static void host_mesh(const vector<string>& args);
    static void client_mesh(const vector<string>& args);
    static void mesh_off(const vector<string>& args);
    static void pack_targeted_branch(const string &branch_name, const string &output_filename);
    static void pull_repository(const vector<string> &args);
};




#endif
