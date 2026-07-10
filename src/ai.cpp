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
#include <Commands.hpp>
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
    fs::create_directories(".voxel/ai");
        
        
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
ai::sendToAI::sendToAI() {
    is_ready = ai::load_config(config);
}
string ai::sendToAI::escape_json(const string& raw_input){
    stringstream ss;
    for (char ch : raw_input) {
        switch (ch) {
        case '\\': ss << "\\\\"; break;
        case '"':  ss << "\\\""; break;
        case '\b': ss << "\\b"; break;
        case '\f': ss << "\\f"; break;
        case '\n': ss << "\\n"; break;
        case '\r': ss << "\\r"; break;
        case '\t': ss << "\\t"; break;
        default:   ss << ch; break;
        }
    }
    return ss.str();

}
string ai::sendToAI::transmit(const string& url, const string& headers, const string& json_body) {
    if(!fs::exists(".voxel")){
        cout << "\033[31m[Error] Cannot transmit data. Repository not initialized Run 'voxel init' first.\033[0m\n";
        return "";
    }
    string out_cache = ".voxel/ai/netpayload.json";
    string in_cache = ".voxel/ai/netresponse.json";
    ofstream writer(out_cache,ios::out|ios::trunc);
    if (!writer.is_open()) return "Error: Disk caching arrays inaccessible.";
    writer << json_body;
    writer.close();
    string command = "curl -s -X POST \"" + url + "\" " + headers + " -d @" + out_cache + " > " + in_cache;
    int code = std::system(command.c_str());
    if (code != 0) {
        fs::remove(out_cache);
        return "Error: Transmission failed with code ,Check your network connection.";
    }
    ifstream reader(in_cache);
    if (!reader.is_open()) return "Error: System stream terminated prematurely.";
    stringstream buffer;
    buffer << reader.rdbuf();
    reader.close();

    fs::remove(out_cache);
    fs::remove(in_cache);
    return buffer.str();
    

}
string ai::sendToAI::clean_json_response(const string& raw_json, const string& key_token) {
    size_t start = raw_json.find(key_token);
    if (start == string::npos) return "";
    start += key_token.length();

    // 🌟 THE FIX: Smart search that ignores escaped quotes!
    size_t end = start;
    while (end < raw_json.length()) {
        if (raw_json[end] == '"') {
            // Check if the quote is escaped by a backslash
            if (end > 0 && raw_json[end - 1] == '\\') {
                // Keep going! It's just an escaped quote inside the code.
                end++;
            } else {
                // True unescaped quote found. This is the real end of the JSON string.
                break;
            }
        } else {
            end++;
        }
    }

    if (end == raw_json.length()) return "";

    string parsed = raw_json.substr(start, end - start);
    
    // Clean up all escaped characters back to normal C++/Python formatting
    size_t marker = 0;
    while ((marker = parsed.find("\\n", marker)) != string::npos) {
        parsed.replace(marker, 2, "\n"); 
        marker += 1;
    }
    marker = 0;
    while ((marker = parsed.find("\\\"", marker)) != string::npos) {
        parsed.replace(marker, 2, "\""); 
        marker += 1;
    }
    marker = 0;
    while ((marker = parsed.find("\\t", marker)) != string::npos) {
        parsed.replace(marker, 2, "\t"); 
        marker += 1;
    }
    marker = 0;
    while ((marker = parsed.find("\\\\", marker)) != string::npos) {
        parsed.replace(marker, 2, "\\"); 
        marker += 1;
    }
    marker = 0;
    while ((marker = parsed.find("\\u003c", marker)) != string::npos) {
        parsed.replace(marker, 6, "<"); marker += 1;
    }
    marker = 0;
    while ((marker = parsed.find("\\u003e", marker)) != string::npos) {
        parsed.replace(marker, 6, ">"); marker += 1;
    }
    marker = 0;
    while ((marker = parsed.find("\\u0026", marker)) != string::npos) {
        parsed.replace(marker, 6, "&"); marker += 1;
    }
    marker = 0;
    while ((marker = parsed.find("\\u0027", marker)) != string::npos) {
        parsed.replace(marker, 6, "'"); marker += 1;
    }
    
    return parsed;
}
string ai::sendToAI::run_gemini(const string& sys, const string& content) {
    string url = "https://generativelanguage.googleapis.com/v1beta/models/" + config.model + ":generateContent?key=" + config.api_key;
    string headers = "-H \"Content-Type: application/json\"";
    string body = "{\"contents\":[{\"parts\":[{\"text\":\"" + escape_json(sys) + "\\n\\n" + escape_json(content) + "\"}]}]}";
    
    string output = transmit(url, headers, body);
    string parsed = clean_json_response(output, "\"text\": \"");
    return parsed.empty() ? "Gemini Execution Failure Payload:\n" + output : parsed;
}
string ai::sendToAI::run_openai(const string& sys, const string& content) {
    string url = "https://api.openai.com/v1/chat/completions";
    string headers = "-H \"Content-Type: application/json\" -H \"Authorization: Bearer " + config.api_key + "\"";
    string body = "{\"model\":\"" + config.model + "\",\"messages\":["
                  "{\"role\":\"system\",\"content\":\"" + escape_json(sys) + "\"},"
                  "{\"role\":\"user\",\"content\":\"" + escape_json(content) + "\"}]}";
                  
    string output = transmit(url, headers, body);
    string parsed = clean_json_response(output, "\"content\":\"");
    return parsed.empty() ? "OpenAI Execution Failure Payload:\n" + output : parsed;
}
string ai::sendToAI::run_claude(const string& sys, const string& content) {
    string url = "https://api.anthropic.com/v1/messages";
    string headers = "-H \"Content-Type: application/json\" -H \"x-api-key: " + config.api_key + "\" -H \"anthropic-version: 2023-06-01\"";
    string body = "{\"model\":\"" + config.model + "\",\"system\":\"" + escape_json(sys) + "\",\"messages\":["
                  "{\"role\":\"user\",\"content\":\"" + escape_json(content) + "\"}]}";
                  
    string output = transmit(url, headers, body);
    string parsed = clean_json_response(output, "\"text\": \"");
    return parsed.empty() ? "Claude Execution Failure Payload:\n" + output : parsed;
}
string ai::sendToAI::run_ollama(const string& sys, const string& content) {
    string url = "http://localhost:11434/api/generate";
    string headers = "-H \"Content-Type: application/json\"";
    string body = "{\"model\":\"" + config.model + "\",\"system\":\"" + escape_json(sys) + "\",\"prompt\":\"" + escape_json(content) + "\",\"stream\":false}";
    
    string output = transmit(url, headers, body);
    string parsed = clean_json_response(output, "\"response\":\"");
    return parsed.empty() ? "Ollama Execution Failure Payload:\n" + output : parsed;
}
string ai::sendToAI::run_groq(const string& sys, const string& content) {
    string url = "https://api.groq.com/openai/v1/chat/completions";
    string headers = "-H \"Content-Type: application/json\" -H \"Authorization: Bearer " + config.api_key + "\"";
    string body = "{\"model\":\"" + config.model + "\",\"messages\":["
                  "{\"role\":\"system\",\"content\":\"" + escape_json(sys) + "\"},"
                  "{\"role\":\"user\",\"content\":\"" + escape_json(content) + "\"}]}";
                  
    string output = transmit(url, headers, body);
    string parsed = clean_json_response(output, "\"content\":\"");
    return parsed.empty() ? "Groq Execution Failure Payload:\n" + output : parsed;
}
string ai::sendToAI::run_deepseek(const string& sys, const string& content) {
    string url = "https://api.deepseek.com/chat/completions";
    string headers = "-H \"Content-Type: application/json\" -H \"Authorization: Bearer " + config.api_key + "\"";
    string body = "{\"model\":\"" + config.model + "\",\"messages\":["
                  "{\"role\":\"system\",\"content\":\"" + escape_json(sys) + "\"},"
                  "{\"role\":\"user\",\"content\":\"" + escape_json(content) + "\"}]}";
                  
    string output = transmit(url, headers, body);
    string parsed = clean_json_response(output, "\"content\":\"");
    return parsed.empty() ? "DeepSeek Execution Failure Payload:\n" + output : parsed;
}
string ai::sendToAI::execute(const string& system_prompt, const string& content) {
    if (!is_ready || config.is_connected != "true") {
        return "Error: Subsystem missing parameters. Run 'voxel login'.";
    }
    if (config.provider == "gemini")   return run_gemini(system_prompt, content);
    if (config.provider == "openai")   return run_openai(system_prompt, content);
    if (config.provider == "claude")   return run_claude(system_prompt, content);
    if (config.provider == "ollama")   return run_ollama(system_prompt, content);
    if (config.provider == "groq")     return run_groq(system_prompt, content);
    if (config.provider == "deepseek") return run_deepseek(system_prompt, content);
    return "Error: Unknown provider specified in configuration. Run 'voxel login' to reset.";
}
void ai::execute_voxel_review(const std::vector<std::string>& files_to_review, const std::string& optional_note) {
    if(files_to_review.empty()) {
        cout << "\033[1;33m[Notice] No files specified for review. Please provide file paths.\033[0m\n";
        return;
    }
    ai::sendToAI ai_agent;
    string fixed_prompt = "You are an elite software auditor. Scan the code for bugs, security flaws, and performance leaks.Specify the exact line number, explain the issue simply, and provide a clean code block demonstrating the fix.Tell the person how to fix this code in a simple way. If the code is perfect, say 'No issues found.' ";
    bool snapshot_created = false;
    for (const string& filename : files_to_review){
        ifstream infile(filename);
        if (!infile.is_open()) {
            cout << "\033[1;31m[Error] Cannot open file: " << filename << "\033[0m\n";
            continue;
        }
        stringstream buffer;
        buffer << infile.rdbuf();
        infile.close();
        string file_content = buffer.str();
        if (file_content.empty()) continue;
        string content_payload = file_content;
        if (!optional_note.empty()) {
            content_payload = " Developer Context Note: \"" + optional_note + "\"\n\nTarget Code Matrix:\n" + file_content;
        }
        cout << "\n\033[1;34mVoxel AI is auditing " << filename << "...\033[0m\n";
        cout << "-------------------------------------------------------\n";
        cout << ai_agent.execute(fixed_prompt, content_payload) << "\n";
        cout << "-------------------------------------------------------\n";
        cout << "\033[1;33mWould you like Voxel Agent to automatically apply these fixes to " << filename << "? (yes/no): \033[0m";
        string choice;
        cin >> choice;
        if(choice == "yes" || choice == "y" || choice == "Y" || choice == "Yes"  || choice == "YES"){
            if (!snapshot_created) {
                Commands::clear_snapshot_silent();
                Commands::create_snapshot();
                snapshot_created = true;
            }
            string agent_prompt = 
            "You are a precise automated refactoring engine. Rewrite the provided source code to fix the audited issues. "
            "You must return ONLY the raw, complete, updated source code text. Do NOT wrap your answer in markdown code blocks "
             "(no ```cpp or ```), do NOT include introductory pleasantries, and do NOT explain changes. Your entire response "
            "must be valid, compilable code matching the original file.";
            string fixed_code = ai_agent.execute(agent_prompt, content_payload);
            if (!fixed_code.empty() && fixed_code.find("Error:") == string::npos){
                ofstream outfile(filename, ios::out | ios::trunc);
                if (outfile.is_open()) {
                    outfile << fixed_code;
                    outfile.close();
                    cout << "\033[1;32mSuccess! " << filename << " optimized and updated by Voxel AI Agent.\033[0m\n";
                    cout << "\033[1;33mA snapshot of the previous state before ai changed the file has been created. Use 'voxel snapback' to revert if needed.\033[0m\n";
                }
                else{
                    cout << "\033[1;31mError: Failed to commit update vector to disk.\033[0m\n";
                }
            }
            else{
                cout << "\033[1;31mError: AI Engine failed to generate stable refactoring stream.\033[0m\n";
            }
        }
        else {
            cout << "Review complete. File left unchanged.\n";
        }
        
        cout << "-------------------------------------------------------\n";


    }





}
    

