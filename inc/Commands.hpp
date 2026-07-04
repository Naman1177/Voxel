#ifndef COMMANDS_HPP
#define COMMANDS_HPP
#include <string>
class Commands {
public:
    static void track_all_files();
    static void commit_changes(const std::string& message);
    static void create_branch(const std::string& branch_name);
    static void switch_branch(const std::string& target_branch);
};




#endif // COMMANDS_HPP