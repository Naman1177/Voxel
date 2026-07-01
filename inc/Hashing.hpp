#ifndef HASHING_HPP
#define HASHING_HPP
#include <string>


class Hashing {
public:
    static std::string generate_sha256(const std::string& input);
};
#endif // HASHING_HPP