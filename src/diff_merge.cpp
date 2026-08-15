#include "diff_merge.hpp"
#include "Hashing.hpp"
#include "FileSystem.hpp"
#include "Zstd.hpp"
#include "Commands.hpp"
#include <vector>
#include <iomanip>
#include <sstream>
#include <iostream>
#include <unordered_set>
#include <set>
#include "ai.hpp"
#include "../third_party_lib/dtl/dtl.hpp"
namespace fs = std::filesystem;
using namespace std;
#define RESET "\033[0m"
#define RED "\033[31m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define CYAN "\033[36m"
#define BOLD "\033[1m"
#define DIM "\033[2m"



string diffEngine::generate_block_hash(const vector<string> &lines){
    string combined = "";
    for (const auto &line : lines)
    {
    
        if (line.find_first_not_of(" \t\r\n") != string::npos)
        {
            combined += line + "\n";
        }
    }
    return Hashing::generate_sha256(combined);
}
vector<Block> diffEngine::parse_file(const string &filepath)
{
    string file_content = FileSystem::read_file_to_string(filepath);
    if (file_content.empty())
        return vector<Block>();
    return diffEngine::parse_memory(file_content);
}
bool diffEngine::is_scope_header(const string &raw_line, string &out_scope_name)
{
    size_t start = raw_line.find_first_not_of(" \t\r\n");
    if (start == string::npos)
        return false;

    string line = raw_line.substr(start);

    // Sniff for Structural Keywords (Python & C++ friendly)
    if (line.find("def ") == 0 || line.find("class ") == 0 ||
        line.find("struct ") == 0 || line.find("namespace ") == 0 ||

        // JavaScript & TypeScript (For the Web/MERN Stack)
        line.find("function ") == 0 || line.find("interface ") == 0 ||
        line.find("export ") == 0 || line.find("type ") == 0 ||

        // Go & Rust (Modern Systems)
        line.find("func ") == 0 || line.find("fn ") == 0 ||
        line.find("impl ") == 0 || line.find("trait ") == 0 ||

        // Standard Control Flow (Universal)
        line.find("for ") == 0 || line.find("for(") == 0 ||
        line.find("while ") == 0 || line.find("while(") == 0 ||
        line.find("if ") == 0 || line.find("if(") == 0 ||
        line.find("elif ") == 0 ||
        line.find("else if ") == 0 || line.find("else if(") == 0 ||
        line.find("else") == 0 || // Catches "else :" and "else {"
        line.find("switch ") == 0 || line.find("switch(") == 0 ||
        line.find("try") == 0 || // Catches "try :" and "try {"
        line.find("catch ") == 0 || line.find("catch(") == 0)
    {

        // Clean up the scope name for the UI (remove trailing colons or braces)
        size_t end = line.find_last_not_of(" {:\r\n");
        out_scope_name = line.substr(0, end + 1);
        return true;
    }
    return false;
}
vector<Block> diffEngine::parse_memory(const string &raw_content)
{
    vector<Block> blocks;
    stringstream stream(raw_content);
    string line;

    int line_num = 1;
    Block current_block;
    current_block.start_line = 1;
    current_block.scope = "Global Scope";

    string scope_name;

    while (getline(stream, line))
    {
        // If we hit a new structural header, package the old block and start a new one
        if (is_scope_header(line, scope_name))
        {
            if (!current_block.lines.empty())
            {
                current_block.end_line = line_num - 1;
                current_block.content_hash = generate_block_hash(current_block.lines);
                blocks.push_back(current_block);
            }
            // Start new block
            current_block = Block();
            current_block.start_line = line_num;
            current_block.scope = scope_name;
        }

        current_block.lines.push_back(line);
        line_num++;
    }

    // Capture the final block
    if (!current_block.lines.empty())
    {
        current_block.end_line = line_num - 1;
        current_block.content_hash = generate_block_hash(current_block.lines);
        blocks.push_back(current_block);
    }

    return blocks;
}
vector<DiffResult> diffEngine::analyze_diff(const vector<Block> &old_blocks, const vector<Block> &new_blocks)
{
    vector<DiffResult> results;
    vector<bool> old_matched(old_blocks.size(), false);
    vector<bool> new_matched(new_blocks.size(), false);

    // Pass 1: Exact structural identity match (Same scope name)
    for (size_t i = 0; i < old_blocks.size(); i++)
    {
        for (size_t j = 0; j < new_blocks.size(); j++)
        {
            if (!new_matched[j] && old_blocks[i].scope == new_blocks[j].scope)
            {
                DiffResult res;
                res.old_block = old_blocks[i];
                res.new_block = new_blocks[j];

                if (old_blocks[i].content_hash == new_blocks[j].content_hash)
                {
                    res.type = UNCHANGED;
                }
                else
                {
                    res.type = MODIFIED;
                }

                results.push_back(res);
                old_matched[i] = true;
                new_matched[j] = true;
                break; // Move to next old_block
            }
        }
    }
    // Pass 1.5: Fuzzy Content Matching (Catch renamed scope headers!)
    for (size_t i = 0; i < old_blocks.size(); i++)
    {
        if (old_matched[i])
            continue;

        for (size_t j = 0; j < new_blocks.size(); j++)
        {
            if (new_matched[j])
                continue;

            // Count how many identical lines of code they share inside
            int shared_lines = 0;
            for (const auto &old_line : old_blocks[i].lines)
            {
                // Ignore whitespace lines for the similarity check
                if (old_line.find_first_not_of(" \t\r\n") != string::npos)
                {
                    if (find(new_blocks[j].lines.begin(), new_blocks[j].lines.end(), old_line) != new_blocks[j].lines.end())
                    {
                        shared_lines++;
                    }
                }
            }

            // If they share at least 1 line of actual code, they are the same block!
            if (shared_lines > 0)
            {
                DiffResult res;
                res.type = MODIFIED;
                res.old_block = old_blocks[i];
                res.new_block = new_blocks[j];
                results.push_back(res);

                old_matched[i] = true;
                new_matched[j] = true;
                break;
            }
        }
    }

    // Pass 2: Catch Unmatched Deletions
    for (size_t i = 0; i < old_blocks.size(); i++)
    {
        if (!old_matched[i])
        {
            DiffResult res;
            res.type = DELETED;
            res.old_block = old_blocks[i];
            results.push_back(res);
        }
    }

    // Pass 3: Catch Unmatched Additions
    for (size_t j = 0; j < new_blocks.size(); j++)
    {
        if (!new_matched[j])
        {
            DiffResult res;
            res.type = ADDED;
            res.new_block = new_blocks[j];
            results.push_back(res);
        }
    }

    return results;
}
static void render_granular_diff(const vector<string> &old_lines, const vector<string> &new_lines,int old_start, int new_start,int &lines_ins, int &chars_ins, int &lines_del, int &chars_del){

    size_t i = 0, j = 0;

    while (i < old_lines.size() || j < new_lines.size())
    {
        if (i < old_lines.size() && j < new_lines.size() && old_lines[i] == new_lines[j])
        {
            // UNCHANGED LINE: Skip printing
            i++;
            j++;
        }
        else if (i < old_lines.size() && (j >= new_lines.size() ||
                                          find(new_lines.begin() + j, new_lines.end(), old_lines[i]) == new_lines.end()))
        {

            // DELETED LINE: Only process if it's NOT just whitespace
            if (old_lines[i].find_first_not_of(" \t\r\n") != string::npos)
            {
                cout << DIM << setw(4) << (old_start + i) << " │ " << RESET << RED << "- " << old_lines[i] << RESET << "\n";
                lines_del++;
                chars_del += old_lines[i].length();
            }
            i++;
        }
        else
        {
            // ADDED LINE: Only process if it's NOT just whitespace
            if (new_lines[j].find_first_not_of(" \t\r\n") != string::npos)
            {
                cout << DIM << setw(4) << (new_start + j) << " │ " << RESET << GREEN << "+ " << new_lines[j] << RESET << "\n";
                lines_ins++;
                chars_ins += new_lines[j].length();
            }
            j++;
        }
    }
}
void diffEngine::render_diff(const vector<DiffResult> &results, const string &fileA, const string &fileB)
{
    cout << BOLD << CYAN << "\n┌──────────────────────────────────────────────────────────┐\n";
    cout << "│  VOXEL DIFF GRAPH: " << fileA << " ➔ " << fileB << "\n";
    cout << "└──────────────────────────────────────────────────────────┘\n"
              << RESET;

    int lines_inserted = 0, chars_inserted = 0;
    int lines_deleted = 0, chars_deleted = 0;
    bool has_changes = false;

    for (const auto &res : results)
    {
        if (res.type == UNCHANGED)
            continue;

        has_changes = true;

        if (res.type == MODIFIED)
        {
            cout << "\nScope: " << BOLD << res.new_block.scope << RESET << "  " << YELLOW << "[MODIFIED]" << RESET << "\n";
            cout << DIM << " ────────────────────────────────────────────────────────\n"
                      << RESET;

            render_granular_diff(res.old_block.lines, res.new_block.lines,
                                 res.old_block.start_line, res.new_block.start_line,
                                 lines_inserted, chars_inserted, lines_deleted, chars_deleted);
        }
        else if (res.type == ADDED){
            
                 
            cout << "\nScope: " << BOLD << res.new_block.scope << RESET << "  " << GREEN << "[ADDED]" << RESET << "\n";
            cout << DIM << " ────────────────────────────────────────────────────────\n"
                      << RESET;
            int ln = res.new_block.start_line;
            for (const auto &line : res.new_block.lines){
            
                if (line.find_first_not_of(" \t\r\n") == string::npos) {
                    ln++; 
                    continue; 
                }

                cout << DIM << setw(4) << ln++ << " | " << RESET << GREEN << "+ " << line << RESET << "\n";
                lines_inserted++;
                chars_inserted += line.length();
            }
        }
        else if (res.type == DELETED)
        {
            cout << "\nScope: " << BOLD << res.old_block.scope << RESET << "  " << RED << "[DELETED]" << RESET << "\n";
            cout << DIM << " ────────────────────────────────────────────────────────\n"
                      << RESET;
            int ln = res.old_block.start_line;
            for (const auto &line : res.old_block.lines){
                if (line.find_first_not_of(" \t\r\n") == string::npos) {
                    ln++; 
                    continue; 
                }

                cout << DIM << setw(4) << ln++ << " | " << RESET << RED << "- " << line << RESET << "\n";
                lines_deleted++;
                chars_deleted += line.length();
            }
        }
    }

    if (!has_changes)
    {
        cout << "\n"
                  << DIM << "  No structural changes detected." << RESET << "\n";
    }

    // SUMMARY FOOTER
    cout << "\n"
              << DIM << " ────────────────────────────────────────────────────────\n"
              << RESET;
    cout << BOLD << "DIFF SUMMARY:\n"
              << RESET;
    cout << GREEN << "    + " << lines_inserted << " lines inserted (" << chars_inserted << " characters)\n"
              << RESET;
    cout << RED << "    - " << lines_deleted << " lines deleted  (" << chars_deleted << " characters)\n"
              << RESET;
    cout << DIM << " ────────────────────────────────────────────────────────\n\n"
              << RESET;
}
void diffEngine::run_engine_on_file(const string &filepath, const string &old_content, const string &new_content)
{
    vector<Block> old_blocks = parse_memory(old_content);
    vector<Block> new_blocks = parse_memory(new_content);

    vector<DiffResult> results = analyze_diff(old_blocks, new_blocks);

    // File names for UI
    string old_name = filepath + " (Old)";
    string new_name = filepath + " (New)";

    render_diff(results, old_name, new_name);
}
static string fetch_decompress(const string &object_hash) {
    if (object_hash.empty()) return "";
    
    string src_path = ".voxel/objects/" + object_hash;
    string tmp_path = ".voxel/tmp_diff_" + object_hash;
    
    if (fs::exists(src_path)) {
        
        if (Zstd::decompress_file(src_path, tmp_path)) {
            string content = FileSystem::read_file_to_string(tmp_path);
            fs::remove(tmp_path);
            return content;
        }
        return FileSystem::read_file_to_string(src_path);
    }
    
    return "";
}
static string read_first_line(const string &path)
{
    ifstream file(path);
    string line;
    if (file.is_open())
    {
        getline(file, line);
    }
    return line;
}
static string get_branch_latest_commit(const string &branch_name)
{
    string ref_path = ".voxel/refs/heads/" + branch_name;
    if (!fs::exists(ref_path))
    {
        cout << RED << "Error: Branch '" << branch_name << "' does not exist.\n"
             << RESET;
        return "";
    }
    return read_first_line(ref_path);
}
static string find_branch_base_commit(string branch_tip_hash)
{

    string trunk_tip_hash = get_branch_latest_commit("main");
    if (trunk_tip_hash.empty())
        return branch_tip_hash;

    unordered_set<string> trunk_history;

    string current = trunk_tip_hash;
    while (!current.empty())
    {
        trunk_history.insert(current);
        string commit_path = ".voxel/objects/" + current;
        if (!fs::exists(commit_path))
            break;

        string commit_data = FileSystem::read_file_to_string(commit_path);
        istringstream stream(commit_data);
        string line;
        string parent = "";

        while (getline(stream, line))
        {
            if (line.find("parent - ") == 0)
            {
                parent = line.substr(9);
                break;
            }
        }
        current = parent;
    }

    current = branch_tip_hash;
    string last_valid = current;

    while (!current.empty())
    {
        if (trunk_history.count(current))
        {
            return current;
        }

        last_valid = current;

        string commit_path = ".voxel/objects/" + current;
        if (!filesystem::exists(commit_path))
            break;

        string commit_data = FileSystem::read_file_to_string(commit_path);
        istringstream stream(commit_data);
        string line;
        string parent = "";

        while (getline(stream, line))
        {
            if (line.find("parent - ") == 0)
            {
                parent = line.substr(9);
                break;
            }
        }
        current = parent;
    }
    return last_valid;
}
static string resolve_target_to_commit(const string &target)
{
    if (target.length() == 64)
    {
        return target;
    }
    return get_branch_latest_commit(target);
}
static string get_current_branch_last_commit()
{
    string current_branch = Commands::get_current_branch_name();
    return get_branch_latest_commit(current_branch);
}
static string find_root_commit(string current_commit_hash)
{
    string prev_hash = current_commit_hash;
    while (!current_commit_hash.empty())
    {
        prev_hash = current_commit_hash;
        string commit_path = ".voxel/objects/" + current_commit_hash;
        string commit_data = FileSystem::read_file_to_string(commit_path);
        istringstream stream(commit_data);
        string line;
        bool found_parent = false;
        while (getline(stream, line))
        {
            if (line.find("parent - ") == 0)
            {
                current_commit_hash = line.substr(9);
                found_parent = true;
                break;
            }
        }
        if (!found_parent)
            break;
    }
    return prev_hash;
}
static string get_file_blob_hash_from_commit(const string &commit_hash, const string &filepath) {
    if (commit_hash.empty()) return "";

    string commit_path = ".voxel/objects/" + commit_hash;
    if (!filesystem::exists(commit_path)) return "";
    
    string commit_data = FileSystem::read_file_to_string(commit_path);
    istringstream commit_stream(commit_data);
    string line;
    string index_copy_hash = "";

    // 1. Get the Tree Index hash
    while (getline(commit_stream, line)) {
        if (line.find("tree - ") == 0) {
            index_copy_hash = line.substr(7);
            index_copy_hash.erase(index_copy_hash.find_last_not_of(" \n\r\t") + 1);
            break;
        }
    }

    if (index_copy_hash.empty()) return "";

    string index_path = ".voxel/objects/" + index_copy_hash;
    if (!filesystem::exists(index_path)) return "";
    
    string index_data = FileSystem::read_file_to_string(index_path);
    istringstream index_stream(index_data);
    
    // 2. Extract just the filename to match safely regardless of how Voxel stores the path
    string target_filename = filesystem::path(filepath).filename().string();

    while (getline(index_stream, line)) {
        // Search using the clean filename
        if (line.find(target_filename) != string::npos) {
            istringstream line_stream(line);
            string word;
            while (line_stream >> word) {
                if (word.length() == 64) {
                    return word; // Successfully found the Blob Hash
                }
            }
        }
    }
    return ""; 
}
static vector<string> get_all_filepaths_from_commit(const string &commit_hash) {
    vector<string> paths;
    if (commit_hash.empty()) return paths;

    string commit_path = ".voxel/objects/" + commit_hash;
    if (!filesystem::exists(commit_path)) return paths;

    string commit_data = FileSystem::read_file_to_string(commit_path);
    istringstream commit_stream(commit_data);
    string line;
    string tree_hash = "";

    // 1. Get the Tree Index hash (same lookup as get_file_blob_hash_from_commit)
    while (getline(commit_stream, line)) {
        if (line.find("tree - ") == 0) {
            tree_hash = line.substr(7);
            tree_hash.erase(tree_hash.find_last_not_of(" \n\r\t") + 1);
            break;
        }
    }
    if (tree_hash.empty()) return paths;

    string tree_path = ".voxel/objects/" + tree_hash;
    if (!filesystem::exists(tree_path)) return paths;

    // 2. Walk every line of the tree and collect the full relative filepath
    //    (first token on each line), not just the bare filename.
    string tree_data = FileSystem::read_file_to_string(tree_path);
    istringstream tree_stream(tree_data);
    while (getline(tree_stream, line)) {
        if (line.empty()) continue;
        istringstream line_stream(line);
        string filepath;
        line_stream >> filepath;
        if (!filepath.empty()) paths.push_back(filepath);
    }
    return paths;
}
void diffEngine::report_media_file_diff(const string &file,const string &old_content,const string &new_content,bool old_existed, bool new_existed)
{
    if (!old_existed && new_existed)
    {
        cout << GREEN << "[+ ADDED]   " << file << RESET
             << "  (media file, " << new_content.size() << " bytes)\n";
    }
    else if (old_existed && !new_existed)
    {
        cout << RED << "[- DELETED] " << file << RESET
             << "  (media file, was " << old_content.size() << " bytes)\n";
    }
    else if (old_existed && new_existed)
    {
        if (old_content.size() != new_content.size())
        {
            cout << YELLOW << "[~ MODIFIED]" << RESET << " " << file
                 << "  (media file, " << old_content.size() << " -> " << new_content.size() << " bytes)\n";
        }
        else
        {
            cout << CYAN << "[= UNCHANGED]" << RESET << " " << file
                 << "  (media file, " << old_content.size() << " bytes)\n";
        }
    }
    // If neither side existed there's nothing meaningful to report.
}
void diffEngine::route_diff(const vector<string> &args)
{
    vector<string> all_files = FileSystem::list_workspace_files();
    if (args.size() == 2 && fs::exists(args[0]) && fs::exists(args[1]))
    {
        string old_c = FileSystem::read_file_to_string(args[0]);
        string new_c = FileSystem::read_file_to_string(args[1]);
        diffEngine::run_engine_on_file(args[0] + " -> " + args[1], old_c, new_c);
        return;
    }
    string left_commit_hash = "";
    string right_commit_hash = "WORKSPACE";
    if (args.empty())
    {
        cout << BOLD << CYAN << "Comparing Workspace vs Last Commit (Current Branch)...\n"
             << RESET;
        left_commit_hash = get_current_branch_last_commit();
    }
    else if (args.size() == 1)
    {
        string target = args[0];
        if (target == "Head" || target == "HEAD" || target == "head")
        {
            cout << BOLD << CYAN << "Comparing Workspace vs Root Commit (Head)...\n"
                 << RESET;
            left_commit_hash = find_root_commit(get_current_branch_last_commit());
        }
        else
        {
            cout << BOLD << CYAN << "Comparing Workspace vs Root Commit of branch '" << target << "'...\n"
                 << RESET;
            left_commit_hash = find_branch_base_commit(resolve_target_to_commit(target));
        }
    }
    else if (args.size() == 2)
    {
        string target1 = args[0];
        string target2 = args[1];

        cout << BOLD << CYAN << "Comparing " << target1 << " vs " << target2 << "...\n"
                  << RESET;

        left_commit_hash = resolve_target_to_commit(target1);
        right_commit_hash = resolve_target_to_commit(target2);
    }
    else
    {
        cerr << RED << "Error: Invalid number of arguments for voxel diff. Run voxel help for usage information.\n"
                  << RESET;
        return;
    }
    for (const auto &file : all_files)
    {
        string old_content = "";
        string new_content = "";
        bool old_existed = false;
        bool new_existed = false;

        if (!left_commit_hash.empty())
        {
            string file_blob_hash = get_file_blob_hash_from_commit(left_commit_hash, file);
            if (!file_blob_hash.empty())
            {
                old_content = fetch_decompress(file_blob_hash);
                old_existed = true;
            }
        }
        if (right_commit_hash == "WORKSPACE")
        {
            if (fs::exists(file))
            {
                new_content = FileSystem::read_file_to_string(file);
                new_existed = true;
            }
        }
        else if (!right_commit_hash.empty())
        {
            string file_blob_hash = get_file_blob_hash_from_commit(right_commit_hash, file);
            if (!file_blob_hash.empty())
            {
                new_content = fetch_decompress(file_blob_hash);
                new_existed = true;
            }
        }

        if (Commands::should_ignore_extension(fs::path(file).extension().string()))
        {
            diffEngine::report_media_file_diff(file, old_content, new_content, old_existed, new_existed);
            continue;
        }

        diffEngine::run_engine_on_file(file, old_content, new_content);
    }
}
void diffEngine::ai_diff(const vector<string> &args)
{
    vector<string> all_files = FileSystem::list_workspace_files();
    if (args.size() == 2 && fs::exists(args[0]) && fs::exists(args[1]))
    {
        string old_c = FileSystem::read_file_to_string(args[0]);
        string new_c = FileSystem::read_file_to_string(args[1]);
        diffEngine::run_engine_on_file(args[0] + " -> " + args[1], old_c, new_c);
        return;
    }
    string left_commit_hash = "";
    string right_commit_hash = "WORKSPACE";
    if (args.empty())
    {
        cout << BOLD << CYAN << "Comparing Workspace vs Last Commit (Current Branch)...\n"
             << RESET;
        left_commit_hash = get_current_branch_last_commit();
    }
    else if (args.size() == 1)
    {
        string target = args[0];
        if (target == "Head" || target == "HEAD" || target == "head")
        {
            cout << BOLD << CYAN << "Comparing Workspace vs Root Commit (Head)...\n"
                 << RESET;
            left_commit_hash = find_root_commit(get_current_branch_last_commit());
        }
        else
        {
            cout << BOLD << CYAN << "Comparing Workspace vs Root Commit of branch '" << target << "'...\n"
                 << RESET;
            left_commit_hash = find_branch_base_commit(resolve_target_to_commit(target));
        }
    }
    else if (args.size() == 2)
    {
        string target1 = args[0];
        string target2 = args[1];

        cout << BOLD << CYAN << "Comparing " << target1 << " vs " << target2 << "...\n"
                  << RESET;

        left_commit_hash = resolve_target_to_commit(target1);
        right_commit_hash = resolve_target_to_commit(target2);
    }
    else
    {
        cerr << RED << "Error: Invalid number of arguments for voxel diff. Run voxel help for usage information.\n"
                  << RESET;
        return;
    }
    for (const auto &file : all_files)
    {
        string old_content = "";
        string new_content = "";
        bool old_existed = false;
        bool new_existed = false;

        if (!left_commit_hash.empty())
        {
            string file_blob_hash = get_file_blob_hash_from_commit(left_commit_hash, file);
            if (!file_blob_hash.empty())
            {
                old_content = fetch_decompress(file_blob_hash);
                old_existed = true;
            }
        }
        if (right_commit_hash == "WORKSPACE")
        {
            if (fs::exists(file))
            {
                new_content = FileSystem::read_file_to_string(file);
                new_existed = true;
            }
        }
        else if (!right_commit_hash.empty())
        {
            string file_blob_hash = get_file_blob_hash_from_commit(right_commit_hash, file);
            if (!file_blob_hash.empty())
            {
                new_content = fetch_decompress(file_blob_hash);
                new_existed = true;
            }
        }

        if (Commands::should_ignore_extension(fs::path(file).extension().string()))
        {
            diffEngine::report_media_file_diff(file, old_content, new_content, old_existed, new_existed);
            continue;
        }

        ai::run_ai_diff(file, old_content, new_content);
    }
}
namespace PdfGen {
    enum PdfStyle {
        STYLE_NORMAL = 0,   
        STYLE_ADDED = 1,    
        STYLE_DELETED = 2,  
        STYLE_TITLE = 3,    
        STYLE_SCOPE = 4     
    };

