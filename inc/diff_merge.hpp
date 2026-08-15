#ifndef DIFF_MERGE_HPP
#define DIFF_MERGE_HPP
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <algorithm>
using namespace std;
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
    static bool is_scope_header(const std::string& raw_line, std::string& out_scope_name);
    static void run_engine_on_file(const std::string& filepath, const std::string& old_content, const std::string& new_content);

public:
    static std::vector<Block> parse_file(const std::string& filepath);
    static std::vector<Block> parse_memory(const std::string& raw_content);
    static std::vector<DiffResult> analyze_diff(const std::vector<Block>& old_blocks,const std::vector<Block>& new_blocks);
    static void render_diff(const std::vector<DiffResult>& results, const std::string& fileA,  const std::string& fileB);
    static void route_diff(const std::vector<std::string>& args);
    static void ai_diff(const std::vector<std::string> &args);
    static void report_media_file_diff(const std::string &file,const std::string &old_content,const std::string &new_content, bool old_existed, bool new_existed);
    static void diff_pdf(const std::vector<std::string> &args);
    
};

class merge{
public:
    static void execute(const std::string &current_branch, const std::string &incoming_branch, bool use_ai = false);
    static string get_branch_commit(const std::string& branch_name);
    static bool process_file_merge(const std::string &filepath, const std::string &target_branch,const std::string &source_branch, const std::string &base_commit, bool use_ai = false);
    static bool resolve_conflict_ai(const std::string &filepath, const std::string &base_content,const std::string &ours_content, const std::string &theirs_content,const std::string &target_branch, const std::string &source_branch);
private:
    static bool setup_sandbox();
    static void cleanup_sandbox();
    static void apply_sandbox_to_workspace();
    static string find_lowest_common_ancestor(const std::string& branchA, const std::string& branchB);
    
    static string get_file_content_from_commit(const std::string& commit_hash, const std::string& filepath);
    
    static void resolve_conflict_interactive(const std::string &filepath,const std::vector<std::string> &base_lines,const std::vector<std::string> &ours_lines,const std::vector<std::string> &theirs_lines,const std::string &target_branch,const std::string &source_branch);
    static std::string format_branch_name(const std::string& raw_name);
};



#endif // "DIFF_MERGE_HPP"
