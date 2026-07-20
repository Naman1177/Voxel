#include "ai.hpp"
#include <iostream>
#include "FileSystem.hpp"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <filesystem>
#include <cstdlib>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <Commands.hpp>
#include "Zstd.hpp"
#include "../third_party_lib/ai_parser/json.hpp"
#define RESET "\033[0m"
#define RED "\033[31m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define CYAN "\033[36m"
#define BOLD "\033[1m"
#define DIM "\033[2m"
using namespace std;
namespace fs = std::filesystem;
const string CONFIG_PATH = ".voxel/config";
bool ai::save_config(const VoxelConfig &config)
{

    if (!fs::exists(".voxel"))
    {
        std::cerr << "\033[31m[Config Error] Cannot save configuration. Repository not initialized.\033[0m\n";
        return false;
    }

    std::ofstream file(CONFIG_PATH);
    if (!file.is_open())
    {
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
bool ai::load_config(VoxelConfig &config)
{
    std::ifstream file(CONFIG_PATH);
    if (!file.is_open())
    {
        return false;
    }

    std::string line;
    while (std::getline(file, line))
    {
        std::istringstream line_stream(line);
        std::string key, value;

        if (std::getline(line_stream, key, '=') && std::getline(line_stream, value))
        {
            if (key == "username")
                config.username = value;
            else if (key == "email")
                config.email = value;
            else if (key == "provider")
                config.provider = value;
            else if (key == "model")
                config.model = value;
            else if (key == "api_key")
                config.api_key = value;
            else if (key == "is_connected")
                config.is_connected = value;
        }
    }
    return true;
}
bool ai::create_default_config()
{
    VoxelConfig default_config;

    default_config.username = "voxel_user";
    default_config.email = "unknown@voxel.internal";
    default_config.provider = "none";
    default_config.model = "none";
    default_config.api_key = "none";
    default_config.is_connected = "false";

    return save_config(default_config);
}
bool ai::init_ai()
{
    std::cout << "\033[1;36m----------Upgrading Voxel Environment with AI Services----------\033[0m\n";

    VoxelConfig active_config;
    fs::create_directories(".voxel/ai");

    if (!load_config(active_config))
    {
        std::cout << "\033[1;33m[Notice] No existing base configuration found. Run 'voxel init' first.\033[0m\n";
        return false;
    }

    std::cout << "Retained Base Local Identity: " << active_config.username << "\n";

    cout << "Enter Your Username: ";
    cin >> active_config.username;
    cout << "Enter Your Email: ";
    cin >> active_config.email;
    cout << "Enter Your API Key: ";
    cin >> active_config.api_key;

    // active_config.username = "namanmalhotra";
    // active_config.email = "namanmalhotra451@gmail.com";
    active_config.provider = "gemini";
    active_config.model = "gemini-2.5-flash";
    // active_config.api_key = "AIzaSyFakeKey_GeminiVerified";
    active_config.is_connected = "true";

    if (save_config(active_config))
    {
        std::cout << "\033[1;32mAI Integration successfully authorized!\033[0m\n";
        std::cout << "Updated Profile Saved: " << active_config.username << " (Connected to " << active_config.provider << ")\n";
        return true;
    }
    return false;
}
ai::sendToAI::sendToAI()
{
    is_ready = ai::load_config(config);
}
string ai::sendToAI::escape_json(const string &raw_input)
{
    stringstream ss;
    for (char ch : raw_input)
    {
        switch (ch)
        {
        case '\\':
            ss << "\\\\";
            break;
        case '"':
            ss << "\\\"";
            break;
        case '\b':
            ss << "\\b";
            break;
        case '\f':
            ss << "\\f";
            break;
        case '\n':
            ss << "\\n";
            break;
        case '\r':
            ss << "\\r";
            break;
        case '\t':
            ss << "\\t";
            break;
        default:
            ss << ch;
            break;
        }
    }
    return ss.str();
}
string ai::sendToAI::transmit(const string &url, const string &headers, const string &json_body)
{
    if (!fs::exists(".voxel"))
    {
        cout << "\033[31m[Error] Cannot transmit data. Repository not initialized Run 'voxel init' first.\033[0m\n";
        return "";
    }
    string out_cache = ".voxel/ai/netpayload.json";
    string in_cache = ".voxel/ai/netresponse.json";
    ofstream writer(out_cache, ios::out | ios::trunc);
    if (!writer.is_open())
        return "Error: Disk caching arrays inaccessible.";
    writer << json_body;
    writer.close();
    string command = "curl -s -X POST \"" + url + "\" " + headers + " -d @" + out_cache + " > " + in_cache;
    int code = std::system(command.c_str());
    if (code != 0)
    {
        fs::remove(out_cache);
        return "Error: Transmission failed with code ,Check your network connection.";
    }
    ifstream reader(in_cache);
    if (!reader.is_open())
        return "Error: System stream terminated prematurely.";
    stringstream buffer;
    buffer << reader.rdbuf();
    reader.close();

    fs::remove(out_cache);
    fs::remove(in_cache);
    return buffer.str();
}

static nlohmann::json find_key_recursive(const nlohmann::json &j, const string &key)
{
    if (j.is_object())
    {
        if (j.contains(key))
            return j[key];
        for (auto it = j.begin(); it != j.end(); ++it)
        {
            nlohmann::json res = find_key_recursive(it.value(), key);
            if (!res.is_null())
                return res;
        }
    }
    else if (j.is_array())
    {
        for (const auto &element : j)
        {
            nlohmann::json res = find_key_recursive(element, key);
            if (!res.is_null())
                return res;
        }
    }
    return nlohmann::json(); // Return null if not found
}
std::unordered_map<std::string, std::string> parse_tree_content(const std::string &content)
{
    std::unordered_map<std::string, std::string> map;
    std::istringstream stream(content);
    std::string file_path, hash;
    while (stream >> file_path >> hash)
    {
        map[file_path] = hash;
    }
    return map;
}

string ai::sendToAI::clean_json_response(const string &raw_json, const string &key_token)
{
    string clean_string = raw_json;
    size_t start = clean_string.find("```json");
    if (start != string::npos)
        clean_string.erase(start, 7);
    else
    {
        start = clean_string.find("```");
        if (start != string::npos)
            clean_string.erase(start, 3);
    }

    size_t end = clean_string.rfind("```");
    if (end != string::npos)
        clean_string.erase(end, 3);

    // 2. Aggressively clean the target key (Removes quotes, colons, and spaces)
    // This turns "\"text\": " perfectly into just "text"
    string target_key = key_token;
    target_key.erase(std::remove(target_key.begin(), target_key.end(), '\"'), target_key.end());
    target_key.erase(std::remove(target_key.begin(), target_key.end(), ':'), target_key.end());
    target_key.erase(std::remove(target_key.begin(), target_key.end(), ' '), target_key.end());
    target_key.erase(std::remove(target_key.begin(), target_key.end(), '\n'), target_key.end());

    // 3. Parse and Extract Recursively!
    try
    {
        nlohmann::json j = nlohmann::json::parse(clean_string);

        // Let the recursive function dig through Gemini/OpenAI's massive payloads
        nlohmann::json found_node = find_key_recursive(j, target_key);

        if (!found_node.is_null())
        {
            if (found_node.is_string())
            {
                return found_node.get<string>(); // Returns the pure string!
            }
            else
            {
                return found_node.dump(); // Fallback if the AI returns an array/object
            }
        }
        else
        {
            std::cerr << "\033[1;31m❌ AI returned JSON, but missing key: " << target_key << "\033[0m\n";
            return "";
        }
    }
    catch (const nlohmann::json::parse_error &e)
    {
        std::cerr << "\033[1;31m❌ JSON Parsing Error. AI hallucinated invalid format.\033[0m\n";
        std::cerr << "Exception: " << e.what() << "\n";
        return "";
    }
}
string ai::sendToAI::run_gemini(const string &sys, const string &content)
{
    string url = "https://generativelanguage.googleapis.com/v1beta/models/" + config.model + ":generateContent?key=" + config.api_key;
    string headers = "-H \"Content-Type: application/json\"";
    string body = "{\"contents\":[{\"parts\":[{\"text\":\"" + escape_json(sys) + "\\n\\n" + escape_json(content) + "\"}]}]}";

    string output = transmit(url, headers, body);
    string parsed = clean_json_response(output, "\"text\": \"");
    return parsed.empty() ? "Gemini Execution Failure Payload:\n" + output : parsed;
}
string ai::sendToAI::run_openai(const string &sys, const string &content)
{
    string url = "https://api.openai.com/v1/chat/completions";
    string headers = "-H \"Content-Type: application/json\" -H \"Authorization: Bearer " + config.api_key + "\"";
    string body = "{\"model\":\"" + config.model + "\",\"messages\":["
                                                   "{\"role\":\"system\",\"content\":\"" +
                  escape_json(sys) + "\"},"
                                     "{\"role\":\"user\",\"content\":\"" +
                  escape_json(content) + "\"}]}";

    string output = transmit(url, headers, body);
    string parsed = clean_json_response(output, "\"content\":\"");
    return parsed.empty() ? "OpenAI Execution Failure Payload:\n" + output : parsed;
}
string ai::sendToAI::run_claude(const string &sys, const string &content)
{
    string url = "https://api.anthropic.com/v1/messages";
    string headers = "-H \"Content-Type: application/json\" -H \"x-api-key: " + config.api_key + "\" -H \"anthropic-version: 2023-06-01\"";
    string body = "{\"model\":\"" + config.model + "\",\"system\":\"" + escape_json(sys) + "\",\"messages\":["
                                                                                           "{\"role\":\"user\",\"content\":\"" +
                  escape_json(content) + "\"}]}";

    string output = transmit(url, headers, body);
    string parsed = clean_json_response(output, "\"text\": \"");
    return parsed.empty() ? "Claude Execution Failure Payload:\n" + output : parsed;
}
string ai::sendToAI::run_ollama(const string &sys, const string &content)
{
    string url = "http://localhost:11434/api/generate";
    string headers = "-H \"Content-Type: application/json\"";
    string body = "{\"model\":\"" + config.model + "\",\"system\":\"" + escape_json(sys) + "\",\"prompt\":\"" + escape_json(content) + "\",\"stream\":false}";

    string output = transmit(url, headers, body);
    string parsed = clean_json_response(output, "\"response\":\"");
    return parsed.empty() ? "Ollama Execution Failure Payload:\n" + output : parsed;
}
string ai::sendToAI::run_groq(const string &sys, const string &content)
{
    string url = "https://api.groq.com/openai/v1/chat/completions";
    string headers = "-H \"Content-Type: application/json\" -H \"Authorization: Bearer " + config.api_key + "\"";
    string body = "{\"model\":\"" + config.model + "\",\"messages\":["
                                                   "{\"role\":\"system\",\"content\":\"" +
                  escape_json(sys) + "\"},"
                                     "{\"role\":\"user\",\"content\":\"" +
                  escape_json(content) + "\"}]}";

    string output = transmit(url, headers, body);
    string parsed = clean_json_response(output, "\"content\":\"");
    return parsed.empty() ? "Groq Execution Failure Payload:\n" + output : parsed;
}
string ai::sendToAI::run_deepseek(const string &sys, const string &content)
{
    string url = "https://api.deepseek.com/chat/completions";
    string headers = "-H \"Content-Type: application/json\" -H \"Authorization: Bearer " + config.api_key + "\"";
    string body = "{\"model\":\"" + config.model + "\",\"messages\":["
                                                   "{\"role\":\"system\",\"content\":\"" +
                  escape_json(sys) + "\"},"
                                     "{\"role\":\"user\",\"content\":\"" +
                  escape_json(content) + "\"}]}";

    string output = transmit(url, headers, body);
    string parsed = clean_json_response(output, "\"content\":\"");
    return parsed.empty() ? "DeepSeek Execution Failure Payload:\n" + output : parsed;
}
string ai::sendToAI::execute(const string &system_prompt, const string &content)
{
    if (!is_ready || config.is_connected != "true")
    {
        return "Error: Subsystem missing parameters. Run 'voxel login'.";
    }
    if (config.provider == "gemini")
        return run_gemini(system_prompt, content);
    if (config.provider == "openai")
        return run_openai(system_prompt, content);
    if (config.provider == "claude")
        return run_claude(system_prompt, content);
    if (config.provider == "ollama")
        return run_ollama(system_prompt, content);
    if (config.provider == "groq")
        return run_groq(system_prompt, content);
    if (config.provider == "deepseek")
        return run_deepseek(system_prompt, content);
    return "Error: Unknown provider specified in configuration. Run 'voxel login' to reset.";
}
void ai::execute_voxel_review(const std::vector<std::string> &files_to_review, const std::string &optional_note)
{
    if (files_to_review.empty())
    {
        cout << "\033[1;33m[Notice] No files specified for review. Please provide file paths.\033[0m\n";
        return;
    }
    ai::sendToAI ai_agent;
    string fixed_prompt = "You are an elite software auditor. Scan the code for bugs, security flaws, and performance leaks.Specify the exact line number, explain the issue simply, and provide a clean code block demonstrating the fix.Tell the person how to fix this code in a simple way. If the code is perfect, say 'No issues found.' You MUST output ONLY valid JSON. No conversational text, no explanations outside the JSON. Use this exact schema: {\"explanation\": \"your explanation\", \"code\": \"the complete updated file code here\"}.";
    bool snapshot_created = false;
    for (const string &filename : files_to_review)
    {
        ifstream infile(filename);
        if (!infile.is_open())
        {
            cout << "\033[1;31m[Error] Cannot open file: " << filename << "\033[0m\n";
            continue;
        }
        stringstream buffer;
        buffer << infile.rdbuf();
        infile.close();
        string file_content = buffer.str();
        if (file_content.empty())
            continue;
        string content_payload = file_content;
        if (!optional_note.empty())
        {
            content_payload = " Developer Context Note: \"" + optional_note + "\"\n\nTarget Code Matrix:\n" + file_content;
        }
        cout << "\n\033[1;34mVoxel AI is auditing " << filename << "...\033[0m\n";
        cout << "-------------------------------------------------------\n";
        string json_response = ai_agent.execute(fixed_prompt, content_payload);

        string fixed_code = "";

        // 2. Parse the JSON safely
        try
        {
            nlohmann::json parsed = nlohmann::json::parse(json_response);

            // 3. Print ONLY the explanation to the terminal with nice formatting
            if (parsed.contains("explanation"))
            {
                std::cout << "\n\033[1;36mAI Diagnosis:\033[0m\n"; // Cyan header
                std::cout << parsed["explanation"].get<std::string>() << "\n\n";
                cout << "-------------------------------------------------------\n";
                cout << "\033[1;33mWould you like Voxel Agent to automatically apply these fixes to " << filename << "? (yes/no): \033[0m";
                string choice;
                cin >> choice;
                if (choice == "yes" || choice == "y" || choice == "Y" || choice == "Yes" || choice == "YES")
                {
                    if (!snapshot_created)
                    {
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
                    if (!fixed_code.empty() && fixed_code.find("Error:") == string::npos)
                    {
                        ofstream outfile(filename, ios::out | ios::trunc);
                        if (outfile.is_open())
                        {
                            outfile << fixed_code;
                            outfile.close();
                            cout << "\033[1;32mSuccess! " << filename << " optimized and updated by Voxel AI Agent.\033[0m\n";
                            cout << "\033[1;33mA snapshot of the previous state before ai changed the file has been created. Use 'voxel snapback' to revert if needed.\033[0m\n";
                        }
                        else
                        {
                            cout << "\033[1;31mError: Failed to commit update vector to disk.\033[0m\n";
                        }
                    }
                    else
                    {
                        cout << "\033[1;31mError: AI Engine failed to generate stable refactoring stream.\033[0m\n";
                    }
                }
                else
                {
                    cout << "Review complete. File left unchanged.\n";
                }
            }
            else
            {
                std::cerr << "\033[1;31m❌ AI response missing 'explanation' key.\033[0m\n";
                return;
            }

            // 4. Save the code portion to a variable so you can write it to the file later
            if (parsed.contains("code"))
            {
                fixed_code = parsed["code"].get<std::string>();
            }
            else
            {
                std::cerr << "\033[1;31mAI response missing 'code' key.\033[0m\n";
                return;
            }
        }
        catch (nlohmann::json::parse_error &e)
        {
            std::cerr << "\033[1;31mFailed to parse AI JSON response: " << e.what() << "\033[0m\n";
            return;
        }

        // 5. NOW prompt the user

        // ... rest of your code (cin >> choice, write fixed_code to file, etc.) ...

        cout << "-------------------------------------------------------\n";
    }
}
void ai::commit_with_ai()
{
    cout << "\033[1;36m----------Voxel AI Commit Engine----------\033[0m\n";
    if (!fs::exists(".voxel/index"))
    {
        cout << "\033[1;31mError: No tracked files found. Run 'voxel track' first.\033[0m\n";
        return;
    }
    std::string new_index_content = FileSystem::read_file_to_string(".voxel/index");
    if (new_index_content.empty())
    {
        std::cout << "\033[1;31mError: Tracking area is empty. Nothing to commit.\033[0m\n";
        return;
    }
    unordered_map<std::string, std::string> new_tracked = parse_tree_content(new_index_content);
    unordered_map<std::string, std::string> old_tracked;
    string current_branch = Commands::get_current_branch_name();
    string current_branch_path = ".voxel/refs/heads/" + current_branch;
    if (fs::exists(current_branch_path))
    {
        string parent_hash = FileSystem::read_file_to_string(current_branch_path);
        if (!parent_hash.empty() && parent_hash != "0000000000000000000000000000000000000000000000000000000000000000")
        {
            std::string parent_commit_content = FileSystem::read_file_to_string(".voxel/objects/" + parent_hash);
            string tree_hash;
            istringstream commit_stream(parent_commit_content);
            string line;
            while (std::getline(commit_stream, line))
            {
                if (line.find("tree - ") == 0)
                {
                    tree_hash = line.substr(7);
                    break;
                }
            }
            if (!tree_hash.empty())
            {
                std::string old_tree_content = FileSystem::read_file_to_string(".voxel/objects/" + tree_hash);
                old_tracked = parse_tree_content(old_tree_content);
            }
        }
    }
    stringstream change_summary;
    change_summary << "Voxel Branch: " << current_branch << "\n\n";
    int file_count = 0;
    for (const auto &[file_path, new_hash] : new_tracked)
    {
        string ext = fs::path(file_path).extension().string();
        if (Commands::should_ignore_extension(ext))
        {
            continue;
        }
        bool is_new = (old_tracked.find(file_path) == old_tracked.end());
        bool is_modified = (!is_new && old_tracked[file_path] != new_hash);
        if (is_new || is_modified)
        {
            change_summary << "--- FILE: " << file_path << " (" << (is_new ? "NEW" : "MODIFIED") << ") ---\n";

            if (is_modified)
            {
                std::string old_object_path = ".voxel/objects/" + old_tracked[file_path];

                // 1. Define a temporary file path inside your hidden folder
                std::string temp_path = ".voxel/temp_decompress";

                // 2. Decompress the old object from the vault into the temp file
                Zstd::decompress_file(old_object_path, temp_path);

                // 3. Read the actual text content from the temp file into your string
                std::string old_code = FileSystem::read_file_to_string(temp_path);

                // 4. Delete the temp file immediately to keep the workspace clean
                fs::remove(temp_path);

                change_summary << "[PREVIOUS STATE]\n"
                               << old_code << "\n";
            }

            std::string new_code = FileSystem::read_file_to_string(file_path);
            change_summary << "[CURRENT STATE]\n"
                           << new_code << "\n\n";
            file_count++;
        }
    }
    if (file_count == 0)
    {
        std::cout << "\033[1;33mNo text/code modifications found to analyze for message generation.\033[0m\n";
        cout << "\033[1;33mPlease ensure you have tracked files and made changes before using 'voxel commit ai'.\033[0m\n";
        return;
    }
    std::string system_prompt = R"(You are the core logging agent for the Voxel version control engine.
    Analyze the provided file changes (old vs new state). Write a highly professional, standard conventional commit message.
    You MUST output ONLY a valid JSON object. Do not wrap it in markdown text blocks.
    Use this exact schema:
    {"commit_message": "Brief clear summary of the changes starting with a capital letter"}
    IMPORTANT: Do NOT use scopes like (example) or (core). Just use the type (e.g., feat, fix, refactor, chore) followed by a colon.)
    IMPORTANT: Do NOT use Git conventional commit prefixes like "feat:", "fix:", or "chore:". Just write the plain English summary.)";

    cout << "\033[1;35mAnalyzing compressed object and generating message...\033[0m\n";
    ai::sendToAI ai_agent;
    string raw_response = ai_agent.execute(system_prompt, change_summary.str());
    std::string ai_commit_msg = "";
    try
    {
        std::string clean_json = raw_response;
        size_t start = clean_json.find("```json");
        if (start != std::string::npos)
            clean_json.erase(start, 7);
        else
        {
            start = clean_json.find("```");
            if (start != std::string::npos)
                clean_json.erase(start, 3);
        }
        size_t end = clean_json.rfind("```");
        if (end != std::string::npos)
            clean_json.erase(end, 3);

        nlohmann::json parsed_data = nlohmann::json::parse(clean_json);
        if (parsed_data.contains("commit_message"))
        {
            ai_commit_msg = parsed_data["commit_message"].get<std::string>();
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "\033[1;31mAI format failure. Using fallback message.\033[0m\n";
        ai_commit_msg = "chore: automated Voxel engine state update";
    }

    std::cout << "\033[1;32mVoxel AI drafted message:\033[0m \"" << ai_commit_msg << "\"\n";
    std::cout << "\033[1;36mHanding off to Voxel commit engine...\033[0m\n";
    Commands::commit_changes(ai_commit_msg);
}
void ai::run_ai_diff(const std::string &filepath, const std::string &old_content, const std::string &new_content)
{
    ai::sendToAI ai_agent;
    std::cout << DIM << "Analyzing structural changes and blast radius for " << filepath << "..." << RESET << "\n\n";

    // 1. Combine old and new content into a single payload
    std::string ai_payload = "FILE: " + filepath + "\n\n";
    ai_payload += "=== OLD FILE CONTENT ===\n";
    ai_payload += old_content + "\n\n";
    ai_payload += "=== NEW FILE CONTENT ===\n";
    ai_payload += new_content + "\n";

    // 2. Strict System Prompt (NO JSON, Pure Terminal Formatting)
    std::string system_prompt =
        "You are Voxel's AI Diff Analyzer, a semantic version control assistant.\n"
        "I will provide you with the old and new content of a file.\n\n"
        "STRICT OUTPUT RULES:\n"
        "1. DO NOT output JSON under any circumstances.\n"
        "2. DO NOT wrap your entire response in markdown code blocks (e.g., no ``` or ```json).\n"
        "3. Output ONLY clean, human-readable terminal text following the exact template below.\n\n"
        "TEMPLATE:\n\n"
        "🌟 EXECUTIVE SUMMARY:\n"
        "<Write a punchy, 1-sentence summary of the entire file change>\n\n"
        "🔍 SCOPE BREAKDOWN:\n"
        "• Scope: [<Scope Name, e.g., def start_engine()>]\n"
        "  - Change: <1-sentence explanation of what changed>\n"
        "  - Edits: <Brief bullet points of additions/removals>\n"
        "(Repeat for each modified, added, or deleted scope)\n\n"
        "📈 OVERALL CHANGE SUMMARY:\n"
        "• <Estimate lines added, deleted, and modified based on the code provided>\n\n"
        "⚠️ BLAST RADIUS & IMPACT:\n"
        "• <Bullet point 1: What other functions, files, or imports might break/need updates>\n"
        "• <Bullet point 2: Variable scope leaks, unhandled exceptions, or architectural shifts>";

    // 3. Dispatch to AI router
    cout << "\033[1;35mFiling AI diff request for " << filepath << "...\033[0m\n";
    cout << ai_agent.execute(system_prompt, ai_payload);
}
