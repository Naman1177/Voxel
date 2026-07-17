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
    string buffer = "";
    
    for (const auto& raw_line : lines) {
        std::string line = raw_line;
        
        // 1. Strip C++ inline comments
        size_t comment_pos = line.find("//");
        if (comment_pos != std::string::npos) {
            line = line.substr(0, comment_pos);
        }
        
        // 2. Trim all leading and trailing whitespace
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start != std::string::npos) {
            size_t end = line.find_last_not_of(" \t\r\n");
            line = line.substr(start, end - start + 1);
            
            // 3. Only add to the hash buffer if there is actual code left
            if (!line.empty()) {
                buffer += line + "\n";
            }
        }
    }
    
    // Pass the pure, sanitized code string to your mbedTLS hardware layer
    return Hashing::generate_sha256(buffer); 
}

vector<Block> diffEngine::parse_file(const string& filepath) {
    string file_content = FileSystem::read_file_to_string(filepath);
    if (file_content.empty()) {
        return vector<Block>();
    }
    return diffEngine::parse_memory(file_content);
}

bool diffEngine::is_scope_header(const std::string& raw_line) {
   
    size_t start = raw_line.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return false; 
    
    size_t end = raw_line.find_last_not_of(" \t\r\n");
    std::string line = raw_line.substr(start, end - start + 1);

    if (line.back() == ';') return false;
    if (line.find("class ") == 0 || line.find("struct ") == 0 || 
        line.find("namespace ") == 0 || line.find("def ") == 0 || 
        line.find("interface ") == 0 || line.find("enum ") == 0) {
        
        if (line.find("using ") == 0) return false; 
        return true;
    }
    size_t open_paren = line.find('(');
    size_t close_paren = line.find(')');
    
    if (open_paren != std::string::npos && close_paren != std::string::npos && open_paren < close_paren) {
        if (line.find("if ") == 0 || line.find("if(") == 0 ||
            line.find("for ") == 0 || line.find("for(") == 0 ||
            line.find("while ") == 0 || line.find("while(") == 0 ||
            line.find("catch ") == 0 || line.find("catch(") == 0 ||
            line.find("switch ") == 0 || line.find("switch(") == 0 ||
            line.find("else ") == 0 || line.find("else{") == 0 ||
            line.find("else if ") == 0 || line.find("else if(") == 0||
            line.find("elif ") == 0 || line.find("elif(") == 0||
            line.find("try ") == 0 || line.find("try(") == 0) {
            return true;
        }
        char last_char = line.back();
        if (last_char == '{' || last_char == ')' || last_char == ':') {
            return true;
        }
    }

    return false;
}

static void render_granular_diff(const std::vector<std::string>& old_lines, const std::vector<std::string>& new_lines, int& lines_ins, int& chars_ins, int& lines_del, int& chars_del) {
    size_t i = 0, j = 0;
    
    // Simple side-by-side comparison
    while (i < old_lines.size() || j < new_lines.size()) {
        // Case 1: Lines are identical
        if (i < old_lines.size() && j < new_lines.size() && old_lines[i] == new_lines[j]) {
            std::cout << DIM << std::setw(4) << " " << " │ " << RESET << "  " << old_lines[i] << "\n";
            i++; j++;
        }
        // Case 2: Line deleted
        else if (i < old_lines.size() && (j >= new_lines.size() || old_lines[i] != new_lines[j])) {
            std::cout << DIM << std::setw(4) << " " << " │ " << RESET << RED << "- " << old_lines[i] << RESET << "\n";
            lines_del++;
            chars_del += old_lines[i].length();
            i++;
        }
        // Case 3: Line added
        else {
            std::cout << DIM << std::setw(4) << " " << " │ " << RESET << GREEN << "+ " << new_lines[j] << RESET << "\n";
            lines_ins++;
            chars_ins += new_lines[j].length();
            j++;
        }
    }
}
std::vector<Block> diffEngine::parse_memory(const std::string& raw_content) {
    std::vector<Block> blocks;
    std::stringstream stream(raw_content);
    std::string line;
    
    int line_num = 1;
    int scope_depth = 0; // NEW: The Brace Depth Tracker
    
    Block current_block;
    current_block.start_line = 1;
    std::string current_scope = "Global Scope";

    while (std::getline(stream, line)) {
        // 1. Update brace depth (count '{' and '}')
        for (char c : line) {
            if (c == '{') scope_depth++;
            else if (c == '}') scope_depth--;
        }
        if (scope_depth < 0) scope_depth = 0; // Prevent negative depth edge-cases

        // 2. Sniff for block headers
        if (is_scope_header(line)) {
            // NEW: Only update the name if we are at the top level of the function!
            if (scope_depth <= 1) { 
                size_t start = line.find_first_not_of(" \t\r");
                current_scope = (start != std::string::npos) ? line.substr(start) : line;
            }
        }

        // 3. Chunking Rule: Only cut on a blank line IF we are outside all scopes
        bool is_blank_line = (line.empty() || line.find_first_not_of(" \t\r") == std::string::npos);
        
        if (is_blank_line && scope_depth == 0) {
            if (!current_block.lines.empty()) {
                current_block.end_line = line_num - 1;
                current_block.scope = current_scope;
                current_block.content_hash = generate_block_hash(current_block.lines);
                blocks.push_back(current_block);
                
                current_block = Block(); // Clean up state
                current_block.start_line = line_num + 1; // Mark start of next block
            }
        } else {
            current_block.lines.push_back(line);
        }
        line_num++;
    }

    // Capture residual block at the end of the file
    if (!current_block.lines.empty()) {
        current_block.end_line = line_num - 1;
        current_block.scope = current_scope;
        current_block.content_hash = generate_block_hash(current_block.lines);
        blocks.push_back(current_block);
    }

    return blocks;
}

