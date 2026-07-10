#ifndef AI_HPP
#define AI_HPP
#include <string>
using namespace std;
class ai {
public:
    struct VoxelConfig{
        std::string username = "voxel_user";
        std::string email = "unknown@voxel.internal";
        std::string provider = "none";
        std::string model = "none";
        std::string api_key = "none";
        std::string is_connected = "false";
    };
    static bool load_config(VoxelConfig& config);
    static bool save_config(const VoxelConfig& config);
    static bool create_default_config();
    static bool init_ai();
private:
   








};
#endif
