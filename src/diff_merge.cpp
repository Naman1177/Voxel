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



std::string diffEngine::generate_block_hash(const std::vector<std::string> &lines)
{
    std::string combined = "";
    for (const auto &line : lines)
    {
    
        if (line.find_first_not_of(" \t\r\n") != std::string::npos)
        {
            combined += line + "\n";
        }
    }
    return Hashing::generate_sha256(combined);
}
std::vector<Block> diffEngine::parse_file(const std::string &filepath)
{
    std::string file_content = FileSystem::read_file_to_string(filepath);
    if (file_content.empty())
        return std::vector<Block>();
    return diffEngine::parse_memory(file_content);
}
bool diffEngine::is_scope_header(const std::string &raw_line, std::string &out_scope_name)
{
    size_t start = raw_line.find_first_not_of(" \t\r\n");
    if (start == std::string::npos)
        return false;

    std::string line = raw_line.substr(start);

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
std::vector<Block> diffEngine::parse_memory(const std::string &raw_content)
{
    std::vector<Block> blocks;
    std::stringstream stream(raw_content);
    std::string line;

    int line_num = 1;
    Block current_block;
    current_block.start_line = 1;
    current_block.scope = "Global Scope";

    std::string scope_name;

    while (std::getline(stream, line))
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
std::vector<DiffResult> diffEngine::analyze_diff(const std::vector<Block> &old_blocks, const std::vector<Block> &new_blocks)
{
    std::vector<DiffResult> results;
    std::vector<bool> old_matched(old_blocks.size(), false);
    std::vector<bool> new_matched(new_blocks.size(), false);

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
                if (old_line.find_first_not_of(" \t\r\n") != std::string::npos)
                {
                    if (std::find(new_blocks[j].lines.begin(), new_blocks[j].lines.end(), old_line) != new_blocks[j].lines.end())
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
static void render_granular_diff(const std::vector<std::string> &old_lines, const std::vector<std::string> &new_lines,int old_start, int new_start,int &lines_ins, int &chars_ins, int &lines_del, int &chars_del){

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
                                          std::find(new_lines.begin() + j, new_lines.end(), old_lines[i]) == new_lines.end()))
        {

            // DELETED LINE: Only process if it's NOT just whitespace
            if (old_lines[i].find_first_not_of(" \t\r\n") != std::string::npos)
            {
                std::cout << DIM << std::setw(4) << (old_start + i) << " │ " << RESET << RED << "- " << old_lines[i] << RESET << "\n";
                lines_del++;
                chars_del += old_lines[i].length();
            }
            i++;
        }
        else
        {
            // ADDED LINE: Only process if it's NOT just whitespace
            if (new_lines[j].find_first_not_of(" \t\r\n") != std::string::npos)
            {
                std::cout << DIM << std::setw(4) << (new_start + j) << " │ " << RESET << GREEN << "+ " << new_lines[j] << RESET << "\n";
                lines_ins++;
                chars_ins += new_lines[j].length();
            }
            j++;
        }
    }
}
void diffEngine::render_diff(const std::vector<DiffResult> &results, const std::string &fileA, const std::string &fileB)
{
    std::cout << BOLD << CYAN << "\n┌──────────────────────────────────────────────────────────┐\n";
    std::cout << "│  VOXEL DIFF GRAPH: " << fileA << " ➔ " << fileB << "\n";
    std::cout << "└──────────────────────────────────────────────────────────┘\n"
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
            std::cout << "\nScope: " << BOLD << res.new_block.scope << RESET << "  " << YELLOW << "[MODIFIED]" << RESET << "\n";
            std::cout << DIM << " ────────────────────────────────────────────────────────\n"
                      << RESET;

            render_granular_diff(res.old_block.lines, res.new_block.lines,
                                 res.old_block.start_line, res.new_block.start_line,
                                 lines_inserted, chars_inserted, lines_deleted, chars_deleted);
        }
        else if (res.type == ADDED){
            
                 
            std::cout << "\nScope: " << BOLD << res.new_block.scope << RESET << "  " << GREEN << "[ADDED]" << RESET << "\n";
            std::cout << DIM << " ────────────────────────────────────────────────────────\n"
                      << RESET;
            int ln = res.new_block.start_line;
            for (const auto &line : res.new_block.lines){
            
                if (line.find_first_not_of(" \t\r\n") == std::string::npos) {
                    ln++; 
                    continue; 
                }

                std::cout << DIM << std::setw(4) << ln++ << " | " << RESET << GREEN << "+ " << line << RESET << "\n";
                lines_inserted++;
                chars_inserted += line.length();
            }
        }
        else if (res.type == DELETED)
        {
            std::cout << "\nScope: " << BOLD << res.old_block.scope << RESET << "  " << RED << "[DELETED]" << RESET << "\n";
            std::cout << DIM << " ────────────────────────────────────────────────────────\n"
                      << RESET;
            int ln = res.old_block.start_line;
            for (const auto &line : res.old_block.lines){
                if (line.find_first_not_of(" \t\r\n") == std::string::npos) {
                    ln++; 
                    continue; 
                }

                std::cout << DIM << std::setw(4) << ln++ << " | " << RESET << RED << "- " << line << RESET << "\n";
                lines_deleted++;
                chars_deleted += line.length();
            }
        }
    }

    if (!has_changes)
    {
        std::cout << "\n"
                  << DIM << "  No structural changes detected." << RESET << "\n";
    }

    // SUMMARY FOOTER
    std::cout << "\n"
              << DIM << " ────────────────────────────────────────────────────────\n"
              << RESET;
    std::cout << BOLD << "DIFF SUMMARY:\n"
              << RESET;
    std::cout << GREEN << "    + " << lines_inserted << " lines inserted (" << chars_inserted << " characters)\n"
              << RESET;
    std::cout << RED << "    - " << lines_deleted << " lines deleted  (" << chars_deleted << " characters)\n"
              << RESET;
    std::cout << DIM << " ────────────────────────────────────────────────────────\n\n"
              << RESET;
}
void diffEngine::run_engine_on_file(const std::string &filepath, const std::string &old_content, const std::string &new_content)
{
    std::vector<Block> old_blocks = parse_memory(old_content);
    std::vector<Block> new_blocks = parse_memory(new_content);

    std::vector<DiffResult> results = analyze_diff(old_blocks, new_blocks);

    // File names for UI
    std::string old_name = filepath + " (Old)";
    std::string new_name = filepath + " (New)";

    render_diff(results, old_name, new_name);
}
static string fetch_decompress(const std::string &object_hash) {
    if (object_hash.empty()) return "";
    
    string src_path = ".voxel/objects/" + object_hash;
    string tmp_path = ".voxel/tmp_diff_" + object_hash;
    
    if (fs::exists(src_path)) {
        
        if (Zstd::decompress_file(src_path, tmp_path)) {
            std::string content = FileSystem::read_file_to_string(tmp_path);
            fs::remove(tmp_path);
            return content;
        }
        return FileSystem::read_file_to_string(src_path);
    }
    
    return "";
}
static std::string read_first_line(const std::string &path)
{
    std::ifstream file(path);
    std::string line;
    if (file.is_open())
    {
        std::getline(file, line);
    }
    return line;
}
static std::string get_branch_latest_commit(const std::string &branch_name)
{
    std::string ref_path = ".voxel/refs/heads/" + branch_name;
    if (!fs::exists(ref_path))
    {
        cout << RED << "Error: Branch '" << branch_name << "' does not exist.\n"
             << RESET;
        return "";
    }
    return read_first_line(ref_path);
}
static std::string find_branch_base_commit(std::string branch_tip_hash)
{

    std::string trunk_tip_hash = get_branch_latest_commit("main");
    if (trunk_tip_hash.empty())
        return branch_tip_hash;

    std::unordered_set<std::string> trunk_history;

    std::string current = trunk_tip_hash;
    while (!current.empty())
    {
        trunk_history.insert(current);
        std::string commit_path = ".voxel/objects/" + current;
        if (!std::filesystem::exists(commit_path))
            break;

        std::string commit_data = FileSystem::read_file_to_string(commit_path);
        std::istringstream stream(commit_data);
        std::string line;
        std::string parent = "";

        while (std::getline(stream, line))
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
    std::string last_valid = current;

    while (!current.empty())
    {
        if (trunk_history.count(current))
        {
            return current;
        }

        last_valid = current;

        std::string commit_path = ".voxel/objects/" + current;
        if (!std::filesystem::exists(commit_path))
            break;

        std::string commit_data = FileSystem::read_file_to_string(commit_path);
        std::istringstream stream(commit_data);
        std::string line;
        std::string parent = "";

        while (std::getline(stream, line))
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
static std::string resolve_target_to_commit(const std::string &target)
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
static std::string find_root_commit(std::string current_commit_hash)
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
        while (std::getline(stream, line))
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
static std::string get_file_blob_hash_from_commit(const std::string &commit_hash, const std::string &filepath) {
    if (commit_hash.empty()) return "";

    std::string commit_path = ".voxel/objects/" + commit_hash;
    if (!std::filesystem::exists(commit_path)) return "";
    
    std::string commit_data = FileSystem::read_file_to_string(commit_path);
    std::istringstream commit_stream(commit_data);
    std::string line;
    std::string index_copy_hash = "";

    // 1. Get the Tree Index hash
    while (std::getline(commit_stream, line)) {
        if (line.find("tree - ") == 0) {
            index_copy_hash = line.substr(7);
            index_copy_hash.erase(index_copy_hash.find_last_not_of(" \n\r\t") + 1);
            break;
        }
    }

    if (index_copy_hash.empty()) return "";

    std::string index_path = ".voxel/objects/" + index_copy_hash;
    if (!std::filesystem::exists(index_path)) return "";
    
    std::string index_data = FileSystem::read_file_to_string(index_path);
    std::istringstream index_stream(index_data);
    
    // 2. Extract just the filename to match safely regardless of how Voxel stores the path
    std::string target_filename = std::filesystem::path(filepath).filename().string();

    while (std::getline(index_stream, line)) {
        // Search using the clean filename
        if (line.find(target_filename) != std::string::npos) {
            std::istringstream line_stream(line);
            std::string word;
            while (line_stream >> word) {
                if (word.length() == 64) {
                    return word; // Successfully found the Blob Hash
                }
            }
        }
    }
    return ""; 
}
void diffEngine::report_media_file_diff(const std::string &file,
                                         const std::string &old_content,
                                         const std::string &new_content,
                                         bool old_existed, bool new_existed)
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
void diffEngine::route_diff(const std::vector<std::string> &args)
{
    vector<string> all_files = FileSystem::list_workspace_files();
    if (args.size() == 2 && fs::exists(args[0]) && fs::exists(args[1]))
    {
        std::string old_c = FileSystem::read_file_to_string(args[0]);
        std::string new_c = FileSystem::read_file_to_string(args[1]);
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
        std::string target = args[0];
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
        std::string target1 = args[0];
        std::string target2 = args[1];

        std::cout << BOLD << CYAN << "Comparing " << target1 << " vs " << target2 << "...\n"
                  << RESET;

        left_commit_hash = resolve_target_to_commit(target1);
        right_commit_hash = resolve_target_to_commit(target2);
    }
    else
    {
        std::cerr << RED << "Error: Invalid number of arguments for voxel diff. Run voxel help for usage information.\n"
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
            std::string file_blob_hash = get_file_blob_hash_from_commit(right_commit_hash, file);
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
void diffEngine::ai_diff(const std::vector<std::string> &args)
{
    vector<string> all_files = FileSystem::list_workspace_files();
    if (args.size() == 2 && fs::exists(args[0]) && fs::exists(args[1]))
    {
        std::string old_c = FileSystem::read_file_to_string(args[0]);
        std::string new_c = FileSystem::read_file_to_string(args[1]);
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
        std::string target = args[0];
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
        std::string target1 = args[0];
        std::string target2 = args[1];

        std::cout << BOLD << CYAN << "Comparing " << target1 << " vs " << target2 << "...\n"
                  << RESET;

        left_commit_hash = resolve_target_to_commit(target1);
        right_commit_hash = resolve_target_to_commit(target2);
    }
    else
    {
        std::cerr << RED << "Error: Invalid number of arguments for voxel diff. Run voxel help for usage information.\n"
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
            std::string file_blob_hash = get_file_blob_hash_from_commit(right_commit_hash, file);
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

// ---------------------------------------------------------------------------
// Minimal 3-way conflict rendering.
//
// dtl::Diff3::merge() only tells us success/failure; it doesn't expose a
// stable, version-independent way to get per-line "which side changed what"
// info for the conflict case. So when a conflict happens we run our own
// lightweight LCS-based line diff (base vs ours, base vs theirs) and use it
// to split the file into pieces that are either common to both, changed on
// only one side (auto-applied, no conflict), or genuinely changed on both
// sides (wrapped in <<<<<<< / ======= / >>>>>>> markers). This is what
// prevents unchanged context lines (e.g. "Line 2: Base") from being dumped
// twice into the conflict markers.
// ---------------------------------------------------------------------------
struct MergeOpcode {
    enum Tag { EQUAL, CHANGE } tag;
    size_t a1, a2; // range in `base`  [a1, a2)
    size_t b1, b2; // range in `other` [b1, b2)
};

// LCS-based line diff describing how `base` becomes `other`.
static std::vector<MergeOpcode> merge_diff_opcodes(const std::vector<std::string> &base,
                                                     const std::vector<std::string> &other) {
    size_t n = base.size(), m = other.size();
    std::vector<std::vector<int>> dp(n + 1, std::vector<int>(m + 1, 0));
    for (size_t i = n; i-- > 0;)
        for (size_t j = m; j-- > 0;)
            dp[i][j] = (base[i] == other[j]) ? dp[i + 1][j + 1] + 1
                                              : std::max(dp[i + 1][j], dp[i][j + 1]);

    std::vector<MergeOpcode> ops;
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

// p is "covered" by an opcode if it falls in [a1,a2); a zero-width insert
// (a1 == a2, i.e. pure addition with nothing deleted) is treated as covering
// exactly the point p == a1.
static MergeOpcode::Tag merge_find_status(const std::vector<MergeOpcode> &ops, size_t p) {
    for (const auto &op : ops) {
        if (op.a1 == op.a2) { if (op.a1 == p) return MergeOpcode::CHANGE; }
        else if (op.a1 <= p && p < op.a2) return op.tag;
    }
    return MergeOpcode::EQUAL;
}

// Reconstructs what `side` contributes for base range [rs, re). rs == re
// only happens for the dedicated end-of-file trailing-insert segment (see
// build_three_way_pieces); every other call has rs < re.
static std::vector<std::string> merge_extract_side_text(const std::vector<std::string> &base,
                                                          const std::vector<std::string> &side,
                                                          const std::vector<MergeOpcode> &ops,
                                                          size_t rs, size_t re) {
    std::vector<std::string> out;
    for (const auto &op : ops) {
        if (op.a1 == op.a2) {
            bool included = (rs < re) ? (op.a1 >= rs && op.a1 < re) : (op.a1 == rs);
            if (included) for (size_t k = op.b1; k < op.b2; k++) out.push_back(side[k]);
            continue;
        }
        if (op.a2 <= rs || op.a1 >= re) continue;
        if (op.tag == MergeOpcode::EQUAL) {
            size_t s = std::max(op.a1, rs), e = std::min(op.a2, re);
            for (size_t k = s; k < e; k++) out.push_back(base[k]);
        } else {
            for (size_t k = op.b1; k < op.b2; k++) out.push_back(side[k]);
        }
    }
    return out;
}

struct MergePiece {
    enum Kind { COMMON, OURS_ONLY, THEIRS_ONLY, CONFLICT } kind;
    std::vector<std::string> ours_lines;
    std::vector<std::string> theirs_lines;
};

static void merge_classify_and_push(std::vector<MergePiece> &pieces,
                                     const std::vector<std::string> &base,
                                     const std::vector<std::string> &ours,
                                     const std::vector<std::string> &theirs,
                                     const std::vector<MergeOpcode> &ops_o,
                                     const std::vector<MergeOpcode> &ops_t,
                                     size_t seg_start, size_t seg_end,
                                     bool o_changed, bool t_changed, bool &has_conflict) {
    std::vector<std::string> ours_text = merge_extract_side_text(base, ours, ops_o, seg_start, seg_end);
    std::vector<std::string> theirs_text = merge_extract_side_text(base, theirs, ops_t, seg_start, seg_end);
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

// Walks base/ours/theirs together and groups them into minimal common /
// one-sided-change / conflicting pieces, instead of dumping the entire
// ours/theirs file into every conflict block.
static std::vector<MergePiece> build_three_way_pieces(const std::vector<std::string> &base,
                                                        const std::vector<std::string> &ours,
                                                        const std::vector<std::string> &theirs,
                                                        bool &has_conflict) {
    std::vector<MergeOpcode> ops_o = merge_diff_opcodes(base, ours);
    std::vector<MergeOpcode> ops_t = merge_diff_opcodes(base, theirs);
    has_conflict = false;
    size_t n = base.size();

    std::set<size_t> cut_set{0, n};
    for (const auto &op : ops_o) { cut_set.insert(op.a1); cut_set.insert(op.a2); }
    for (const auto &op : ops_t) { cut_set.insert(op.a1); cut_set.insert(op.a2); }
    std::vector<size_t> cuts(cut_set.begin(), cut_set.end());

    std::vector<MergePiece> pieces;
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

// Merge class implementation
static const string SANDBOX_DIR = "sandbox_merge";
string merge::format_branch_name(const std::string &raw_name){
    string formatted = raw_name;
    replace(formatted.begin(), formatted.end(), '/', '@');
    return formatted;
}
void merge::execute(const std::string &current_branch, const std::string &incoming_branch){
    // target_branch = branch we are merging INTO (your current/active branch)
    // source_branch = branch we are merging FROM (the incoming branch)
    // NOTE: these two were previously swapped, which caused process_file_merge()
    // to fetch "theirs" content from the current branch instead of the incoming
    // branch, and caused the OURS/THEIRS labels in conflict markers to be wrong.
    string target_branch = format_branch_name(current_branch);
    string source_branch = format_branch_name(incoming_branch);
    cout << "\033[1;36mVoxel Merge: Merging '" << source_branch << "' into '" << target_branch << "'...\033[0m\n";
    string base_commit = merge::find_lowest_common_ancestor(source_branch, target_branch);
    if (base_commit.empty()) {
        std::cerr << "\033[1;31mError: No common ancestor found. Cannot proceed with merge. Try making a commit on both branches.\033[0m\n";
        return;
    }
    if (!setup_sandbox()) return;
    vector<string> all_files = FileSystem::list_workspace_files();
    bool has_conflicts = false;
    for (const auto& file : all_files){
        string ext = fs::path(file).extension().string();
        if (Commands::should_ignore_extension(ext)) {
            //add file system later
            continue;
        }
        bool conflict = process_file_merge(file, target_branch, source_branch, base_commit);
        if (conflict) {
            has_conflicts = true;
        }
    }
    if (has_conflicts) {
        std::cout << "\n\033[1;33mConflicts were resolved in the sandbox.\033[0m\n";
    }
    cout << "\n\033[1;32mSandbox merge complete.\033[0m\n";
    cout << "Type 'yes' to finalize this merge and apply changes to your active workspace: ";
    
    string user_confirmation;
    cin >> user_confirmation;
    if (user_confirmation == "yes" || user_confirmation == "Yes" || user_confirmation == "YES" || user_confirmation == "y" || user_confirmation == "Y"){
        apply_sandbox_to_workspace();
        cout << "\033[1;32mMerge applied successfully! Workspace updated.\033[0m\n";
    }
    else {
        cout << "\033[1;31mMerge aborted by user. Workspace remains untouched.\033[0m\n";
    }
    merge::cleanup_sandbox();
}
static std::vector<std::string> split_lines(const std::string& text) {
    std::vector<std::string> lines;
    std::stringstream ss(text);
    std::string line;
    while (std::getline(ss, line)) {
        lines.push_back(line);
    }
    return lines;
}
bool merge::process_file_merge(const std::string &filepath, const std::string &target_branch, const std::string &source_branch, const std::string &base_commit) {
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
        std::ofstream out(dest);
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
    std::vector<std::string> base_lines = split_lines(base_content);
    std::vector<std::string> ours_lines = split_lines(ours_content);
    std::vector<std::string> theirs_lines = split_lines(theirs_content);
 
    // Run the Diff3 engine.
    // IMPORTANT: dtl::Diff3's signature is Diff3(A, B, C) where B (the MIDDLE
    // argument) is the common ancestor that A and C are each diffed against
    // internally (diff_ba = diff(B,A), diff_bc = diff(B,C)). base_lines must
    // go in the middle slot, not the first one, or the merge logic computes
    // everything relative to the wrong reference point.
    dtl::Diff3<std::string> diff3(ours_lines, base_lines, theirs_lines);
    diff3.compose();
 
    if (diff3.merge()) {
        // DTL successfully merged line-by-line without overlapping conflicts!
        fs::path dest = fs::path(SANDBOX_DIR) / filepath;
        fs::create_directories(dest.parent_path());
        std::ofstream out(dest);
        
        // Write the auto-merged lines to the sandbox
        for (const auto& line : diff3.getMergedSequence()) {
            out << line << "\n";
        }
        return false; // Handled cleanly, no manual conflict!
    }
    merge::resolve_conflict_interactive(filepath, base_lines, ours_lines, theirs_lines, target_branch, source_branch);
    return true;
 
}
void merge::resolve_conflict_interactive(const std::string &filepath,
                                          const std::vector<std::string> &base_lines,
                                          const std::vector<std::string> &ours_lines,
                                          const std::vector<std::string> &theirs_lines,
                                          const std::string &target_branch,
                                          const std::string &source_branch) {
    bool has_conflict = false;
    std::vector<MergePiece> pieces = build_three_way_pieces(base_lines, ours_lines, theirs_lines, has_conflict);

    fs::path dest = fs::path(SANDBOX_DIR) / filepath;
    fs::create_directories(dest.parent_path());

    cout << "\033[1;31mConflict detected in: " << filepath << "\033[0m\n";

    // Phase 1: write the file with markers around ONLY the real conflicting
    // hunks (unchanged and one-sided-change lines are auto-applied) so the
    // person can review it in their editor before choosing a resolution.
    {
        std::ofstream out(dest, std::ios::trunc);
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
    std::ofstream resolved_out(dest, std::ios::trunc);
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
    } catch (const std::exception& e) {
        std::cerr << "Failed to create sandbox: " << e.what() << "\n";
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
            std::string sandbox_path = entry.path().string();
            std::string real_path = sandbox_path.substr(SANDBOX_DIR.length() + 1); 
            fs::copy_file(sandbox_path, real_path, fs::copy_options::overwrite_existing);
        }
    }
}
std::string merge::get_branch_commit(const std::string& branch_name) {
    std::string ref_path = ".voxel/refs/heads/" + branch_name;
    if (fs::exists(ref_path)) {
        return FileSystem::read_file_to_string(ref_path);
    }
    return "";
}
string merge::get_file_content_from_commit(const std::string& commit_hash, const std::string& filepath) {
    if (commit_hash.empty()) return "";

    // NOTE: commit_hash points at the COMMIT object, not a file blob.
    // We must first resolve the specific blob hash for `filepath` inside
    // this commit's tree (same lookup diff already does), then decompress
    // THAT blob. Previously this function tried to decompress the commit
    // object itself and ignored filepath entirely, which meant it always
    // returned "" for every file, tricking process_file_merge() into
    // thinking "theirs never changed this file" for everything and never
    // writing anything into sandbox_merge/.
    std::string blob_hash = get_file_blob_hash_from_commit(commit_hash, filepath);
    if (blob_hash.empty()) return "";

    return fetch_decompress(blob_hash);
}
std::string merge::find_lowest_common_ancestor(const std::string& branchA, const std::string& branchB) {
    std::string commitA = get_branch_commit(branchA);
    std::string commitB = get_branch_commit(branchB);
 
    if (commitA.empty() || commitB.empty()) return "";
    if (commitA == commitB) return commitA; // They are on the exact same commit
 
    // Fetch the repository graph map
    std::map<std::string, Commands::CommitNode> graph = Commands::build_complete_repo_graph().second;
 
    std::set<std::string> history_of_A;
    std::string currentA = commitA;
 
    // 1. Walk backward from Branch A to the beginning
    while (!currentA.empty() && currentA != "NONE") {
        history_of_A.insert(currentA);
        if (graph.find(currentA) != graph.end()) {
            currentA = graph[currentA].parent;
        } else {
            break;
        }
    }
 
    // 2. Walk backward from Branch B until a collision with A's history
    std::string currentB = commitB;
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