vector<DiffResult> diffEngine::analyze_diff(const vector<Block>& old_blocks, const vector<Block>& new_blocks) {
    vector<DiffResult> results;
    vector<bool> new_matched(new_blocks.size(), false);
    vector<bool> old_matched(old_blocks.size(), false);
    for(size_t i = 0;i<old_blocks.size();i++){
        for (size_t j = 0; j < new_blocks.size(); j++){
            if (!new_matched[j] && old_blocks[i].content_hash == new_blocks[j].content_hash){
                DiffResult res;
                res.old_block = old_blocks[i];
                res.new_block = new_blocks[j];
                if (i == j) {
                    res.type = UNCHANGED;
                }
                else {
                    res.type = MOVED;
                    res.line_shift = (new_blocks[j].start_line - old_blocks[i].start_line);
                }
                results.push_back(res);
                old_matched[i] = true;
                new_matched[j] = true;
                break;
            }
        }
    }
    for (size_t i = 0; i < old_blocks.size(); i++){
        if(old_matched[i]){
            continue;
        }
        for (size_t j = 0; j < new_blocks.size(); j++){
            if (!new_matched[j] && old_blocks[i].scope == new_blocks[j].scope) {
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
    for (size_t i = 0; i < old_blocks.size(); i++) {
        if (!old_matched[i]) {
            DiffResult res;
            res.type = DELETED;
            res.old_block = old_blocks[i];
            results.push_back(res);
        }
    }
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

void diffEngine::render_diff(const std::vector<DiffResult>& results, const std::string& fileA,  const std::string& fileB){
    cout << BOLD << CYAN << "\n┌──────────────────────────────────────────────────────────┐\n";
    cout << "│  VOXEL DIFF GRAPH: " << fileA << " ➔ " << fileB << "\n";
    cout << "└──────────────────────────────────────────────────────────┘\n" << RESET;
    int lines_inserted = 0, chars_inserted = 0;
    int lines_deleted = 0, chars_deleted = 0;
    for(auto& res : results){
        if (res.type == UNCHANGED) continue;
        if (res.type == MOVED) {
            std::string direction = res.line_shift > 0 ? "Down +" : "Up -";
            std::cout << "\n" << BOLD << res.new_block.scope << RESET 
                      << YELLOW << "  [Shifted " << direction << std::abs(res.line_shift) << " lines]" << RESET << "\n";
            std::cout << DIM << " ────────────────────────────────────────────────────────\n" << RESET;
        }
        else if (res.type == ADDED) {
            std::cout << "\nScope: " << BOLD << res.new_block.scope << RESET << "  " << GREEN << "[ADDED BLOCK]" << RESET << "\n";
            std::cout << DIM << " ────────────────────────────────────────────────────────\n" << RESET;
            
            int ln = res.new_block.start_line;
            for (const auto& line : res.new_block.lines) {
                // std::setw(4) perfectly aligns the line numbers!
                std::cout << DIM << std::setw(4) << ln++ << " │ " << RESET << GREEN << "+ " << line << RESET << "\n";
                lines_inserted++;
                chars_inserted += line.length();
            }
        }
        
        else if (res.type == DELETED) {
            std::cout << "\nScope: " << BOLD << res.old_block.scope << RESET << "  " << RED << "[DELETED BLOCK]" << RESET << "\n";
            std::cout << DIM << " ────────────────────────────────────────────────────────\n" << RESET;
            
            int ln = res.old_block.start_line;
            for (const auto& line : res.old_block.lines) {
                std::cout << DIM << std::setw(4) << ln++ << " │ " << RESET << RED << "- " << line << RESET << "\n";
                lines_deleted++;
                chars_deleted += line.length();
            }
        }
        
        else if (res.type == MODIFIED) {
            cout << "\nScope: " << BOLD << res.new_block.scope << RESET << "  " << YELLOW << "[MODIFIED STATE]" << RESET << "\n";
            cout << DIM << " ────────────────────────────────────────────────────────\n" << RESET;
    
            render_granular_diff(res.old_block.lines, res.new_block.lines, lines_inserted, chars_inserted, lines_deleted, chars_deleted);
        }
    }

    // --- NEW: THE SUMMARY FOOTER ---
    std::cout << "\n" << DIM << " ────────────────────────────────────────────────────────\n" << RESET;
    std::cout << BOLD << "DIFF SUMMARY:\n" << RESET;
    std::cout << GREEN << "    + " << lines_inserted << " lines inserted (" << chars_inserted << " characters)\n" << RESET;
    std::cout << RED   << "    - " << lines_deleted  << " lines deleted  (" << chars_deleted  << " characters)\n" << RESET;
    std::cout << "____________________________________________________________\n";
    std::cout << "\n";
}
void diffEngine::run_engine_on_file(const std::string& filepath, const std::string& old_content, const std::string& new_content) {
    std::vector<Block> old_blocks = diffEngine::parse_memory(old_content);
    std::vector<Block> new_blocks = diffEngine::parse_memory(new_content);
    
    std::vector<DiffResult> diffs = diffEngine::analyze_diff(old_blocks, new_blocks);
    
    if (!diffs.empty()) {
        diffEngine::render_diff(diffs, filepath + " (Old)", filepath + " (New)");
    }
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












