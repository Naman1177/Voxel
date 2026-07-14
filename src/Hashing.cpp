#include "Hashing.hpp"
#include "mbedtls/sha256.h"
#include <string>
#include <sstream>
#include <iomanip>


std::string Hashing::generate_sha256(const std::string& input) {
    unsigned char output[32]; 
    
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    
   
    mbedtls_sha256_starts(&ctx, 0);
    

    mbedtls_sha256_update(&ctx, reinterpret_cast<const unsigned char*>(input.c_str()), input.size());
    
    mbedtls_sha256_finish(&ctx, output);
    mbedtls_sha256_free(&ctx);

  
    std::stringstream ss;
    for (int i = 0; i < 32; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(output[i]);
    }
    
    return ss.str();
}