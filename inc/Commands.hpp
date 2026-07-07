#ifndef COMMANDS_HPP
#define COMMANDS_HPP
#include <string>
#include <vector>
#include <algorithm>
#include <set>         
#include <map>         
#include <utility>
class Commands {

public:
    struct CommitNode {
        std::string hash;
        std::string parent;
        std::string author;
        std::string timestamp;
        std::string message;
        std::set<std::string> branches; // Branch tracking flags anchored here
        std::vector<std::string> children; // Forward links mapped out by engine
    };
    static void track_all_files();
    static void commit_changes(const std::string& message);
    static void create_branch(const std::string& branch_name);
    static void switch_branch(const std::string& target_branch);
    static std::string get_current_branch_name();
    static void display_basic_log();
    static void display_graph_log();
    static void export_repository_pdf();
    static void restore_workspace_state(const std::string& target_expr);


private:
    static std::pair<std::string, std::map<std::string, CommitNode>> build_complete_repo_graph();
    static void checkout_files_from_tree(const std::string& tree_hash);

};




#endif // COMMANDS_HPP