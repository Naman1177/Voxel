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

};




#endif
