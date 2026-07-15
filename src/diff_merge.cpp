#include "diff_merge.hpp"
#include "Hashing.hpp"
#include "FileSystem.hpp"
#include <vector>
#include <iomanip>
#include <sstream>
#include <iostream>


using namespace std;
#define RESET       "\033[0m"
#define RED         "\033[31m"
#define GREEN       "\033[32m"
#define YELLOW      "\033[33m"
#define CYAN        "\033[36m"
#define BOLD        "\033[1m"
#define DIM         "\033[2m"


// --- Private Helper: Pure Logic Hashing (Ignores comments & whitespace) ---
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
            std::cout << "\nScope: " << BOLD << res.new_block.scope << RESET << "  " << YELLOW << "[MODIFIED STATE]" << RESET << "\n";
            std::cout << DIM << " ────────────────────────────────────────────────────────\n" << RESET;
            
            // Render Deletions (Old Code)
            int old_ln = res.old_block.start_line;
            for (const auto& line : res.old_block.lines) {
                if (std::find(res.new_block.lines.begin(), res.new_block.lines.end(), line) == res.new_block.lines.end()) {
                    std::cout << DIM << std::setw(4) << old_ln << " │ " << RESET << RED << "- " << line << RESET << "\n";
                    lines_deleted++;
                    chars_deleted += line.length();
                }
                old_ln++;
            }
            
            // Render Additions & Context (New Code)
            int new_ln = res.new_block.start_line;
            for (const auto& line : res.new_block.lines) {
                if (std::find(res.old_block.lines.begin(), res.old_block.lines.end(), line) == res.old_block.lines.end()) {
                    std::cout << DIM << std::setw(4) << new_ln << " │ " << RESET << GREEN << "+ " << line << RESET << "\n";
                    lines_inserted++;
                    chars_inserted += line.length();
                } else {
                    std::cout << DIM << std::setw(4) << new_ln << " │ " << RESET << "  " << line << "\n"; // Context
                }
                new_ln++;
            }
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

