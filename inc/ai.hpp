#ifndef AI_HPP
#define AI_HPP
#include <string>
using namespace std;
class ai {
public:
    struct VoxelConfig{
        string username = "voxel_user";
        string email = "unknown@voxel.internal";
        string provider = "none";
        string model = "none";
        string api_key = "none";
        string is_connected = "false";
    };
    static bool load_config(VoxelConfig& config);
    static bool save_config(const VoxelConfig& config);
    static bool create_default_config();
    static bool init_ai();

   class sendToAI {
    private:
    VoxelConfig config;
    bool is_ready;
    string run_gemini(const string& sys, const string& content);
    string run_openai(const string& sys, const string& content);
    string run_claude(const string& sys, const string& content);
    string run_ollama(const string& sys, const string& content);
    string run_groq(const string& sys, const string& content);
    string run_deepseek(const string& sys, const string& content);
    string escape_json(const string& raw_input);
    string transmit(const string& url, const string& headers, const string& json_body);
    string clean_json_response(const string& raw_json, const string& key_token);

    public:
    sendToAI();
    string execute(const string& system_prompt, const string& content);



    };









};
#endif
