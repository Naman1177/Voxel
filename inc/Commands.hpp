#ifndef COMMANDS_HPP
#define COMMANDS_HPP
#include <string>
#include <vector>
#include <algorithm>
#include <set>         
#include <map>         
#include <utility>
using namespace std;
class Commands {

public:
    struct CommitNode {
        std::string hash;
        std::string parent;
        std::string author;
        std::string timestamp;
        std::string message;
        std::set<std::string> branches; 
        std::vector<std::string> children;
    };
    static string get_current_timestamp();
    static void track_all_files();
    static void commit_changes(const std::string& message);
    static void create_branch(const std::string& branch_name);
    static void switch_branch(const std::string& target_branch);
    static std::string get_current_branch_name();
    static void display_basic_log();
    static void display_graph_log();
    static void export_repository_pdf();
    static void restore_workspace_state(const std::string& target_expr);
    static void create_snapshot();
    static void restore_snapshot();
    static void clear_snapshot_silent();
    static bool should_ignore_extension(const std::string& ext);
    static void display_diff(const std::string& fileA, const std::string& fileB);
    static void diverge(const std::vector<std::string>& args);
    static std::pair<std::string, std::map<std::string, CommitNode>> build_complete_repo_graph();
    static void bin_target(const std::vector<std::string>& args);
    static void revive_target(const std::vector<std::string>& args);
    static void setup_global_identity();
    static void who();
    static void configure_model();
    static string get_hardware_uuid();
private:
    
    static void checkout_files_from_tree(const std::string& tree_hash);
    static bool is_snapshot_empty();
    static string get_user_name();
    static void bin_commit(const std::string& hash);
    static void bin_branch(const std::string& branch_name);
    static std::string trim_whitespace(const std::string& str);
    static void revive_commit(const std::string& hash);
    static void revive_branch(const std::string& branch_name);

};




#endif // COMMANDS_HPP