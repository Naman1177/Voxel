#include "Commands.hpp"
#include "FileSystem.hpp"
#include "Zstd.hpp"
#include "Hashing.hpp"

#include <iostream>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <vector>
#include <algorithm>
#include <string>
#include <map>
#include <set>
#include <queue>
#include <sys/ioctl.h>
#include <unistd.h>
using namespace std;
namespace fs = std::filesystem;

string get_current_timestamp()
{
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}
string Commands::get_current_branch_name()
{
    if (!fs::exists(".voxel/HEAD"))
        return "main";

    std::ifstream ifs(".voxel/HEAD");
    string line;
    std::getline(ifs, line);
    ifs.close();

    // line will look like: "ref: refs/heads/exp@feat"
    string prefix = "ref: refs/heads/";
    if (line.find(prefix) == 0)
    {
        return line.substr(prefix.length()); // Returns "exp@feat"
    }
    return "main";
}
void Commands::track_all_files()
{
    string index_path = ".voxel/index";
    string objects_dir = ".voxel/objects";
    //fs::create_directories(objects_dir);
    vector<string> files = FileSystem::list_workspace_files();
    ofstream index_file(index_path, ios::trunc);
    if (!index_file.is_open())
    {
        cerr << "\033[31mError: Could not open or create the database index map.\033[0m\n";
        return;
    }
    cout << "\033[36m--- Staging Workspace State to Index ---\033[0m\n";
    int tracked_count = 0;
    for (const auto &file : files)
    {
        string content = FileSystem::read_file_to_string(file);
        string file_hash = Hashing::generate_sha256(content);
        index_file << file << " " << file_hash << "\n";
        string object_file_path = objects_dir + "/" + file_hash;
        
        if (!fs::exists(object_file_path))
        {
            string ext = std::filesystem::path(file).extension().string();
            transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext == ".mp4"  || ext == ".blend" || ext == ".png"  || ext == ".jpg"  || 
                ext == ".jpeg" || ext == ".gif"   || ext == ".tiff" || ext == ".bmp"  || 
                ext == ".wav"  || ext == ".mp3"   || ext == ".flac" || ext == ".ogg"  || 
                ext == ".mov"  || ext == ".avi"   || ext == ".mkv"  || ext == ".3ds"  || 
                ext == ".fbx"  || ext == ".obj"   || ext == ".stl"  || ext == ".dae"  || 
                ext == ".zip"  || ext == ".rar"   || ext == ".7z"   || ext == ".aac"  || 
                ext == ".flv"  || ext == ".m4a"   || ext == ".m4v"  || ext == ".webm" || 
                ext == ".heic" || ext == ".ico"   || ext == ".webp" || ext == ".glb"  || 
                ext == ".gltf" || ext == ".psd") 
            {
                // Media Assets: Direct binary transfer to vault
                fs::copy_file(file, object_file_path, fs::copy_options::overwrite_existing);
            }
            else 
            {
                // Text/Code Assets: Compress right away with Zstd!
                Zstd::compress_file(file, object_file_path);
            }
        }
        cout << "  Tracked: " << file << " -> [" << file_hash.substr(0, 8) << "...]\n";
        tracked_count++;
    }
    index_file.close();
    cout << "\033[32m--- Staging Complete: " << tracked_count << " files tracked to index ---\033[0m\n";
}
void Commands::commit_changes(const string &message)
{
    if (!fs::exists(".voxel/index"))
    {
        cerr << "\033[31mError: No tracked files found. Please run 'voxel track' before committing.\033[0m\n";
        return;
    }
    string index_content = FileSystem::read_file_to_string(".voxel/index");
    if (index_content.empty())
    {
        cout << "\033[31mError: Tracking area is empty. Nothing to commit.\033[0m\n";
        return;
    }
    string tree_hash = Hashing::generate_sha256(index_content);
    string tree_object_path = ".voxel/objects/" + tree_hash;
    if (!fs::exists(tree_object_path))
    {
        ofstream tree_file(tree_object_path);
        tree_file << index_content;
        tree_file.close();
    }

    string parent_hash = "0000000000000000000000000000000000000000000000000000000000000000"; // default zero string
    string current_branch = get_current_branch_name();
    string current_branch_path = ".voxel/refs/heads/" + current_branch;
    if (fs::exists(current_branch_path))
    {
        string saved_parent = FileSystem::read_file_to_string(current_branch_path);
        if (!saved_parent.empty())
        {
            parent_hash = saved_parent;
        }
    }
    string author = "Naman Malhotra";
    string timestamp = get_current_timestamp();
    stringstream commit_stream;
    commit_stream << "tree - " << tree_hash << "\n";
    commit_stream << "parent - " << parent_hash << "\n";
    commit_stream << "author - " << author << "\n";
    commit_stream << "timestamp - " << timestamp << "\n\n";
    commit_stream << "Message - " << message << "\n";
    string commit_content = commit_stream.str();
    string commit_hash = Hashing::generate_sha256(commit_content);
    string commit_object_path = ".voxel/objects/" + commit_hash;
    ofstream commit_file(commit_object_path);
    commit_file << commit_content;
    commit_file.close();
    ofstream current_branch_file(current_branch_path, ios::trunc);
    current_branch_file << commit_hash;
    current_branch_file.close();
    cout << "\033[1;32mCommit successful!\033[0m\n";
    cout << "  Commit ID: [" << commit_hash.substr(0, 8) << "...]\n";
    cout << "  Message:   " << message << "\n";
}
void Commands::create_branch(const string &branch_name)
{
    if (branch_name.empty() || branch_name.find(' ') != std::string::npos)
    {
        std::cout << "\033[31mError: Branch name cannot be empty or contain spaces.\033[0m\n";
        return;
    }
    string current_parent_branch = get_current_branch_name();
    string current_parent_path = ".voxel/refs/heads/" + current_parent_branch;
    string active_hash = FileSystem::read_file_to_string(current_parent_path);
    if (active_hash.empty())
    {
        std::cout << "\033[31mError: Parent timeline has no commit history yet.\033[0m\n";
        return;
    }
    string full_new_branch;
    if (current_parent_branch == "main")
    {
        full_new_branch = branch_name;
    }
    else
    {
        full_new_branch = current_parent_branch + "@" + branch_name;
    }
    string target_file_path = ".voxel/refs/heads/" + full_new_branch;
    if (fs::exists(target_file_path))
    {
        std::cout << "\033[31mError: Timeline cluster path already allocated.\033[0m\n";
        return;
    }
    std::ofstream ofs(target_file_path);
    ofs << active_hash;
    ofs.close();
    string printing_wala = full_new_branch;
    replace(printing_wala.begin(), printing_wala.end(), '@', '/');

    std::cout << "\033[1;32mBranch '" << printing_wala << "' successfully created at commit [" << active_hash.substr(0, 8) << "...]!\033[0m\n";

} // create branch
void Commands::switch_branch(const string &target_branch)
{

    if (target_branch.empty() || target_branch.find(' ') != std::string::npos)
    {
        std::cout << "\033[31mError: Branch name cannot be empty or contain spaces.\033[0m\n";
        return;
    }
    string internal_name = target_branch;
    replace(internal_name.begin(), internal_name.end(), '/', '@');
    string branch_path = ".voxel/refs/heads/" + internal_name;
    if (!fs::exists(branch_path) || fs::is_directory(branch_path))
    {
        std::cout << "\033[31mError: Branch '" << target_branch << "' does not exist.\033[0m\n";
        return;
    }
    ofstream head_file(".voxel/HEAD", ios::trunc);
    head_file << "ref: refs/heads/" << internal_name;
    head_file.close();
    std::cout << "\033[1;32mSwitched to branch '" << target_branch << "' successfully!\033[0m\n";
}
std::pair<std::string, std::map<std::string, Commands::CommitNode>> Commands::build_complete_repo_graph()
{
    std::map<std::string, CommitNode> repo_map;
    std::string genesis_hash = "";

    std::string objects_dir = ".voxel/objects";
    std::string refs_dir = ".voxel/refs/heads";

    if (!fs::exists(objects_dir) || !fs::exists(refs_dir))
    {
        return {genesis_hash, repo_map};
    }

    std::map<std::string, std::set<std::string>> branch_markers;
    std::string active_head = get_current_branch_name();

    // Clean active_head string just in case
    active_head.erase(std::remove_if(active_head.begin(), active_head.end(), ::isspace), active_head.end());

    for (const auto &entry : fs::directory_iterator(refs_dir))
    {
        if (entry.is_regular_file())
        {
            std::string br_name = entry.path().filename().string();
            std::string br_hash = FileSystem::read_file_to_string(entry.path().string());

            // Absolute sanitation: Remove ALL newlines, returns, and spaces
            br_hash.erase(std::remove_if(br_hash.begin(), br_hash.end(), ::isspace), br_hash.end());
            br_name.erase(std::remove_if(br_name.begin(), br_name.end(), ::isspace), br_name.end());

            std::string display_label = br_name;
            std::replace(display_label.begin(), display_label.end(), '@', '/');

            if (br_name == active_head)
            {
                branch_markers[br_hash].insert("HEAD -> " + display_label);
            }
            else
            {
                branch_markers[br_hash].insert(display_label);
            }
        }
    }

    for (const auto &entry : fs::directory_iterator(objects_dir))
    {
        if (!entry.is_regular_file())
            continue;

        std::ifstream ifs(entry.path().string());
        std::string line;
        bool valid_commit = false;
        CommitNode node;
        node.hash = entry.path().filename().string();

        // Clean object filename hash
        node.hash.erase(std::remove_if(node.hash.begin(), node.hash.end(), ::isspace), node.hash.end());

        while (std::getline(ifs, line))
        {
            if (line.rfind("parent - ", 0) == 0)
            {
                node.parent = line.substr(9);
                valid_commit = true;
            }
            else if (line.rfind("author - ", 0) == 0)
            {
                node.author = line.substr(9);
            }
            else if (line.rfind("timestamp - ", 0) == 0)
            {
                node.timestamp = line.substr(12);
            }
            else if (line.rfind("Message - ", 0) == 0)
            {
                node.message = line.substr(10);
            }
        }
        ifs.close();

        if (valid_commit)
        {
            node.parent.erase(std::remove_if(node.parent.begin(), node.parent.end(), ::isspace), node.parent.end());

            // Check for direct match OR substring prefix match (handles short vs long hash variations)
            for (const auto &[ref_hash, labels] : branch_markers)
            {
                if (node.hash == ref_hash ||
                    (node.hash.substr(0, 7) == ref_hash.substr(0, 7) && !ref_hash.empty()))
                {
                    node.branches.insert(labels.begin(), labels.end());
                }
            }

            repo_map[node.hash] = node;
        }
    }
    for (auto &[hash, node] : repo_map)
    {
        if (node.parent == std::string(64, '0') || node.parent.empty() || node.parent.find_first_not_of('0') == std::string::npos)
        {
            genesis_hash = hash;
        }
        else if (repo_map.count(node.parent))
        {
            repo_map[node.parent].children.push_back(hash);
        }
    }

    return {genesis_hash, repo_map};
}
void Commands::display_basic_log()
{
    auto [genesis, repo_map] = build_complete_repo_graph();
    if (genesis.empty())
    {
        std::cout << "\033[33mNo commits found on the database cluster.\033[0m\n";
        return;
    }
    std::map<std::string, std::vector<CommitNode>> chronological_map;
    for (const auto &[hash, node] : repo_map)
    {
        chronological_map[node.timestamp].push_back(node);
    }

    std::cout << "\033[1;36m-----------------VOXEL LOG TIMELINE-----------------\033[0m\n";

    for (const auto &[timestamp, nodes] : chronological_map)
    {
        for (const auto &node : nodes)
        {

            std::cout << "\033[1;32m=== Location: ";
            if (!node.branches.empty())
            {
                bool first = true;
                for (const auto &br : node.branches)
                {
                    if (!first)
                        std::cout << ", ";
                    std::cout << br;
                    first = false;
                }
            }
            else
            {
                std::cout << "Starting Point";
            }
            std::cout << " ===\033[0m\n";

            std::cout << "\033[1;33mCommit ID: \033[0m[" << node.hash << "]\n";
            std::cout << "\033[1;30mAuthor:    \033[0m" << node.author << "\n";
            std::cout << "\033[1;30mDate:      \033[0m" << node.timestamp << "\n";
            std::cout << "\033[1;37mMessage:   \033[0m" << node.message << "\n";
            std::cout << "\033[1;30m───────────────────────────────────────────────────────\033[0m\n\n";
        }
    }
}
void Commands::display_graph_log()
{
    auto [genesis, repo_map] = build_complete_repo_graph();
    if (genesis.empty())
    {
        std::cout << "\033[33mNo commits available to construct architectural typography.\033[0m\n";
        return;
    }

    std::cout << "\033[1;36m------------VOXEL REPOSITORY TIMELINE GRAPH------------\033[0m\n";

    // 1. Group and sort all commits chronologically by timestamp
    std::map<std::string, std::vector<CommitNode>> chronological_map;
    for (const auto &[hash, node] : repo_map)
    {
        chronological_map[node.timestamp].push_back(node);
    }

    // 2. Dynamic horizontal lane assignments for branches
    std::vector<std::string> branch_lanes;
    branch_lanes.push_back("main"); // Spine is always lane 0

    auto get_lane_index = [&](const std::string &b_name) -> size_t
    {
        auto it = std::find(branch_lanes.begin(), branch_lanes.end(), b_name);
        if (it != branch_lanes.end())
            return std::distance(branch_lanes.begin(), it);
        branch_lanes.push_back(b_name);
        return branch_lanes.size() - 1;
    };

    bool is_genesis = true;

    // 3. Render Loop
    for (const auto &[timestamp, nodes] : chronological_map)
    {
        for (const auto &node : nodes)
        {

            // Extract the true active target branch name
            std::string active_branch = "main";
            std::string location_tags = "";

            if (!node.branches.empty())
            {
                for (const auto &br : node.branches)
                {
                    active_branch = (br.find("HEAD -> ") != std::string::npos) ? br.substr(8) : br;
                }

                // Beautifully format the branch tags string wrapper
                location_tags = "  \033[1;32m[";
                for (const auto &br : node.branches)
                {
                    location_tags += br + ", ";
                }
                location_tags.pop_back();
                location_tags.pop_back();
                location_tags += "]\033[0m";
            }

            size_t target_lane = get_lane_index(active_branch);

            // A. Draw Genesis Root anchor block
            if (is_genesis)
            {
                std::cout << "  \033[1;34m○ [Genesis Root]\033[0m\n";
                std::cout << "  │\n";
                is_genesis = false;
            }

            // B. Draw horizontal alignment rails out to the active target branch node
            std::cout << "  ";
            for (size_t i = 0; i < branch_lanes.size(); ++i)
            {
                if (i == target_lane)
                {
                    // Node dot identifier coloring logic
                    if (active_branch == "main")
                        std::cout << "\033[1;33m●\033[0m"; // Main Spine (Gold)
                    else
                        std::cout << "\033[1;35m●\033[0m"; // Sub-Branch (Magenta)
                }
                else if (i < target_lane && target_lane > 0 && i == 0)
                {
                    std::cout << "\033[1;30m│\033[0m───⚙───"; // Structural branch fork out of main
                }
                else if (i < target_lane)
                {
                    std::cout << "───────";
                }
            }

            // C. Output the inline metadata fields
            std::cout << "  \033[1;37m" << node.message << "\033[0m"
                      << " \033[1;30m(" << node.hash.substr(0, 7) << ")\033[0m"
                      << location_tags << "\n";

            // D. Draw continuous vertical track lines safely to pad rows down
            std::cout << "  ";
            for (size_t i = 0; i < branch_lanes.size(); ++i)
            {
                if (i == 0)
                {
                    std::cout << "\033[1;30m│\033[0m"; // Keep the main spine line solid
                }
                else if (i <= target_lane)
                {
                    std::cout << "       \033[1;30m│\033[0m"; // Track lane extension line
                }
            }
            std::cout << "\n";
        }
    }

    std::cout << "  ▼\n";
    std::cout << "\033[1;36m=======================================================\033[0m\n\n";
}
void Commands::export_repository_pdf()
{
    auto [genesis, repo_map] = build_complete_repo_graph();
    if (genesis.empty())
    {
        cout << "\033[33mNo workspace history discovered to compile into PDF.\033[0m\n";
        return;
    }
    string time = get_current_timestamp();
    replace(time.begin(), time.end(), ' ', '_');
    replace(time.begin(), time.end(), ':', '-');
    string filename = "voxel_repo_graph_" + time + ".pdf";
    ofstream pdf(filename, ios::trunc | ios::binary);
    if (!pdf.is_open())
    {
        cerr << "\033[31mError: Failed to instantiate system PDF document writer.\033[0m\n";
        return;
    }
    cout << "\033[36mCompiling architectural history matrix into PDF...\033[0m\n";
    pdf << "%PDF-1.4\n";
    pdf << "1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n";
    pdf << "2 0 obj\n<< /Type /Pages /Kids [3 0 R] /Count 1 >>\nendobj\n";
    pdf << "3 0 obj\n<< /Type /Page /Parent 2 0 R /MediaBox [0 0 595 842] /Contents 4 0 R /Resources << /Font << /F1 << /Type /Font /Subtype /Type1 /BaseFont /Courier >> >> >> >>\nendobj\n";
    stringstream text_stream;
    text_stream << "BT\n/F1 11 Tf\n14 TL\n50 800 Td\n"; // Establish Courier typography scale bounds
    text_stream << "(=======================================================) Tj T*\n";
    text_stream << "(             VOXEL WORKSPACE EXPORT REPORT             ) Tj T*\n";
    text_stream << "(=======================================================) Tj T*\n";
    text_stream << "(Generated: " << time << ") Tj T*\n\n";

    text_stream << "(--- SECTION 1: Full Commit History ---) Tj T*\n\n";
    std::map<std::string, std::vector<CommitNode>> chronological_map;
    for (const auto &[hash, node] : repo_map)
    {
        chronological_map[node.timestamp].push_back(node);
    }
    for (const auto &[timestamp, nodes] : chronological_map)
    {
        for (const auto &node : nodes)
        {
            std::string branch_line = "Location: ";
            if (!node.branches.empty())
            {
                for (const auto &br : node.branches)
                {
                    branch_line += br + ", ";
                }
                branch_line.pop_back();
                branch_line.pop_back();
            }
            else
            {
                branch_line += "Internal/Detached Node";
            }

            text_stream << "=== (" << branch_line << ") === Tj T*\n";
            text_stream << "  (Commit ID: [" << node.hash << "]) Tj T*\n";
            text_stream << "  (Author:    " << node.author << ") Tj T*\n";
            text_stream << "  (Date:      " << node.timestamp << ") Tj T*\n";
            text_stream << "  (Message:   " << node.message << ") Tj T*\n";
            text_stream << "  (-----------------------------------------------------) Tj T*\n\n";
        }
    }
    text_stream << "\n(--- SECTION 2: GRAPH TIMELINE BRANCH MAP ---) Tj T*\n\n";
    text_stream << "  (Starting Point) Tj T*\n";
    text_stream << "  | Tj T*\n";
    vector<string> branch_lanes;
    branch_lanes.push_back("main");
    auto get_lane_idx = [&](const std::string &name) -> size_t
    {
        auto it = std::find(branch_lanes.begin(), branch_lanes.end(), name);
        if (it != branch_lanes.end())
            return std::distance(branch_lanes.begin(), it);
        branch_lanes.push_back(name);
        return branch_lanes.size() - 1;
    };
    for (const auto &[timestamp, nodes] : chronological_map)
    {
        for (const auto &node : nodes)
        {
            std::string active_br = "main";
            std::string tags = "";
            if (!node.branches.empty())
            {
                for (const auto &br : node.branches)
                {
                    active_br = (br.find("HEAD -> ") != std::string::npos) ? br.substr(8) : br;
                }
                tags = " [";
                for (const auto &br : node.branches)
                {
                    tags += br + " ";
                }
                tags.pop_back();
                tags += "]";
            }

            size_t target_lane = get_lane_idx(active_br);
            std::string graph_row = "  ";

            for (size_t i = 0; i < branch_lanes.size(); ++i)
            {
                if (i == target_lane)
                {
                    graph_row += "*"; // Node coordinate indicator
                }
                else if (i < target_lane && target_lane > 0 && i == 0)
                {
                    graph_row += "|---o---";
                }
                else if (i < target_lane)
                {
                    // Fill space safely
                    graph_row += "--------";
                }
            }

            graph_row += " " + node.message + " (" + node.hash.substr(0, 7) + ")" + tags;

            // Clean syntax escape parameters to protect PDF parser layers
            std::replace(graph_row.begin(), graph_row.end(), '(', '[');
            std::replace(graph_row.begin(), graph_row.end(), ')', ']');

            text_stream << "(" << graph_row << ") Tj T*\n";

            // Draw connecting down rails
            std::string rail_row = "  ";
            for (size_t i = 0; i < branch_lanes.size(); ++i)
            {
                if (i == 0)
                    rail_row += "|";
                else if (i <= target_lane)
                    rail_row += "       |";
            }
            text_stream << "(" << rail_row << ") Tj T*\n";
        }
    }

    text_stream << "ET\n";
    std::string output_buffer = text_stream.str();

    // 3. Document Object Size Matrix Assembly
    pdf << "4 0 obj\n<< /Length " << output_buffer.length() << " >>\nstream\n"
        << output_buffer << "endstream\nendobj\n";
    pdf << "xref\n0 5\n0000000000 65535 f \n0000000018 00000 n \n0000000073 00000 n \n0000000133 00000 n \n0000000305 00000 n \n";
    pdf << "trailer\n<< /Size 5 /Root 1 0 R >>\nstartxref\n"
        << (320 + output_buffer.length()) << "\n%%EOF\n";

    pdf.close();
    std::cout << "\033[32m✔ Success: Voxel snapshot summary exported completely to: " << filename << "\033[0m\n\n";
}
void Commands::checkout_files_from_tree(const std::string &tree_hash)
{
    std::string tree_path = ".voxel/objects/" + tree_hash;
    if (!fs::exists(tree_path))
    {
        std::cerr << "\033[31mError: Missing tree structure " << tree_hash.substr(0, 8) << " inside vault.\033[0m\n";
        return;
    }

    std::ifstream tree_file(tree_path);
    std::string line;

    // 1. Loop through the tree and reconstruct the physical files
    while (std::getline(tree_file, line))
    {
        if (line.empty())
            continue;
        std::stringstream ss(line);
        std::string filepath, file_hash;
        ss >> filepath >> file_hash;

        std::string object_path = ".voxel/objects/" + file_hash;
        if (fs::exists(object_path))
        {
            // Extract the parent path safety string checkpoint
            fs::path parent_dir = fs::path(filepath).parent_path();

            // 🔥 CRITICAL PROTECTION: Only create directories if the file is inside subfolders!
            if (!parent_dir.empty() && parent_dir != "." && parent_dir != "")
            {
                fs::create_directories(parent_dir);
            }

            // Overwrite target file layout cleanly with database content stream
            std::string ext = fs::path(filepath).extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

            if (ext == ".mp4"  || ext == ".blend" || ext == ".png"  || ext == ".jpg"  || 
                ext == ".jpeg" || ext == ".gif"   || ext == ".tiff" || ext == ".bmp"  || 
                ext == ".wav"  || ext == ".mp3"   || ext == ".flac" || ext == ".ogg"  || 
                ext == ".mov"  || ext == ".avi"   || ext == ".mkv"  || ext == ".3ds"  || 
                ext == ".fbx"  || ext == ".obj"   || ext == ".stl"  || ext == ".dae"  || 
                ext == ".zip"  || ext == ".rar"   || ext == ".7z"   || ext == ".aac"  || 
                ext == ".flv"  || ext == ".m4a"   || ext == ".m4v"  || ext == ".webm" || 
                ext == ".heic" || ext == ".ico"   || ext == ".webp" || ext == ".glb"  || 
                ext == ".gltf" || ext == ".psd") 
            {
                // Media Assets: Direct uncompressed byte stream recovery from object vault
                fs::copy_file(object_path, filepath, fs::copy_options::overwrite_existing);
            }
            else 
            {
                // Text/Code Assets: Decode Zstd compression layout natively back into workspace
                Zstd::decompress_file(object_path, filepath);
            }
        }
    }

    // Clear and close the file stream reader before resetting position bounds
    tree_file.close();

    // 2. 🔥 SYNCHRONIZE STAGING INDEX AREA (Keep this at the bottom!)
    std::ofstream index_sync(".voxel/index", std::ios::trunc);
    std::ifstream tree_sync(tree_path);
    if (tree_sync.is_open() && index_sync.is_open())
    {
        index_sync << tree_sync.rdbuf();
    }
}
void Commands::restore_workspace_state(const std::string &target_expr)
{
    auto [genesis, repo_map] = build_complete_repo_graph();
    if (genesis.empty())
    {
        std::cout << "\033[33mNo historical records to target time-travel states into.\033[0m\n";
        return;
    }

    std::string working_expr = target_expr;
    // Sanitize input string spacing structures
    working_expr.erase(std::remove_if(working_expr.begin(), working_expr.end(), ::isspace), working_expr.end());

    std::string identifier = working_expr;
    int step_back = 0;

    // Check for negative step modifications like branch-3 or hash-3
    size_t dash_pos = working_expr.find('-');
    if (dash_pos != std::string::npos)
    {
        identifier = working_expr.substr(0, dash_pos);
        try
        {
            step_back = std::stoi(working_expr.substr(dash_pos + 1));
        }
        catch (...)
        {
            std::cerr << "\033[31mError: Invalid timeline fallback metric signature formatting.\033[0m\n";
            return;
        }
    }

    std::string starting_hash = "";

    // 1. Resolve Expression Identifier Root Coordinates
    if (identifier.empty())
    {
        // ✅ FIXED: Now safely defaults to the active branch's latest commit hash instead of its parent
        std::string current_br = get_current_branch_name();
        std::string br_file = ".voxel/refs/heads/" + current_br;
        if (fs::exists(br_file))
        {
            std::string current_hash = FileSystem::read_file_to_string(br_file);
            current_hash.erase(std::remove_if(current_hash.begin(), current_hash.end(), ::isspace), current_hash.end());
            if (repo_map.count(current_hash))
            {
                starting_hash = current_hash; 
            }
        }
    }
    else if (identifier == "head" || identifier == "HEAD")
    {
        // Point straight to the permanent Genesis foundation node configuration anchor block
        starting_hash = genesis;
    }
    else if (repo_map.count(identifier))
    {
        // Identifier is a valid, raw 64-character hash value signature match entry
        starting_hash = identifier;
    }
    // Inside Commands::restore_workspace_state() right where you match branches:
    else
    {
        // 🔥 STEP 1: Handle forward slashes if users input structural paths
        std::string transformed_br = identifier;
        std::replace(transformed_br.begin(), transformed_br.end(), '/', '@');

        // 🔥 STEP 2: Handle reverse slashes if users input nested paths backwards (exp2/exp -> exp2@exp)
        // If the flat file doesn't exist directly, check if inversion matching helps out
        std::string branch_ref_path = ".voxel/refs/heads/" + transformed_br;

        if (!fs::exists(branch_ref_path) && identifier.find('/') != std::string::npos)
        {
            size_t slash_pos = identifier.find('/');
            std::string part1 = identifier.substr(0, slash_pos);
            std::string part2 = identifier.substr(slash_pos + 1);
            // Re-stitch matching your sharded naming schema format rule: branch2@branch1
            transformed_br = part2 + "@" + part1;
            branch_ref_path = ".voxel/refs/heads/" + transformed_br;
        }

        if (fs::exists(branch_ref_path))
        {
            starting_hash = FileSystem::read_file_to_string(branch_ref_path);
            starting_hash.erase(std::remove_if(starting_hash.begin(), starting_hash.end(), ::isspace), starting_hash.end());
        }
        else
        {
            // Backup check: standard hash prefix match
            for (const auto &[hash, node] : repo_map)
            {
                if (hash.substr(0, identifier.length()) == identifier)
                {
                    starting_hash = hash;
                    break;
                }
            }
        }
    }

    if (starting_hash.empty() || !repo_map.count(starting_hash))
    {
        std::cerr << "\033[31mError: Specified timeline position target expression could not be located.\033[0m\n";
        return;
    }

    // 2. Traversal Loop: Step backwards down the chain through parent nodes
    std::string current_target = starting_hash;
    for (int i = 0; i < step_back; ++i)
    {
        std::string parent_ptr = repo_map[current_target].parent;
        // Cease execution processing if parent field terminates to Genesis boundary markers
        if (parent_ptr == std::string(64, '0') || parent_ptr.empty() || parent_ptr.find_first_not_of('0') == std::string::npos)
        {
            current_target = genesis; // Cap layout fallback constraint boundaries smoothly
            break;
        }
        current_target = parent_ptr;
    }

    // 3. Finalize: Extract the root tree map and run checkout file reconstruction
    CommitNode target_node = repo_map[current_target];

    // Parse tree string signature out of raw object serialization maps
    std::string target_tree_hash = "";
    std::string obj_file_content = FileSystem::read_file_to_string(".voxel/objects/" + current_target);
    std::stringstream file_ss(obj_file_content);
    std::string line;
    while (std::getline(file_ss, line))
    {
        if (line.rfind("tree - ", 0) == 0)
        {
            target_tree_hash = line.substr(7);
            target_tree_hash.erase(std::remove_if(target_tree_hash.begin(), target_tree_hash.end(), ::isspace), target_tree_hash.end());
            break;
        }
    }

    if (target_tree_hash.empty())
    {
        std::cerr << "\033[31mError: Structural file blueprint index corrupted on target node.\033[0m\n";
        return;
    }

    std::cout << "\033[36mTime traveling workspace backward to target node checkpoint...\033[0m\n";
    checkout_files_from_tree(target_tree_hash);

    // Update active reference tracks safely to prevent split layout detached state conflicts

    std::cout << "\033[32m✔ Success! Working tree restored to state node: [" << current_target.substr(0, 8) << "]\033[0m\n";
    std::cout << "  Message context reference: \"" << target_node.message << "\"\n";
}
bool Commands::is_snapshot_empty()
{
    string snapshot_dir = ".voxel/snapshot";
    if (!fs::exists(snapshot_dir))
    {
        cout << "\033[36mError: Snapshots directory not found. Run 'voxel init' to initialize the repository.\033[0m\n";
    }
    return fs::directory_iterator(snapshot_dir) == fs::directory_iterator();
}
void Commands::restore_snapshot()
{
    std::string snap_root = ".voxel/snapshot";
    std::string manifest_path = snap_root + "/snapshot_index";

    if (!fs::exists(manifest_path) || is_snapshot_empty())
    {
        std::cout << "\033[33mNo active snapshots found to roll back into.\033[0m\n";
        return;
    }

    std::ifstream manifest(manifest_path);
    std::string line;

    std::cout << "\033[36mRolling back to snapshot checkpoint. Restoring raw states...\033[0m\n";

    while (std::getline(manifest, line))
    {
        if (line.empty())
            continue;
        std::stringstream ss(line);
        std::string original_filepath, file_hash;
        ss >> original_filepath >> file_hash;

        fs::path orig_path(original_filepath);
        fs::path snap_file_path = fs::path(snap_root) / orig_path.parent_path() / file_hash;

        if (fs::exists(snap_file_path))
        {
            fs::path orig_parent = orig_path.parent_path();
            if (!orig_parent.empty() && orig_parent != "." && orig_parent != "")
            {
                fs::create_directories(orig_parent);
            }

            std::ifstream src(snap_file_path, std::ios::binary);
            std::ofstream dest(original_filepath, std::ios::binary | std::ios::trunc);
            dest << src.rdbuf();
        }
    }
    manifest.close();

    // WIPE SCRATCHPAD CLEAN NATIVELY
    fs::remove_all(snap_root);
    fs::create_directories(snap_root);

    std::cout << "\033[32m✔ Workspace restored. Snapshot scratchpad wiped clean.\033[0m\n";
}
void Commands::create_snapshot()
{
    std::string snap_root = ".voxel/snapshot";

    // 🔒 SINGLE-SLOT CONSTRAINT GUARD
    if (!is_snapshot_empty())
    {
        std::cerr << "\033[31mError: An active snapshot already exists.\033[0m\n";
        std::cerr << "You must run '\033[33mvoxel snapback\033[0m' first to clear the last state data.\n";
        return;
    }

    if (!fs::exists(snap_root))
    {
        std::cerr << "\033[31mError: Snapshots directory not found. Run 'voxel init' to initialize the repository.\033[0m\n";
        return;
    }
    std::ofstream manifest(snap_root + "/snapshot_index", std::ios::trunc);
    if (!manifest.is_open())
    {
        std::cerr << "Error: Failed to initialize snapshot index ledger.\n";
        return;
    }

    // 🔄 Securely fetch pre-filtered files (Zero risk of catching 'voxel' or '.git')
    std::vector<std::string> workspace_files = FileSystem::list_workspace_files();

    std::cout << "\033[36mGenerating high-speed workspace shadow snapshot...\033[0m\n";

    for (const auto &relative_path : workspace_files)
    {
        fs::path file_path(relative_path);

        if (should_ignore_extension(file_path.extension().string()))
        {
            continue;
        }

        std::string file_hash = Hashing::generate_sha256(relative_path);

        fs::path parent_snap_dir = fs::path(snap_root) / file_path.parent_path();
        if (!parent_snap_dir.empty() && parent_snap_dir != snap_root)
        {
            fs::create_directories(parent_snap_dir);
        }

        fs::path snap_target_file = parent_snap_dir / file_hash;

        std::ifstream src(relative_path, std::ios::binary);
        std::ofstream dest(snap_target_file, std::ios::binary | std::ios::trunc);
        if (src.is_open() && dest.is_open())
        {
            dest << src.rdbuf();
            manifest << relative_path << " " << file_hash << "\n";
        }
    }

    manifest.close();
    std::cout << "\033[32m✔ Snapshot successfully saved. Slot locked. You may try anything with workspace.\033[0m\n";
}
void Commands::clear_snapshot_silent()
{
    std::string snap_root = ".voxel/snapshot";

    if (!fs::exists(snap_root) || is_snapshot_empty())
    {
        return;
    }
    fs::remove_all(snap_root);
    fs::create_directories(snap_root);
}

bool Commands::should_ignore_extension(const string &ext)
{
    string clean_ext = ext;
    transform(clean_ext.begin(), clean_ext.end(), clean_ext.begin(), ::tolower);
    static const std::vector<std::string> ignored_formats = {
        ".mp4", ".mp3", ".blender", ".jpeg", ".jpg", ".png", ".gif", ".mov", ".wav"};
    return std::find(ignored_formats.begin(), ignored_formats.end(), clean_ext) != ignored_formats.end();
}

