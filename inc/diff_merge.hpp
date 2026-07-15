#ifndef DIFF_MERGE_HPP
#define DIFF_MERGE_HPP
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <algorithm>
struct Block {
    int start_line;
    int end_line;
    std::string scope;
    std::string content_hash;
    std::vector<std::string> lines;
};
enum DiffType { 
    UNCHANGED, 
    ADDED, 
    DELETED, 
    MODIFIED, 
    MOVED 
};
struct DiffResult {
    DiffType type;
    Block old_block;
    Block new_block;
    int line_shift; 
};


class diffEngine{
private:
    static std::string generate_block_hash(const std::vector<std::string>& lines);
    static bool is_scope_header(const std::string& raw_line);

public:
    static std::vector<Block> parse_file(const std::string& filepath);
    static std::vector<Block> parse_memory(const std::string& raw_content);
    static std::vector<DiffResult> analyze_diff(const std::vector<Block>& old_blocks,const std::vector<Block>& new_blocks);
    static void render_diff(const std::vector<DiffResult>& results, const std::string& fileA,  const std::string& fileB);

};




#endif // "DIFF_MERGE_HPP"