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
namespace fs = std::filesystem;
using namespace std;
#define RESET       "\033[0m"
#define RED         "\033[31m"
#define GREEN       "\033[32m"
#define YELLOW      "\033[33m"
#define CYAN        "\033[36m"
#define BOLD        "\033[1m"
#define DIM         "\033[2m"



std::string diffEngine::generate_block_hash(const std::vector<std::string>& lines) {
    std::string combined = "";
    for (const auto& line : lines) {
        // If the line contains something OTHER than spaces, tabs, or newlines
        if (line.find_first_not_of(" \t\r\n") != std::string::npos) {
            combined += line + "\n";
        }
    }
    return Hashing::generate_sha256(combined); 
}

std::vector<Block> diffEngine::parse_file(const std::string& filepath) {
    std::string file_content = FileSystem::read_file_to_string(filepath);
    if (file_content.empty()) return std::vector<Block>();
    return diffEngine::parse_memory(file_content);
}

bool diffEngine::is_scope_header(const std::string& raw_line, std::string& out_scope_name) {
    size_t start = raw_line.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return false;
    
    std::string line = raw_line.substr(start);
    
    // Sniff for Structural Keywords (Python & C++ friendly)
    if (line.find("def ") == 0 || line.find("class ") == 0 || 
        line.find("struct ") == 0 || line.find("namespace ") == 0 ||
        line.find("for ") == 0 || line.find("for(") == 0 ||
        line.find("while ") == 0 || line.find("while(") == 0 ||
        line.find("if ") == 0 || line.find("if(") == 0 ||
        line.find("elif ") == 0 || line.find("else if ") == 0|| line.find("else if(") == 0 || line.find("else ") == 0 ||
        line.find("switch ") == 0 || line.find("switch(") == 0|| line.find("else ") == 0){
        
        // Clean up the scope name for the UI (remove trailing colons or braces)
        size_t end = line.find_last_not_of(" {:\r\n");
        out_scope_name = line.substr(0, end + 1);
        return true;
    }
    return false;
}