    struct PdfLine {
        string text;
        int style;
    };

    class SimplePdfWriter {
    public:
        SimplePdfWriter(int page_width = 612, int page_height = 792) // US Letter, points
            : pw(page_width), ph(page_height) {}

        void add_line(const string &text, int style = STYLE_NORMAL) {
            // Hard-wrap long lines so nothing runs off the page edge.
            const size_t max_chars = 100;
            if (text.size() <= max_chars) {
                lines.push_back({text, style});
                return;
            }
            size_t pos = 0;
            bool first = true;
            while (pos < text.size()) {
                string chunk = text.substr(pos, max_chars);
                if (!first) chunk = "    " + chunk; // indent wrapped continuation
                lines.push_back({chunk, style});
                pos += max_chars;
                first = false;
            }
        }

        void add_blank() {
            lines.push_back({"", STYLE_NORMAL});
        }

        bool save(const string &path) {
            const int margin_left = 40;
            const int margin_top = 40;
            const int margin_bottom = 40;
            const int font_size = 9;
            const int line_height = 12;

            const int usable_height = ph - margin_top - margin_bottom;
            const int lines_per_page = max(1, usable_height / line_height);

            
            vector<vector<PdfLine>> pages;
            for (size_t i = 0; i < lines.size(); i += lines_per_page) {
                size_t end = min(lines.size(), i + (size_t)lines_per_page);
                pages.push_back(vector<PdfLine>(lines.begin() + i, lines.begin() + end));
            }
            if (pages.empty()) pages.push_back({}); // always emit at least one page

            const int page_count = (int)pages.size();

           
            const int obj_catalog = 1;
            const int obj_pages = 2;
            const int obj_font = 3;
            const int first_page_obj = 4;
            const int first_content_obj = first_page_obj + page_count;
            const int total_objects = first_content_obj + page_count - 1;

            vector<string> objects(total_objects + 1); // 1-indexed

            // Catalog
            objects[obj_catalog] = "<< /Type /Catalog /Pages " + to_string(obj_pages) + " 0 R >>";

            
            {
                string kids = "[ ";
                for (int p = 0; p < page_count; p++) {
                    kids += to_string(first_page_obj + p) + " 0 R ";
                }
                kids += "]";
                objects[obj_pages] = "<< /Type /Pages /Kids " + kids +
                                      " /Count " + to_string(page_count) + " >>";
            }

            
            objects[obj_font] = "<< /Type /Font /Subtype /Type1 /BaseFont /Courier >>";

          
            for (int p = 0; p < page_count; p++) {
                int page_obj_num = first_page_obj + p;
                int content_obj_num = first_content_obj + p;

                objects[page_obj_num] =
                    "<< /Type /Page /Parent " + to_string(obj_pages) + " 0 R "
                    "/MediaBox [0 0 " + to_string(pw) + " " + to_string(ph) + "] "
                    "/Resources << /Font << /F1 " + to_string(obj_font) + " 0 R >> >> "
                    "/Contents " + to_string(content_obj_num) + " 0 R >>";

                ostringstream stream;
                int y = ph - margin_top;
                for (const auto &pl : pages[p]) {
                    double r = 0.15, g = 0.15, b = 0.15; // STYLE_NORMAL default
                    switch (pl.style) {
                        case STYLE_ADDED:   r = 0.0;  g = 0.5;  b = 0.0;  break;
                        case STYLE_DELETED: r = 0.75; g = 0.0;  b = 0.0;  break;
                        case STYLE_TITLE:   r = 0.0;  g = 0.0;  b = 0.0;  break;
                        case STYLE_SCOPE:   r = 0.0;  g = 0.15; b = 0.55; break;
                        default: break;
                    }
                    stream << r << " " << g << " " << b << " rg\n";
                    stream << "BT /F1 " << font_size << " Tf "
                           << margin_left << " " << y << " Td ("
                           << escape_pdf_text(pl.text) << ") Tj ET\n";
                    y -= line_height;
                }

                string content = stream.str();
                objects[content_obj_num] =
                    "<< /Length " + to_string(content.size()) + " >>\nstream\n" +
                    content + "endstream";
            }

            
            ofstream out(path, ios::binary);
            if (!out.is_open()) return false;

            vector<size_t> offsets(total_objects + 1, 0);
            ostringstream file_buf;
            file_buf << "%PDF-1.4\n";

            for (int i = 1; i <= total_objects; i++) {
                offsets[i] = (size_t)file_buf.tellp();
                file_buf << i << " 0 obj\n" << objects[i] << "\nendobj\n";
            }

            size_t xref_offset = (size_t)file_buf.tellp();
            file_buf << "xref\n0 " << (total_objects + 1) << "\n";
            file_buf << "0000000000 65535 f \n";
            char buf[32];
            for (int i = 1; i <= total_objects; i++) {
                snprintf(buf, sizeof(buf), "%010zu 00000 n \n", offsets[i]);
                file_buf << buf;
            }
            file_buf << "trailer\n<< /Size " << (total_objects + 1)
                      << " /Root " << obj_catalog << " 0 R >>\n";
            file_buf << "startxref\n" << xref_offset << "\n%%EOF";

            string final_data = file_buf.str();
            out.write(final_data.data(), (streamsize)final_data.size());
            out.close();
            return true;
        }