std::vector<Block> diffEngine::parse_memory(const std::string& raw_content) {
    std::vector<Block> blocks;
    std::stringstream stream(raw_content);
    std::string line;
    
    int line_num = 1;
    Block current_block;
    current_block.start_line = 1;
    current_block.scope = "Global Scope";
    
    std::string scope_name;

    while (std::getline(stream, line)) {
        // If we hit a new structural header, package the old block and start a new one
        if (is_scope_header(line, scope_name)) {
            if (!current_block.lines.empty()) {
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
    if (!current_block.lines.empty()) {
        current_block.end_line = line_num - 1;
        current_block.content_hash = generate_block_hash(current_block.lines);
        blocks.push_back(current_block);
    }

    return blocks;
}

std::vector<DiffResult> diffEngine::analyze_diff(const std::vector<Block>& old_blocks, const std::vector<Block>& new_blocks) {
    std::vector<DiffResult> results;
    std::vector<bool> old_matched(old_blocks.size(), false);
    std::vector<bool> new_matched(new_blocks.size(), false);

    // Pass 1: Exact structural identity match (Same scope name)
    for (size_t i = 0; i < old_blocks.size(); i++) {
        for (size_t j = 0; j < new_blocks.size(); j++) {
            if (!new_matched[j] && old_blocks[i].scope == new_blocks[j].scope) {
                DiffResult res;
                res.old_block = old_blocks[i];
                res.new_block = new_blocks[j];
                
                if (old_blocks[i].content_hash == new_blocks[j].content_hash) {
                    res.type = UNCHANGED;
                } else {
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
    for (size_t i = 0; i < old_blocks.size(); i++) {
        if (old_matched[i]) continue;
        
        for (size_t j = 0; j < new_blocks.size(); j++) {
            if (new_matched[j]) continue;

            // Count how many identical lines of code they share inside
            int shared_lines = 0;
            for (const auto& old_line : old_blocks[i].lines) {
                // Ignore whitespace lines for the similarity check
                if (old_line.find_first_not_of(" \t\r\n") != std::string::npos) {
                    if (std::find(new_blocks[j].lines.begin(), new_blocks[j].lines.end(), old_line) != new_blocks[j].lines.end()) {
                        shared_lines++;
                    }
                }
            }

            // If they share at least 1 line of actual code, they are the same block!
            if (shared_lines > 0) {
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
    for (size_t i = 0; i < old_blocks.size(); i++) {
        if (!old_matched[i]) {
            DiffResult res;
            res.type = DELETED;
            res.old_block = old_blocks[i];
            results.push_back(res);
        }
    }

    // Pass 3: Catch Unmatched Additions
    for (size_t j = 0; j < new_blocks.size(); j++) {
        if (!new_matched[j]) {
            DiffResult res;
            res.type = ADDED;
            res.new_block = new_blocks[j];
            results.push_back(res);
        }
    }

    return results;
}

static void render_granular_diff(const std::vector<std::string>& old_lines, const std::vector<std::string>& new_lines, 
                                 int old_start, int new_start, 
                                 int& lines_ins, int& chars_ins, int& lines_del, int& chars_del) {
    
    size_t i = 0, j = 0;
    
    while (i < old_lines.size() || j < new_lines.size()) {
        if (i < old_lines.size() && j < new_lines.size() && old_lines[i] == new_lines[j]) {
            // UNCHANGED LINE: Skip printing
            i++; 
            j++;
        } 
        else if (i < old_lines.size() && (j >= new_lines.size() || 
                 std::find(new_lines.begin() + j, new_lines.end(), old_lines[i]) == new_lines.end())) {
            
            // DELETED LINE: Only process if it's NOT just whitespace
            if (old_lines[i].find_first_not_of(" \t\r\n") != std::string::npos) {
                std::cout << DIM << std::setw(4) << (old_start + i) << " │ " << RESET << RED << "- " << old_lines[i] << RESET << "\n";
                lines_del++;
                chars_del += old_lines[i].length();
            }
            i++;
        } 
        else {
            // ADDED LINE: Only process if it's NOT just whitespace
            if (new_lines[j].find_first_not_of(" \t\r\n") != std::string::npos) {
                std::cout << DIM << std::setw(4) << (new_start + j) << " │ " << RESET << GREEN << "+ " << new_lines[j] << RESET << "\n";
                lines_ins++;
                chars_ins += new_lines[j].length();
            }
            j++;
        }
    }
}

void diffEngine::render_diff(const std::vector<DiffResult>& results, const std::string& fileA, const std::string& fileB) {
    std::cout << BOLD << CYAN << "\n┌──────────────────────────────────────────────────────────┐\n";
    std::cout << "│  VOXEL DIFF GRAPH: " << fileA << " ➔ " << fileB << "\n";
    std::cout << "└──────────────────────────────────────────────────────────┘\n" << RESET;
    
    int lines_inserted = 0, chars_inserted = 0;
    int lines_deleted = 0, chars_deleted = 0;
    bool has_changes = false;

    for (const auto& res : results) {
        if (res.type == UNCHANGED) continue;
        
        has_changes = true;
        
        if (res.type == MODIFIED) {
            std::cout << "\nScope: " << BOLD << res.new_block.scope << RESET << "  " << YELLOW << "[MODIFIED]" << RESET << "\n";
            std::cout << DIM << " ────────────────────────────────────────────────────────\n" << RESET;
            
            render_granular_diff(res.old_block.lines, res.new_block.lines, 
                                 res.old_block.start_line, res.new_block.start_line, 
                                 lines_inserted, chars_inserted, lines_deleted, chars_deleted);
        }
        else if (res.type == ADDED) {
            std::cout << "\nScope: " << BOLD << res.new_block.scope << RESET << "  " << GREEN << "[ADDED]" << RESET << "\n";
            std::cout << DIM << " ────────────────────────────────────────────────────────\n" << RESET;
            int ln = res.new_block.start_line;
            for (const auto& line : res.new_block.lines) {
                std::cout << DIM << std::setw(4) << ln++ << " │ " << RESET << GREEN << "+ " << line << RESET << "\n";
                lines_inserted++;
                chars_inserted += line.length();
            }
        }
        else if (res.type == DELETED) {
            std::cout << "\nScope: " << BOLD << res.old_block.scope << RESET << "  " << RED << "[DELETED]" << RESET << "\n";
            std::cout << DIM << " ────────────────────────────────────────────────────────\n" << RESET;
            int ln = res.old_block.start_line;
            for (const auto& line : res.old_block.lines) {
                std::cout << DIM << std::setw(4) << ln++ << " │ " << RESET << RED << "- " << line << RESET << "\n";
                lines_deleted++;
                chars_deleted += line.length();
            }
        }
    }

    if (!has_changes) {
        std::cout << "\n" << DIM << "  No structural changes detected." << RESET << "\n";
    }

    // SUMMARY FOOTER
    std::cout << "\n" << DIM << " ────────────────────────────────────────────────────────\n" << RESET;
    std::cout << BOLD << "DIFF SUMMARY:\n" << RESET;
    std::cout << GREEN << "    + " << lines_inserted << " lines inserted (" << chars_inserted << " characters)\n" << RESET;
    std::cout << RED   << "    - " << lines_deleted  << " lines deleted  (" << chars_deleted  << " characters)\n" << RESET;
    std::cout << DIM << " ────────────────────────────────────────────────────────\n\n" << RESET;
}

void diffEngine::run_engine_on_file(const std::string& filepath, const std::string& old_content, const std::string& new_content) {
    std::vector<Block> old_blocks = parse_memory(old_content);
    std::vector<Block> new_blocks = parse_memory(new_content);
    
    std::vector<DiffResult> results = analyze_diff(old_blocks, new_blocks);
    
    // File names for UI
    std::string old_name = filepath + " (Old)";
    std::string new_name = filepath + " (New)";
    
    render_diff(results, old_name, new_name);
}


static string fetch_decompress(const std::string& object_hash){
    if (object_hash.empty()) return "";
    string src_path = ".voxel/objects/" + object_hash;
    string tmp_path = ".voxel/tmp_diff_" + object_hash;
    if (Zstd::decompress_file(src_path, tmp_path)) {
        
        std::string content = FileSystem::read_file_to_string(tmp_path);
        fs::remove(tmp_path); 
        return content;
    }
    return "";
}
static std::string read_first_line(const std::string& path) {
    std::ifstream file(path);
    std::string line;
    if (file.is_open()) {
        std::getline(file, line);
    }
    return line;
}
static std::string get_branch_latest_commit(const std::string& branch_name) {
    std::string ref_path = ".voxel/refs/heads/" + branch_name;
    if (!fs::exists(ref_path)) {
        cout << RED << "Error: Branch '" << branch_name << "' does not exist.\n" << RESET;
        return "";
    }
    return read_first_line(ref_path);
}
static std::string find_branch_base_commit(std::string branch_tip_hash) {
   
    std::string trunk_tip_hash = get_branch_latest_commit("main");
    if (trunk_tip_hash.empty()) return branch_tip_hash;

    std::unordered_set<std::string> trunk_history;
    
   
    std::string current = trunk_tip_hash;
    while (!current.empty()) {
        trunk_history.insert(current);
        std::string commit_path = ".voxel/objects/" + current;
        if (!std::filesystem::exists(commit_path)) break;
        
        std::string commit_data = FileSystem::read_file_to_string(commit_path);
        std::istringstream stream(commit_data);
        std::string line;
        std::string parent = "";
        
        while (std::getline(stream, line)) {
            if (line.find("parent - ") == 0) {
                parent = line.substr(9); 
                break;
            }
        }
        current = parent; 
    }

    
    current = branch_tip_hash;
    std::string last_valid = current;
    
    while (!current.empty()) {
        if (trunk_history.count(current)) {
            return current; 
        }
        
        last_valid = current; 
        
        std::string commit_path = ".voxel/objects/" + current;
        if (!std::filesystem::exists(commit_path)) break;

        std::string commit_data = FileSystem::read_file_to_string(commit_path);
        std::istringstream stream(commit_data);
        std::string line;
        std::string parent = "";
        
        while (std::getline(stream, line)) {
            if (line.find("parent - ") == 0) {
                parent = line.substr(9);
                break;
            }
        }
        current = parent;
    }
    return last_valid; 
}
static std::string resolve_target_to_commit(const std::string& target) {
    if (target.length() == 64) {
        return target; 
    }
    return get_branch_latest_commit(target); 
}
static string get_current_branch_last_commit(){
    string current_branch = Commands::get_current_branch_name();
    return get_branch_latest_commit(current_branch);
}
static std::string find_root_commit(std::string current_commit_hash){
    string prev_hash = current_commit_hash;
    while (!current_commit_hash.empty()) {
        prev_hash = current_commit_hash;
        string commit_path = ".voxel/objects/" + current_commit_hash;
        string commit_data = FileSystem::read_file_to_string(commit_path);
        istringstream stream(commit_data);
        string line;
        bool found_parent = false;
        while (std::getline(stream, line)) {
            if (line.find("parent - ") == 0) {
                current_commit_hash = line.substr(9);
                found_parent = true;
                break;
            }
        }
        if (!found_parent) break;

    }
    return prev_hash;
}
static std::string get_file_blob_hash_from_commit(const std::string& commit_hash, const std::string& filepath) {
    if (commit_hash.empty()) return "";

    // 1. Read the Commit Object (Plain text)
    std::string commit_path = ".voxel/objects/" + commit_hash;
    if (!std::filesystem::exists(commit_path)) return "";
    std::string commit_data = FileSystem::read_file_to_string(commit_path);

    // 2. Find the 'tree - ' line, which points to your saved Index Copy hash
    std::istringstream commit_stream(commit_data);
    std::string line;
    std::string index_copy_hash = "";
    
    while (std::getline(commit_stream, line)) {
        if (line.find("tree - ") == 0) {
            index_copy_hash = line.substr(7); // "tree - " is 7 chars
            // Trim whitespace
            index_copy_hash.erase(index_copy_hash.find_last_not_of(" \n\r\t") + 1);
            break;
        }
    }

    if (index_copy_hash.empty()) return "";

    // 3. Read the Index Copy object directly
    std::string index_path = ".voxel/objects/" + index_copy_hash;
    if (!std::filesystem::exists(index_path)) return "";
    std::string index_data = FileSystem::read_file_to_string(index_path);

    // 4. Parse the Index Copy for the file mapping
    // This file contains: "filepath hash" (or similar)
    std::istringstream index_stream(index_data);
    while (std::getline(index_stream, line)) {
        // If the line contains our filename
        if (line.find(filepath) != std::string::npos) {
            
            // Extract the 64-character hash from this line
            std::istringstream line_stream(line);
            std::string word;
            while (line_stream >> word) {
                // Return the hash as soon as we find the 64-char string
                if (word.length() == 64) {
                    return word; 
                }
            }
        }
    }

    return ""; // File wasn't in this index
}
void diffEngine::route_diff(const std::vector<std::string>& args){
    vector<string> all_files = FileSystem::list_workspace_files();
    if (args.size() == 2 && fs::exists(args[0]) && fs::exists(args[1])) {
        std::string old_c = FileSystem::read_file_to_string(args[0]);
        std::string new_c = FileSystem::read_file_to_string(args[1]);
        diffEngine::run_engine_on_file(args[0] + " -> " + args[1], old_c, new_c);
        return;
    }
    string left_commit_hash = "";
    string right_commit_hash = "WORKSPACE";
    if (args.empty()) {
        cout << BOLD << CYAN << "Comparing Workspace vs Last Commit (Current Branch)...\n" << RESET;
        left_commit_hash = get_current_branch_last_commit();
    }
    else if (args.size() == 1) {
        std::string target = args[0];
        if (target == "Head" || target == "HEAD"|| target == "head") {
            cout << BOLD << CYAN << "Comparing Workspace vs Root Commit (Head)...\n" << RESET;
            left_commit_hash = find_root_commit(get_current_branch_last_commit());
        }
        else{
            cout << BOLD << CYAN << "Comparing Workspace vs Root Commit of branch '" << target << "'...\n" << RESET;
            left_commit_hash = find_branch_base_commit(resolve_target_to_commit(target));
        }
    }
    else if (args.size() == 2) {
        std::string target1 = args[0];
        std::string target2 = args[1];
        
        std::cout << BOLD << CYAN << "Comparing " << target1 << " vs " << target2 << "...\n" << RESET;
        
        left_commit_hash = resolve_target_to_commit(target1);
        right_commit_hash = resolve_target_to_commit(target2);
    }
    else {
        std::cerr << RED << "Error: Invalid number of arguments for voxel diff. Run voxel help for usage information.\n" << RESET;
        return;
    }
    for (const auto& file : all_files){
        if (Commands::should_ignore_extension(file)){
            continue;
        }
        string old_content = "";
        string new_content = "";
        if (!left_commit_hash.empty()) {
            string file_blob_hash = get_file_blob_hash_from_commit(left_commit_hash, file);
            if (!file_blob_hash.empty()) {
                old_content = fetch_decompress(file_blob_hash);
            }
        }
        if (right_commit_hash == "WORKSPACE") {
            new_content = FileSystem::read_file_to_string(file);
        }
        else if (!right_commit_hash.empty()) {
            std::string file_blob_hash = get_file_blob_hash_from_commit(right_commit_hash, file);
            if (!file_blob_hash.empty()) {
                new_content = fetch_decompress(file_blob_hash);
            }
        }
        diffEngine::run_engine_on_file(file, old_content, new_content);
    }
}