    private:
        vector<PdfLine> lines;
        int pw, ph;

        string escape_pdf_text(const string &s) {
            string out;
            out.reserve(s.size());
            for (char c : s) {
                if (c == '(' || c == ')' || c == '\\') {
                    out += '\\';
                    out += c;
                } else if ((unsigned char)c < 0x20) {
                    
                    out += ' ';
                } else {
                    out += c;
                }
            }
            return out;
        }
    };

} // namespace PdfGen
static string diff_pdf_display_timestamp() {
    time_t t = time(nullptr);
    tm tm_buf = *localtime(&t);
    ostringstream oss;
    oss << put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}
static string diff_pdf_filename_timestamp() {
    time_t t = time(nullptr);
    tm tm_buf = *localtime(&t);
    ostringstream oss;
    oss << put_time(&tm_buf, "%Y%m%d_%H%M%S");
    return oss.str();
}
static void collect_granular_diff_pdf(PdfGen::SimplePdfWriter &pdf,const vector<string> &old_lines,const vector<string> &new_lines,int old_start, int new_start,int &lines_ins, int &chars_ins,int &lines_del, int &chars_del) {
    size_t i = 0, j = 0;
    while (i < old_lines.size() || j < new_lines.size()) {
        if (i < old_lines.size() && j < new_lines.size() && old_lines[i] == new_lines[j]) {
            i++; j++;
        } else if (i < old_lines.size() && (j >= new_lines.size() ||
                   find(new_lines.begin() + j, new_lines.end(), old_lines[i]) == new_lines.end())) {
            if (old_lines[i].find_first_not_of(" \t\r\n") != string::npos) {
                ostringstream oss;
                oss << setw(4) << (old_start + i) << " | - " << old_lines[i];
                pdf.add_line(oss.str(), PdfGen::STYLE_DELETED);
                lines_del++;
                chars_del += (int)old_lines[i].length();
            }
            i++;
        } else {
            if (new_lines[j].find_first_not_of(" \t\r\n") != string::npos) {
                ostringstream oss;
                oss << setw(4) << (new_start + j) << " | + " << new_lines[j];
                pdf.add_line(oss.str(), PdfGen::STYLE_ADDED);
                lines_ins++;
                chars_ins += (int)new_lines[j].length();
            }
            j++;
        }
    }
}
static void collect_diff_results_pdf(PdfGen::SimplePdfWriter &pdf,const vector<DiffResult> &results,const string &fileA, const string &fileB,int &total_ins, int &total_del) {
    pdf.add_line("FILE DIFF: " + fileA + "  ->  " + fileB, PdfGen::STYLE_TITLE);
    pdf.add_blank();

    int lines_inserted = 0, chars_inserted = 0;
    int lines_deleted = 0, chars_deleted = 0;
    bool has_changes = false;

    for (const auto &res : results) {
        if (res.type == UNCHANGED) continue;
        has_changes = true;

        if (res.type == MODIFIED) {
            pdf.add_line("Scope: " + res.new_block.scope + "  [MODIFIED]", PdfGen::STYLE_SCOPE);
            collect_granular_diff_pdf(pdf, res.old_block.lines, res.new_block.lines,
                                       res.old_block.start_line, res.new_block.start_line,
                                       lines_inserted, chars_inserted, lines_deleted, chars_deleted);
            pdf.add_blank();
        } else if (res.type == ADDED) {
            pdf.add_line("Scope: " + res.new_block.scope + "  [ADDED]", PdfGen::STYLE_SCOPE);
            int ln = res.new_block.start_line;
            for (const auto &line : res.new_block.lines) {
                if (line.find_first_not_of(" \t\r\n") == string::npos) { ln++; continue; }
                ostringstream oss;
                oss << setw(4) << ln++ << " | + " << line;
                pdf.add_line(oss.str(), PdfGen::STYLE_ADDED);
                lines_inserted++;
                chars_inserted += (int)line.length();
            }
            pdf.add_blank();
        } else if (res.type == DELETED) {
            pdf.add_line("Scope: " + res.old_block.scope + "  [DELETED]", PdfGen::STYLE_SCOPE);
            int ln = res.old_block.start_line;
            for (const auto &line : res.old_block.lines) {
                if (line.find_first_not_of(" \t\r\n") == string::npos) { ln++; continue; }
                ostringstream oss;
                oss << setw(4) << ln++ << " | - " << line;
                pdf.add_line(oss.str(), PdfGen::STYLE_DELETED);
                lines_deleted++;
                chars_deleted += (int)line.length();
            }
            pdf.add_blank();
        }
    }

    if (!has_changes) {
        pdf.add_line("No structural changes detected.", PdfGen::STYLE_NORMAL);
        pdf.add_blank();
    }

    pdf.add_line("Summary: +" + to_string(lines_inserted) + " lines inserted (" +
                 to_string(chars_inserted) + " chars)   -" +
                 to_string(lines_deleted) + " lines deleted (" +
                 to_string(chars_deleted) + " chars)", PdfGen::STYLE_TITLE);
    pdf.add_blank();

    total_ins += lines_inserted;
    total_del += lines_deleted;
}
static void run_engine_on_file_pdf(PdfGen::SimplePdfWriter &pdf, const string &filepath,const string &old_content, const string &new_content,int &total_ins, int &total_del) {
    vector<Block> old_blocks = diffEngine::parse_memory(old_content);
    vector<Block> new_blocks = diffEngine::parse_memory(new_content);
    vector<DiffResult> results = diffEngine::analyze_diff(old_blocks, new_blocks);
    collect_diff_results_pdf(pdf, results, filepath + " (Old)", filepath + " (New)", total_ins, total_del);
}
static void report_media_file_diff_pdf(PdfGen::SimplePdfWriter &pdf, const string &file,const string &old_content, const string &new_content,bool old_existed, bool new_existed) {
    if (!old_existed && new_existed) {
        pdf.add_line("[+ ADDED]    " + file + "  (media file, " +
                     to_string(new_content.size()) + " bytes)", PdfGen::STYLE_ADDED);
    } else if (old_existed && !new_existed) {
        pdf.add_line("[- DELETED]  " + file + "  (media file, was " +
                     to_string(old_content.size()) + " bytes)", PdfGen::STYLE_DELETED);
    } else if (old_existed && new_existed) {
        if (old_content.size() != new_content.size()) {
            pdf.add_line("[~ MODIFIED] " + file + "  (media file, " +
                         to_string(old_content.size()) + " -> " +
                         to_string(new_content.size()) + " bytes)", PdfGen::STYLE_SCOPE);
        } else {
            pdf.add_line("[= UNCHANGED]" + file + "  (media file, " +
                         to_string(old_content.size()) + " bytes)", PdfGen::STYLE_NORMAL);
        }
    }
    pdf.add_blank();
}
void diffEngine::diff_pdf(const vector<string> &args) {
    PdfGen::SimplePdfWriter pdf;
    string display_ts = diff_pdf_display_timestamp();

    pdf.add_line("VOXEL VERSION CONTROL - DIFF REPORT", PdfGen::STYLE_TITLE);
    pdf.add_line("Generated: " + display_ts, PdfGen::STYLE_NORMAL);
    pdf.add_blank();

    int total_ins = 0, total_del = 0;
    vector<string> all_files = FileSystem::list_workspace_files();

    // Case: two literal on-disk file paths, e.g. "voxel diff_pdf a.cpp b.cpp"
    if (args.size() == 2 && fs::exists(args[0]) && fs::exists(args[1])) {
        string old_c = FileSystem::read_file_to_string(args[0]);
        string new_c = FileSystem::read_file_to_string(args[1]);
        run_engine_on_file_pdf(pdf, args[0] + " -> " + args[1], old_c, new_c, total_ins, total_del);

        string out_path = "diff_pdf_" + diff_pdf_filename_timestamp() + ".pdf";
        if (pdf.save(out_path)) {
            cout << BOLD << CYAN << "PDF diff written to: " << RESET << out_path << "\n";
        } else {
            cerr << RED << "Error: Failed to write PDF diff to '" << out_path << "'.\n" << RESET;
        }
        return;
    }

    string left_commit_hash = "";
    string right_commit_hash = "WORKSPACE";

    if (args.empty()) {
        cout << BOLD << CYAN << "Generating PDF diff: Workspace vs Last Commit (Current Branch)...\n" << RESET;
        left_commit_hash = get_current_branch_last_commit();
    } else if (args.size() == 1) {
        string target = args[0];
        if (target == "Head" || target == "HEAD" || target == "head") {
            cout << BOLD << CYAN << "Generating PDF diff: Workspace vs Root Commit (Head)...\n" << RESET;
            left_commit_hash = find_root_commit(get_current_branch_last_commit());
        } else {
            cout << BOLD << CYAN << "Generating PDF diff: Workspace vs Root Commit of branch '" << target << "'...\n" << RESET;
            left_commit_hash = find_branch_base_commit(resolve_target_to_commit(target));
        }
    } else if (args.size() == 2) {
        string target1 = args[0];
        string target2 = args[1];
        cout << BOLD << CYAN << "Generating PDF diff: " << target1 << " vs " << target2 << "...\n" << RESET;
        left_commit_hash = resolve_target_to_commit(target1);
        right_commit_hash = resolve_target_to_commit(target2);
    } else {
        cerr << RED << "Error: Invalid number of arguments for voxel diff_pdf.\n"
                  << "Usage: voxel diff_pdf | voxel diff_pdf <branch|hash> | voxel diff_pdf <a> <b>\n" << RESET;
        return;
    }

    for (const auto &file : all_files) {
        string old_content = "";
        string new_content = "";
        bool old_existed = false;
        bool new_existed = false;

        if (!left_commit_hash.empty()) {
            string file_blob_hash = get_file_blob_hash_from_commit(left_commit_hash, file);
            if (!file_blob_hash.empty()) {
                old_content = fetch_decompress(file_blob_hash);
                old_existed = true;
            }
        }
        if (right_commit_hash == "WORKSPACE") {
            if (fs::exists(file)) {
                new_content = FileSystem::read_file_to_string(file);
                new_existed = true;
            }
        } else if (!right_commit_hash.empty()) {
            string file_blob_hash = get_file_blob_hash_from_commit(right_commit_hash, file);
            if (!file_blob_hash.empty()) {
                new_content = fetch_decompress(file_blob_hash);
                new_existed = true;
            }
        }

        if (Commands::should_ignore_extension(fs::path(file).extension().string())) {
            report_media_file_diff_pdf(pdf, file, old_content, new_content, old_existed, new_existed);
            continue;
        }

        run_engine_on_file_pdf(pdf, file, old_content, new_content, total_ins, total_del);
    }

    pdf.add_line("TOTAL: +" + to_string(total_ins) + " lines inserted, -" +
                 to_string(total_del) + " lines deleted across all files.", PdfGen::STYLE_TITLE);

    string out_path = "diff_pdf_" + diff_pdf_filename_timestamp() + ".pdf";
    if (pdf.save(out_path)) {
        cout << BOLD << CYAN << "PDF diff written to: " << RESET << out_path << "\n";
    } else {
        cerr << RED << "Error: Failed to write PDF diff to '" << out_path << "'.\n" << RESET;
    }
}
//merge engine
struct MergeOpcode {
    enum Tag { EQUAL, CHANGE } tag;
    size_t a1, a2; 
    size_t b1, b2; 
};
static const string SANDBOX_DIR = "sandbox_merge";
static bool process_media_file_merge(const string &filepath, const string &target_branch, const string &source_branch, const string &base_commit);
static string merge_format_human_size(uint64_t bytes) {
    static const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    double size = static_cast<double>(bytes);
    int unit_idx = 0;
    while (size >= 1024.0 && unit_idx < 4) {
        size /= 1024.0;
        unit_idx++;
    }
    ostringstream oss;
    oss << fixed << setprecision(2) << size << " " << units[unit_idx];
    return oss.str();
}
static vector<MergeOpcode> merge_diff_opcodes(const vector<string> &base,const vector<string> &other) {
    size_t n = base.size(), m = other.size();
    vector<vector<int>> dp(n + 1, vector<int>(m + 1, 0));
    for (size_t i = n; i-- > 0;)
        for (size_t j = m; j-- > 0;)
            dp[i][j] = (base[i] == other[j]) ? dp[i + 1][j + 1] + 1
                                              : max(dp[i + 1][j], dp[i][j + 1]);

    vector<MergeOpcode> ops;
    size_t i = 0, j = 0, ca = 0, cb = 0;
    auto flush = [&](size_t a1, size_t a2, size_t b1, size_t b2) {
        if (a1 != a2 || b1 != b2) ops.push_back({MergeOpcode::CHANGE, a1, a2, b1, b2});
    };
    while (i < n && j < m) {
        if (base[i] == other[j]) {
            flush(ca, i, cb, j);
            size_t si = i, sj = j;
            while (i < n && j < m && base[i] == other[j]) { i++; j++; }
            ops.push_back({MergeOpcode::EQUAL, si, i, sj, j});
            ca = i; cb = j;
        } else if (dp[i + 1][j] >= dp[i][j + 1]) {
            i++;
        } else {
            j++;
        }
    }
    flush(ca, n, cb, m);
    return ops;
}
static MergeOpcode::Tag merge_find_status(const vector<MergeOpcode> &ops, size_t p) {
    for (const auto &op : ops) {
        if (op.a1 == op.a2) { if (op.a1 == p) return MergeOpcode::CHANGE; }
        else if (op.a1 <= p && p < op.a2) return op.tag;
    }
    return MergeOpcode::EQUAL;
}
static vector<string> merge_extract_side_text(const vector<string> &base,const vector<string> &side,const vector<MergeOpcode> &ops,size_t rs, size_t re) {
    vector<string> out;
    for (const auto &op : ops) {
        if (op.a1 == op.a2) {
            bool included = (rs < re) ? (op.a1 >= rs && op.a1 < re) : (op.a1 == rs);
            if (included) for (size_t k = op.b1; k < op.b2; k++) out.push_back(side[k]);
            continue;
        }
        if (op.a2 <= rs || op.a1 >= re) continue;
        if (op.tag == MergeOpcode::EQUAL) {
            size_t s = max(op.a1, rs), e = min(op.a2, re);
            for (size_t k = s; k < e; k++) out.push_back(base[k]);
        } else {
            for (size_t k = op.b1; k < op.b2; k++) out.push_back(side[k]);
        }
    }
    return out;
}
struct MergePiece {
    enum Kind { COMMON, OURS_ONLY, THEIRS_ONLY, CONFLICT } kind;
    vector<string> ours_lines;
    vector<string> theirs_lines;
};
static void merge_classify_and_push(vector<MergePiece> &pieces,const vector<string> &base,const vector<string> &ours,const vector<string> &theirs,const vector<MergeOpcode> &ops_o,const vector<MergeOpcode> &ops_t,size_t seg_start, size_t seg_end,bool o_changed, bool t_changed, bool &has_conflict) {
    vector<string> ours_text = merge_extract_side_text(base, ours, ops_o, seg_start, seg_end);
    vector<string> theirs_text = merge_extract_side_text(base, theirs, ops_t, seg_start, seg_end);
    MergePiece piece;
    if ((!o_changed && !t_changed) || ours_text == theirs_text) {
        piece.kind = MergePiece::COMMON;
        piece.ours_lines = ours_text;
    } else if (!t_changed) {
        piece.kind = MergePiece::OURS_ONLY;
        piece.ours_lines = ours_text;
    } else if (!o_changed) {
        piece.kind = MergePiece::THEIRS_ONLY;
        piece.theirs_lines = theirs_text;
    } else {
        piece.kind = MergePiece::CONFLICT;
        piece.ours_lines = ours_text;
        piece.theirs_lines = theirs_text;
        has_conflict = true;
    }
    pieces.push_back(piece);
}
static vector<MergePiece> build_three_way_pieces(const vector<string> &base,const vector<string> &ours,const vector<string> &theirs,bool &has_conflict) {
    vector<MergeOpcode> ops_o = merge_diff_opcodes(base, ours);
    vector<MergeOpcode> ops_t = merge_diff_opcodes(base, theirs);
    has_conflict = false;
    size_t n = base.size();

    set<size_t> cut_set{0, n};
    for (const auto &op : ops_o) { cut_set.insert(op.a1); cut_set.insert(op.a2); }
    for (const auto &op : ops_t) { cut_set.insert(op.a1); cut_set.insert(op.a2); }
    vector<size_t> cuts(cut_set.begin(), cut_set.end());

    vector<MergePiece> pieces;
    size_t idx = 0;
    while (idx + 1 < cuts.size()) {
        size_t seg_start = cuts[idx];
        MergeOpcode::Tag o_tag = merge_find_status(ops_o, seg_start);
        MergeOpcode::Tag t_tag = merge_find_status(ops_t, seg_start);
        bool changed = (o_tag != MergeOpcode::EQUAL || t_tag != MergeOpcode::EQUAL);

        size_t seg_end = cuts[idx + 1];
        size_t j = idx + 1;
        if (changed) {
            // Merge consecutive changed cut-segments into a single piece so we
            // don't split one logical edit into several conflict blocks.
            while (j + 1 < cuts.size()) {
                size_t next_start = cuts[j];
                MergeOpcode::Tag no_tag = merge_find_status(ops_o, next_start);
                MergeOpcode::Tag nt_tag = merge_find_status(ops_t, next_start);
                if (no_tag == MergeOpcode::EQUAL && nt_tag == MergeOpcode::EQUAL) break;
                seg_end = cuts[j + 1];
                j++;
            }
        }
        merge_classify_and_push(pieces, base, ours, theirs, ops_o, ops_t, seg_start, seg_end,
                                 o_tag != MergeOpcode::EQUAL, t_tag != MergeOpcode::EQUAL, has_conflict);
        idx = j;
    }

    // A trailing insert-only change at end-of-file (a1 == n) never becomes a
    // segment start in the loop above (all segment starts are < n), so handle
    // it as one extra, dedicated final segment.
    bool o_trail = false, t_trail = false;
    for (const auto &op : ops_o) if (op.a1 == op.a2 && op.a1 == n) o_trail = true;
    for (const auto &op : ops_t) if (op.a1 == op.a2 && op.a1 == n) t_trail = true;
    if (o_trail || t_trail) {
        merge_classify_and_push(pieces, base, ours, theirs, ops_o, ops_t, n, n, o_trail, t_trail, has_conflict);
    }

    return pieces;
}
string merge::format_branch_name(const string &raw_name){
    string formatted = raw_name;
    return formatted;
}
void merge::execute(const string &current_branch, const string &incoming_branch, bool use_ai){
    string target_branch = format_branch_name(current_branch);
    string source_branch = format_branch_name(incoming_branch);
    cout << "\033[1;36mVoxel Merge: Merging '" << source_branch << "' into '" << target_branch << "'...\033[0m\n";
    if (use_ai) {
        cout << "\033[1;35m[AI Merge] AI-assisted conflict resolution is ENABLED for this merge.\033[0m\n";
    }
    string base_commit = merge::find_lowest_common_ancestor(source_branch, target_branch);
    if (base_commit.empty()) {
        cerr << "\033[1;31mError: No common ancestor found. Cannot proceed with merge. Try making a commit on both branches.\033[0m\n";
        return;
    }
    if (!setup_sandbox()) return;

    // 🔥 Build the file list from the UNION of the current workspace, the
    // merge base, and both branch tips — not just what happens to exist in
    // the workspace right now. Previously this only used
    // FileSystem::list_workspace_files(), so a file that was added only on
    // the incoming branch (and therefore never checked out into the current
    // workspace) was silently skipped and never made it into 'ours' after
    // the merge, for both text and media files.
    set<string> all_files_set;
    for (const auto &f : FileSystem::list_workspace_files())
        all_files_set.insert(f);
    for (const auto &f : get_all_filepaths_from_commit(base_commit))
        all_files_set.insert(f);
    for (const auto &f : get_all_filepaths_from_commit(merge::get_branch_commit(target_branch)))
        all_files_set.insert(f);
    for (const auto &f : get_all_filepaths_from_commit(merge::get_branch_commit(source_branch)))
        all_files_set.insert(f);
    vector<string> all_files(all_files_set.begin(), all_files_set.end());

    bool has_conflicts = false;
    for (const auto& file : all_files){
        string ext = fs::path(file).extension().string();
        bool conflict;
        if (Commands::should_ignore_extension(ext)) {
            // Binary/media asset (mp4, jpg, blend, etc.) - can't be line-merged
            // with dtl::Diff3, so it gets its own byte-level merge path instead
            // of being skipped entirely.
            // Media/binary files can't be semantically merged by the AI text
            // agent, so they always fall through to the manual size/keep
            // prompt regardless of --ai.
            conflict = process_media_file_merge(file, target_branch, source_branch, base_commit);
        } else {
            conflict = process_file_merge(file, target_branch, source_branch, base_commit, use_ai);
        }
        if (conflict) {
            has_conflicts = true;
        }
    }
    if (has_conflicts) {
        cout << "\n\033[1;33mConflicts were resolved in the sandbox.\033[0m\n";
    }
    cout << "\n\033[1;32mSandbox merge complete.\033[0m\n";
    cout << "Type 'yes' to finalize this merge and apply changes to your active workspace: ";
    
    string user_confirmation;
    cin >> user_confirmation;
    if (user_confirmation == "yes" || user_confirmation == "Yes" || user_confirmation == "YES" || user_confirmation == "y" || user_confirmation == "Y"){
        apply_sandbox_to_workspace();
        cout << "\033[1;32mMerge applied successfully! Workspace updated.\033[0m\n";

        // 🔥 Record the merge as a real commit on the target branch. Merging
        // only ever touched the workspace + files on disk — it never moved
        // the branch ref forward, so as far as any later "restore to this
        // branch's latest commit" call is concerned, nothing happened here
        // and the pre-merge commit is still the truth. That mismatch is what
        // silently reverts merged files (media and text alike) the moment
        // anything downstream re-syncs the workspace to HEAD. Re-tracking
        // the now-merged workspace and committing it closes that gap.
        Commands::track_all_files();
        Commands::commit_changes("Merge branch '" + source_branch + "' into '" + target_branch + "'");
    }
    else {
        cout << "\033[1;31mMerge aborted by user. Workspace remains untouched.\033[0m\n";
    }
    merge::cleanup_sandbox();
}
static vector<string> split_lines(const string& text) {
    vector<string> lines;
    stringstream ss(text);
    string line;
    while (getline(ss, line)) {
        lines.push_back(line);
    }
    return lines;
}
bool merge::process_file_merge(const string &filepath, const string &target_branch, const string &source_branch, const string &base_commit, bool use_ai) {
    string base_content = merge::get_file_content_from_commit(base_commit, filepath);
    string ours_content = FileSystem::read_file_to_string(filepath);
    string theirs_content = merge::get_file_content_from_commit(merge::get_branch_commit(source_branch), filepath);
 
#ifdef VOXEL_MERGE_DEBUG
    cout << "[merge-debug] " << filepath << "\n"
         << "  base_commit hash : " << base_commit << "\n"
         << "  base   (" << base_content.size()   << " bytes): " << base_content   << "\n"
         << "  ours   (" << ours_content.size()   << " bytes): " << ours_content   << "\n"
         << "  theirs (" << theirs_content.size() << " bytes): " << theirs_content << "\n";
#endif
 
    if (ours_content == theirs_content) {
#ifdef VOXEL_MERGE_DEBUG
        cout << "  -> path: ours == theirs, nothing to do\n";
#endif
        return false;
    }
    if (base_content == ours_content && base_content != theirs_content) {
#ifdef VOXEL_MERGE_DEBUG
        cout << "  -> path: fast-forward, take THEIRS\n";
#endif
        fs::path dest = fs::path(SANDBOX_DIR) / filepath;
        fs::create_directories(dest.parent_path());
        ofstream out(dest);
        out << theirs_content;
        return false;
    }
    if (base_content == theirs_content && base_content != ours_content) {
#ifdef VOXEL_MERGE_DEBUG
        cout << "  -> path: fast-forward, keep OURS (theirs never changed this file)\n";
#endif
        return false;
    }
#ifdef VOXEL_MERGE_DEBUG
    cout << "  -> path: running dtl::Diff3 (both sides changed)\n";
#endif
    vector<string> base_lines = split_lines(base_content);
    vector<string> ours_lines = split_lines(ours_content);
    vector<string> theirs_lines = split_lines(theirs_content);
 
    // Run the Diff3 engine.
    // IMPORTANT: dtl::Diff3's signature is Diff3(A, B, C) where B (the MIDDLE
    // argument) is the common ancestor that A and C are each diffed against
    // internally (diff_ba = diff(B,A), diff_bc = diff(B,C)). base_lines must
    // go in the middle slot, not the first one, or the merge logic computes
    // everything relative to the wrong reference point.
    dtl::Diff3<string> diff3(ours_lines, base_lines, theirs_lines);
    diff3.compose();
 
    if (diff3.merge()) {
        // DTL successfully merged line-by-line without overlapping conflicts!
        fs::path dest = fs::path(SANDBOX_DIR) / filepath;
        fs::create_directories(dest.parent_path());
        ofstream out(dest);
        
        // Write the auto-merged lines to the sandbox
        for (const auto& line : diff3.getMergedSequence()) {
            out << line << "\n";
        }
        return false; // Handled cleanly, no manual conflict!
    }
    if (use_ai) {
        bool resolved = merge::resolve_conflict_ai(filepath, base_content, ours_content, theirs_content,
                                                     target_branch, source_branch);
        if (resolved) {
            return true; // AI resolved it, still counts as a conflict that was handled
        }
        cout << "\033[1;33m[AI Merge] Falling back to manual resolution for " << filepath << ".\033[0m\n";
    }
    merge::resolve_conflict_interactive(filepath, base_lines, ours_lines, theirs_lines, target_branch, source_branch);
    return true;
 
}
static string fetch_raw_object(const string &object_hash) {
    if (object_hash.empty()) return "";
    string obj_path = ".voxel/objects/" + object_hash;
    if (!fs::exists(obj_path)) return "";
    return FileSystem::read_file_to_string(obj_path);
}
static string get_media_content_from_commit(const string &commit_hash, const string &filepath) {
    if (commit_hash.empty()) return "";
    string blob_hash = get_file_blob_hash_from_commit(commit_hash, filepath);
    return fetch_raw_object(blob_hash);
}
static bool process_media_file_merge(const string &filepath, const string &target_branch, const string &source_branch, const string &base_commit) {
    string base_content = get_media_content_from_commit(base_commit, filepath);
    string ours_content = FileSystem::read_file_to_string(filepath);
    string theirs_content = get_media_content_from_commit(merge::get_branch_commit(source_branch), filepath);

    if (ours_content == theirs_content) {
        // Identical on both sides (byte-for-byte) - nothing to do.
        return false;
    }
    if (base_content == ours_content && base_content != theirs_content) {
        // Fast-forward: we never touched it, they did - take theirs.
        fs::path dest = fs::path(SANDBOX_DIR) / filepath;
        fs::create_directories(dest.parent_path());
        ofstream out(dest, ios::binary);
        out.write(theirs_content.data(), static_cast<streamsize>(theirs_content.size()));
        return false;
    }
    if (base_content == theirs_content && base_content != ours_content) {
        // Fast-forward: they never touched it, we did - keep ours as-is.
        // (Nothing written to the sandbox, so apply_sandbox_to_workspace
        // leaves the current workspace file untouched.)
        return false;
    }

    // True conflict: both sides changed this media/binary asset differently.
    // There's no way to line-merge a video/image/blend file, so drop a plain
    // text note into the sandbox describing the size mismatch (mirrors how
    // text conflicts leave a reviewable markers file), in addition to asking
    // directly. The note is named after the media file (with a
    // ".CONFLICT_INFO.txt" suffix) so it never collides with the real
    // filepath the media file itself would occupy.
    fs::path info_dest = fs::path(SANDBOX_DIR) / (filepath + ".CONFLICT_INFO.txt");
    fs::create_directories(info_dest.parent_path());

    auto write_conflict_info = [&](const string &resolution_note) {
        ofstream info_out(info_dest, ios::trunc);
        info_out << "Merge conflict: binary/media file changed on both sides\n";
        info_out << "File: " << filepath << "\n\n";
        info_out << "  Ours   (" << target_branch << "): " << merge_format_human_size(ours_content.size())
                  << " (" << ours_content.size()   << " bytes)\n";
        info_out << "  Theirs (" << source_branch << "): " << merge_format_human_size(theirs_content.size())
                  << " (" << theirs_content.size() << " bytes)\n\n";
        info_out << "This is a binary/media file and can't be automatically line-merged.\n";
        if (resolution_note.empty()) {
            info_out << "Awaiting resolution: [1] Keep OURS   [2] Keep THEIRS   [3] Keep BOTH\n";
        } else {
            info_out << "Resolution: " << resolution_note << "\n";
        }
    };
    write_conflict_info("");

    cout << "\033[1;31mMedia conflict detected in: " << filepath << "\033[0m\n";
    cout << "  Ours   (" << target_branch << "): " << merge_format_human_size(ours_content.size())
         << " (" << ours_content.size()   << " bytes)\n";
    cout << "  Theirs (" << source_branch << "): " << merge_format_human_size(theirs_content.size())
         << " (" << theirs_content.size() << " bytes)\n";
    cout << "This is a binary/media file and can't be automatically line-merged.\n";
    cout << "  A summary was also written to " << info_dest.string() << "\n";
    cout << "  [1] Keep OURS\n";
    cout << "  [2] Keep THEIRS\n";
    cout << "  [3] Keep BOTH (theirs saved alongside as a renamed copy)\n";
    cout << "Selection [1-3]: ";
    int choice = 0;
    cin >> choice;

    if (choice == 2) {
        fs::path dest = fs::path(SANDBOX_DIR) / filepath;
        fs::create_directories(dest.parent_path());
        ofstream out(dest, ios::binary);
        out.write(theirs_content.data(), static_cast<streamsize>(theirs_content.size()));
        cout << "Kept THEIRS for " << filepath << "\n";
        write_conflict_info("Kept THEIRS (" + source_branch + ")");
    } else if (choice == 3) {
        // Ours stays untouched at its original path (nothing written there).
        // Theirs gets saved alongside it under a branch-suffixed name so
        // nothing is silently discarded.
        fs::path original(filepath);
        fs::path parent = original.parent_path();
        fs::path renamed_name = parent / (original.stem().string() + "_" + source_branch + original.extension().string());

        fs::path renamed_dest = fs::path(SANDBOX_DIR) / renamed_name;
        fs::create_directories(renamed_dest.parent_path());
        ofstream out(renamed_dest, ios::binary);
        out.write(theirs_content.data(), static_cast<streamsize>(theirs_content.size()));
        cout << "Kept BOTH: ours unchanged, theirs saved as " << renamed_name.string() << "\n";
        write_conflict_info("Kept BOTH: ours unchanged, theirs saved alongside as " + renamed_name.string());
    } else {
        if (choice != 1) {
            cout << "Invalid choice. Defaulting to OURS to protect workspace files.\n";
        }
        // Keep OURS: nothing written to the sandbox, workspace file untouched.
        cout << "Kept OURS for " << filepath << "\n";
        write_conflict_info("Kept OURS (" + target_branch + ")");
    }

    return true; // Required a manual decision, so it counts as a resolved conflict.
}
bool merge::resolve_conflict_ai(const string &filepath, const string &base_content, const string &ours_content,
                                 const string &theirs_content, const string &target_branch, const string &source_branch) {
    cout << "\033[1;35m[AI Merge] Conflict in " << filepath << " — asking Voxel AI to resolve...\033[0m\n";

    string merged_code = ai::resolve_merge_conflict(filepath, base_content, ours_content, theirs_content,
                                                      target_branch, source_branch);

    if (merged_code.empty()) {
        return false; // let the caller fall back to interactive resolution
    }

    fs::path dest = fs::path(SANDBOX_DIR) / filepath;
    fs::create_directories(dest.parent_path());
    ofstream out(dest, ios::trunc);
    out << merged_code;
    out.close();

    cout << "\033[1;32m[AI Merge] " << filepath << " auto-resolved and staged in " << SANDBOX_DIR << "/\033[0m\n";
    return true;
}
void merge::resolve_conflict_interactive(const string &filepath,const vector<string> &base_lines,const vector<string> &ours_lines,const vector<string> &theirs_lines,const string &target_branch,const string &source_branch) {
    bool has_conflict = false;
    vector<MergePiece> pieces = build_three_way_pieces(base_lines, ours_lines, theirs_lines, has_conflict);

    fs::path dest = fs::path(SANDBOX_DIR) / filepath;
    fs::create_directories(dest.parent_path());

    cout << "\033[1;31mConflict detected in: " << filepath << "\033[0m\n";

    // Phase 1: write the file with markers around ONLY the real conflicting
    // hunks (unchanged and one-sided-change lines are auto-applied) so the
    // person can review it in their editor before choosing a resolution.
    {
        ofstream out(dest, ios::trunc);
        for (const auto &piece : pieces) {
            switch (piece.kind) {
                case MergePiece::COMMON:
                case MergePiece::OURS_ONLY:
                    for (const auto &l : piece.ours_lines) out << l << "\n";
                    break;
                case MergePiece::THEIRS_ONLY:
                    for (const auto &l : piece.theirs_lines) out << l << "\n";
                    break;
                case MergePiece::CONFLICT:
                    out << "<<<<<<< OURS (" << target_branch << ") (Current Change)\n";
                    for (const auto &l : piece.ours_lines) out << l << "\n";
                    out << "=======\n";
                    for (const auto &l : piece.theirs_lines) out << l << "\n";
                    out << ">>>>>>> THEIRS (" << source_branch << ") (Incoming Change)\n";
                    break;
            }
        }
    }

    cout << "--------------------------------------------------\n";
    cout << "File: " << filepath << " is now in " << SANDBOX_DIR << "/\n";
    cout << "Please select a resolution for the conflicting section(s):\n";
    cout << "  [1] Keep OURS\n";
    cout << "  [2] Keep THEIRS\n";
    cout << "  [3] Keep BOTH (OURS then THEIRS)\n";
    cout << "Selection [1-3]: ";
    int choice = 0;
    cin >> choice;

    // Phase 2: rewrite with just the CONFLICT pieces resolved per the choice.
    // COMMON / OURS_ONLY / THEIRS_ONLY pieces are already correctly merged
    // and are left untouched by the choice.
    ofstream resolved_out(dest, ios::trunc);
    for (const auto &piece : pieces) {
        switch (piece.kind) {
            case MergePiece::COMMON:
            case MergePiece::OURS_ONLY:
                for (const auto &l : piece.ours_lines) resolved_out << l << "\n";
                break;
            case MergePiece::THEIRS_ONLY:
                for (const auto &l : piece.theirs_lines) resolved_out << l << "\n";
                break;
            case MergePiece::CONFLICT:
                if (choice == 1) {
                    for (const auto &l : piece.ours_lines) resolved_out << l << "\n";
                } else if (choice == 2) {
                    for (const auto &l : piece.theirs_lines) resolved_out << l << "\n";
                } else if (choice == 3) {
                    for (const auto &l : piece.ours_lines) resolved_out << l << "\n";
                    for (const auto &l : piece.theirs_lines) resolved_out << l << "\n";
                } else {
                    cout << "Invalid choice. Defaulting to OURS to protect workspace code.\n";
                    for (const auto &l : piece.ours_lines) resolved_out << l << "\n";
                }
                break;
        }
    }
    resolved_out.close();
    cout << "Conflict resolved in sandbox for " << filepath << "\n";
}
bool merge::setup_sandbox() {
    try {
        if (fs::exists(SANDBOX_DIR)) {
            fs::remove_all(SANDBOX_DIR);
        }
        fs::create_directories(SANDBOX_DIR);
        return true;
    } catch (const exception& e) {
        cerr << "Failed to create sandbox: " << e.what() << "\n";
        return false;
    }
}
void merge::cleanup_sandbox() {
    if (fs::exists(SANDBOX_DIR)) {
        fs::remove_all(SANDBOX_DIR);
    }
}
void merge::apply_sandbox_to_workspace() {
    for (const auto& entry : fs::recursive_directory_iterator(SANDBOX_DIR)) {
        if (entry.is_regular_file()) {
            string sandbox_path = entry.path().string();
            string real_path = sandbox_path.substr(SANDBOX_DIR.length() + 1); 
            fs::copy_file(sandbox_path, real_path, fs::copy_options::overwrite_existing);
        }
    }
}
string merge::get_branch_commit(const string& branch_name) {
    string ref_path = ".voxel/refs/heads/" + branch_name;
    if (fs::exists(ref_path)) {
        return FileSystem::read_file_to_string(ref_path);
    }
    return "";
}
string merge::get_file_content_from_commit(const string& commit_hash, const string& filepath) {
    if (commit_hash.empty()) return "";

    string blob_hash = get_file_blob_hash_from_commit(commit_hash, filepath);
    if (blob_hash.empty()) return "";

    return fetch_decompress(blob_hash);
}
string merge::find_lowest_common_ancestor(const string& branchA, const string& branchB) {
    string commitA = get_branch_commit(branchA);
    string commitB = get_branch_commit(branchB);
 
    if (commitA.empty() || commitB.empty()) return "";
    if (commitA == commitB) return commitA; // They are on the exact same commit
 
    
    map<string, Commands::CommitNode> graph = Commands::build_complete_repo_graph().second;
 
    set<string> history_of_A;
    string currentA = commitA;
 
    
    while (!currentA.empty() && currentA != "NONE") {
        history_of_A.insert(currentA);
        if (graph.find(currentA) != graph.end()) {
            currentA = graph[currentA].parent;
        } else {
            break;
        }
    }
    string currentB = commitB;
    while (!currentB.empty() && currentB != "NONE") {
        if (history_of_A.count(currentB) > 0) {
            return currentB; // Found the LCA!
        }
        if (graph.find(currentB) != graph.end()) {
            currentB = graph[currentB].parent;
        } else {
            break;
        }
    }
 
    return ""; 
}