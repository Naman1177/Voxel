#include "Commands.hpp"
#include "FileSystem.hpp"
#include "Zstd.hpp"
#include "Hashing.hpp"
#include "diff_merge.hpp"
#include "cloud.hpp"
#include "ai.hpp"
#include "../third_party_lib/ai_parser/json.hpp"
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
#include <csignal>
#include <unistd.h>
#include <cstdint>
#include <atomic>
#include <thread>
#include <random>
#include <chrono>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <poll.h>
#include <ifaddrs.h>
#if defined(__APPLE__)
#include <net/if_dl.h>
#elif defined(__linux__)
#include <netpacket/packet.h>
#endif

using namespace std;
namespace fs = std::filesystem;
using json = nlohmann::json;
#define UDP_DISCOVERY_PORT 1178
#define TCP_TRANSFER_PORT 1177
#define BUFFER_SIZE 4096

#define RESET "\033[0m"
#define RED "\033[31m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define CYAN "\033[36m"
#define BOLD "\033[1m"
#define DIM "\033[2m"

static atomic<bool> g_mesh_active(true);
void Cloud::pack_repository(const vector<string> &args)
{
    if (!fs::exists(".voxel"))
    {
        cerr << "\033[1;31mError: Not a Voxel repository.\033[0m\n";
        return;
    }
    string output_filename = fs::current_path().filename().string() + ".vxlpack";
    if (!args.empty())
    {
        output_filename = args[0] + ".vxlpack";
    }
    cout << "\033[1;36m[Voxel Pack] Assembling universal payload...\033[0m\n";
    ofstream out(output_filename, std::ios::binary);
    if (!out)
    {
        cerr << "\033[1;31mError: Could not create payload archive " << output_filename << "\033[0m\n";
        return;
    }
    out.write("VXLPAK01", 8);
    auto write_file_to_pack = [&](const std::string &filepath, const std::string &custom_content = "")
    {
        string content = custom_content.empty() ? FileSystem::read_file_to_string(filepath) : custom_content;
        uint32_t path_len = filepath.size();
        out.write(reinterpret_cast<const char *>(&path_len), sizeof(path_len));
        out.write(filepath.c_str(), path_len);
        uint32_t content_len = content.size();
        out.write(reinterpret_cast<const char *>(&content_len), sizeof(content_len));
        out.write(content.c_str(), content_len);
    };
    int file_count = 0;
    for (const auto &entry : fs::recursive_directory_iterator(".voxel"))
    {
        if (entry.is_regular_file())
        {
            string path = entry.path().string();
            if (path == ".voxel/config" || path.rfind(".voxel/mesh", 0) == 0)
            {
                continue;
            }
            write_file_to_pack(path);
            file_count++;
        }
    }
    string default_config =
        "username=voxel_user\n"
        "email=unknown@voxel.internal\n"
        "provider=none\n"
        "model=none\n"
        "api_key=none\n"
        "is_connected=false\n";
    write_file_to_pack(".voxel/config", default_config);
    file_count++;
    if (fs::exists(".voxelignore"))
    {
        write_file_to_pack(".voxelignore");
        file_count++;
    }
    out.close();
    cout << "\033[1;32m[Voxel Pack] Successfully packed " << file_count << " files into " << output_filename << "\033[0m\n";
}
void Cloud::unpack_repository(const string &filename)
{
    if (filename.empty())
    {
        cerr << "\033[1;31mError: Please provide the payload filename (e.g., voxel unpack test.vxlpack).\033[0m\n";
        return;
    }
    string input_filename = filename;
    if (!fs::exists(input_filename))
    {
        std::cerr << "\033[1;31mError: Payload file '" << input_filename << "' not found.\033[0m\n";
        return;
    }
    if (fs::exists(".voxel"))
    {
        cerr << "\033[1;31mError: A Voxel repository already exists in this directory.\033[0m\n";
        cerr << "Please run unpack inside a fresh, empty directory.\n";
        return;
    }
    ifstream in(input_filename, std::ios::binary);
    if (!in)
    {
        cerr << "\033[1;31mError: Cannot open payload stream.\033[0m\n";
        return;
    }
    char magic[9] = {0};
    in.read(magic, 8);
    if (string(magic) != "VXLPAK01")
    {
        cerr << "\033[1;31mError: Invalid or corrupted Voxel payload. Signature mismatch.\033[0m\n";
        return;
    }
    cout << "\033[1;36m[Voxel Unpack] Extracting timeline and object vault...\033[0m\n";
    int file_count = 0;
    while (in.peek() != EOF)
    {
        uint32_t path_len = 0;
        if (!in.read(reinterpret_cast<char *>(&path_len), sizeof(path_len)))
            break;

        string filepath(path_len, '\0');
        in.read(&filepath[0], path_len);

        uint32_t content_len = 0;
        in.read(reinterpret_cast<char *>(&content_len), sizeof(content_len));

        string content(content_len, '\0');
        in.read(&content[0], content_len);
        fs::path dest_path(filepath);
        if (dest_path.has_parent_path())
        {
            std::filesystem::create_directories(dest_path.parent_path());
        }

        ofstream out_file(filepath, std::ios::binary);
        out_file.write(content.c_str(), content_len);
        out_file.close();

        file_count++;
    }
    in.close();
    cout << "\033[1;32mVault extraction complete (" << file_count << " core files restored).\033[0m\n";
    if (!fs::exists(".voxel/snapshot")) {
        fs::create_directories(".voxel/snapshot");
    }
    Commands::restore_workspace_state("");
    
    
    fs::remove(input_filename);
    Commands::setup_global_identity();
    cout << "\033[1;33mRun 'voxel login' to stamp your identity on this cloned workspace.\033[0m\n";
}
static string generate_mesh_token()
{
    const string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    random_device rd;
    mt19937 generator(rd());
    uniform_int_distribution<> dist(0, chars.size() - 1);
    string token = "VXL";
    for (int i = 0; i < 3; ++i)
    {
        token += chars[dist(generator)];
    }
    return token;
}
static bool recv_exact(int fd, void *buf, size_t len)
{
    size_t got = 0;
    char *p = static_cast<char *>(buf);
    while (got < len)
    {
        ssize_t n = recv(fd, p + got, len - got, 0);
        if (n <= 0)
            return false; // socket error or peer closed mid-message
        got += static_cast<size_t>(n);
    }
    return true;
}
static bool send_exact(int fd, const void *buf, size_t len)
{
    size_t sent = 0;
    const char *p = static_cast<const char *>(buf);
    while (sent < len)
    {
        ssize_t n = send(fd, p + sent, len - sent, 0);
        if (n <= 0)
            return false;
        sent += static_cast<size_t>(n);
    }
    return true;
}
static string get_local_mac_address()
{
    struct ifaddrs *ifap = nullptr;
    if (getifaddrs(&ifap) != 0 || !ifap)
        return "00:00:00:00:00:00";

    char mac_str[18] = "00:00:00:00:00:00";
    for (struct ifaddrs *ifa = ifap; ifa != nullptr; ifa = ifa->ifa_next)
    {
        if (!ifa->ifa_addr)
            continue;

#if defined(__APPLE__)
        if (ifa->ifa_addr->sa_family == AF_LINK)
        {
            auto *sdl = reinterpret_cast<struct sockaddr_dl *>(ifa->ifa_addr);
            if (sdl->sdl_type == 0x06 && sdl->sdl_alen == 6)
            { // Ethernet / Wi-Fi
                unsigned char *ptr = reinterpret_cast<unsigned char *>(LLADDR(sdl));
                snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
                         ptr[0], ptr[1], ptr[2], ptr[3], ptr[4], ptr[5]);
                if (string(ifa->ifa_name) == "en0")
                    break; // Prefer en0 on Mac
            }
        }
#elif defined(__linux__)
        if (ifa->ifa_addr->sa_family == AF_PACKET)
        {
            auto *s = reinterpret_cast<struct sockaddr_ll *>(ifa->ifa_addr);
            if (s->sll_hatype == 1 && s->sll_halen == 6)
            {
                unsigned char *ptr = s->sll_addr;
                snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
                         ptr[0], ptr[1], ptr[2], ptr[3], ptr[4], ptr[5]);
                if (string(ifa->ifa_name) != "lo")
                    break;
            }
        }
#endif
    }
    freeifaddrs(ifap);
    return string(mac_str);
}
static string get_node_hardware_id()
{
    const char *home_dir = getenv("HOME");
    if (!home_dir)
        return "UNKNOWN_HWID";
    string identity_path = string(home_dir) + "/.voxel_identity";
    if (fs::exists(identity_path))
    {
        ifstream in(identity_path);
        string hwid;
        if (in >> hwid)
            return hwid;
    }
    return "PENDING_REGISTRATION";
}
void Cloud::host_mesh(const vector<string> &args){
    if (!fs::exists(".voxel"))
    {
        cerr << "\033[1;31mError: Not a Voxel repository. Run voxel init.\033[0m\n";
        return;
    }
    fs::create_directories(".voxel/mesh");
    if (fs::exists(".voxel/mesh/host.lock"))
    {
        cerr << "\033[1;33mWarning: An active Voxel Host session is already running on this machine.\033[0m\n";
        cerr << "Run \033[1;36mvoxel meshoff\033[0m to terminate the current session before hosting again.\n";
        return;
    }
    ofstream lock(".voxel/mesh/host.lock");
    lock << getpid();
    lock.close();
    string token = generate_mesh_token();
    auto start_time = chrono::steady_clock::now();
    cout << "\033[1;34m=== Starting Voxel P2P Mesh Host ===\033[0m\n";
    cout << "\033[1;32mPairing Token: \033[1;37;44m " << token << " \033[0m \033[1;32m(Valid for 5 minutes)\033[0m\n";
    string pack_arg_name = ".voxel/mesh/mesh_payload";
    vector<string> pack_args = {pack_arg_name};
    Cloud::pack_repository(pack_args);
    string final_pack_file = pack_arg_name + ".vxlpack";
    if (!fs::exists(final_pack_file))
    {
        cerr << "\033[1;31mError: Failed to package repository for mesh transfer.\033[0m\n";
        fs::remove(".voxel/mesh/host.lock");
        return;
    }
    string payload_bytes = FileSystem::read_file_to_string(final_pack_file);
    string payload_sha256 = Hashing::generate_sha256(payload_bytes);
    int udp_fd = socket(AF_INET, SOCK_DGRAM, 0);
    int opt = 1;
    setsockopt(udp_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));

    sockaddr_in udp_addr{};
    udp_addr.sin_family = AF_INET;
    udp_addr.sin_port = htons(UDP_DISCOVERY_PORT);
    udp_addr.sin_addr.s_addr = INADDR_ANY;
    if (::bind(udp_fd, (struct sockaddr *)&udp_addr, sizeof(udp_addr)) < 0)
    {
        cerr << "\033[1;31mError: Could not bind UDP discovery port " << UDP_DISCOVERY_PORT
             << " (already in use?).\033[0m\n";
    }

    int tcp_fd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(tcp_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));

    sockaddr_in tcp_addr{};
    tcp_addr.sin_family = AF_INET;
    tcp_addr.sin_port = htons(TCP_TRANSFER_PORT);
    tcp_addr.sin_addr.s_addr = INADDR_ANY;
    if (::bind(tcp_fd, (struct sockaddr *)&tcp_addr, sizeof(tcp_addr)) < 0)
    {
        cerr << "\033[1;31mError: Could not bind TCP port " << TCP_TRANSFER_PORT
             << " (already in use?).\033[0m\n";
        close(udp_fd);
        close(tcp_fd);
        fs::remove(".voxel/mesh/host.lock");
        return;
    }
    listen(tcp_fd, SOMAXCONN);
    cout << "\033[1;35mMesh listening on TCP Port " << TCP_TRANSFER_PORT << ". Waiting for clients...\033[0m\n";
    json connections_ledger;
    connections_ledger["host_hardware_id"] = get_node_hardware_id();
    connections_ledger["connections"] = json::array();
    g_mesh_active = true;
    bool payload_deleted = false;
    while (g_mesh_active)
    {
        auto now = chrono::steady_clock::now();
        bool token_valid = (chrono::duration_cast<chrono::minutes>(now - start_time).count() < 5);
        if (!token_valid && !payload_deleted && fs::exists(final_pack_file))
        {
            fs::remove(final_pack_file);
            payload_deleted = true;
            cout << "\033[1;33m[Mesh] Token expired. Payload purged from host to save space.\033[0m\n";
        }
        pollfd fds[2];
        fds[0].fd = udp_fd;
        fds[0].events = POLLIN;
        fds[1].fd = tcp_fd;
        fds[1].events = POLLIN;
        int ret = poll(fds, 2, 1000);
        if (ret <= 0)
            continue;
        if (fds[0].revents & POLLIN)
        {
            char udp_buf[256] = {0};
            sockaddr_in client_addr{};
            socklen_t addr_len = sizeof(client_addr);
            recvfrom(udp_fd, udp_buf, sizeof(udp_buf) - 1, 0, (struct sockaddr *)&client_addr, &addr_len);

            if (token_valid && string(udp_buf) == ("DISCOVER:" + token))
            {
                string reply = "OFFER:" + to_string(TCP_TRANSFER_PORT);
                sendto(udp_fd, reply.c_str(), reply.length(), 0, (struct sockaddr *)&client_addr, addr_len);
            }
        }
        if (fds[1].revents & POLLIN)
        {
            sockaddr_in client_addr{};
            socklen_t addr_len = sizeof(client_addr);
            int client_fd = accept(tcp_fd, (struct sockaddr *)&client_addr, &addr_len);

            string client_ip = inet_ntoa(client_addr.sin_addr);
            cout << "\033[1;32m[Mesh] Incoming TCP connection from IP: " << client_ip << "\033[0m\n";

            // Use a micro-poll to detect if the client is speaking first (PULL mode)
            pollfd client_pfd;
            client_pfd.fd = client_fd;
            client_pfd.events = POLLIN;
            int is_pull = poll(&client_pfd, 1, 500); // 500ms timeout

            if (is_pull > 0 && (client_pfd.revents & POLLIN)) {
                
                uint32_t req_len = 0;
                if (recv_exact(client_fd, &req_len, sizeof(req_len)) && req_len > 0) {
                    string req_buf(req_len, '\0');
                    if (recv_exact(client_fd, &req_buf[0], req_len)) {
                        json req = json::parse(req_buf);
                        
                        // 1. Verify Zero-Trust Hardware Ledger
                        bool verified = false;
                        for (const auto& node : connections_ledger["connections"]) {
                            if (node["hardware_id"] == req["hardware_id"]) {
                                verified = true; 
                                break;
                            }
                        }

                        if (!verified) {
                            cerr << "\033[1;31m[Mesh Security] Blocked unauthorized pull request from unregistered hardware.\033[0m\n";
                            close(client_fd);
                            continue;
                        }

                        // 2. Dynamic Packing based on Request Target
                        string pack_target = ".voxel/mesh/live_payload.vxlpack";
                        if (req["action"] == "PULL") {
                            if (req["target"] == "ALL") {
                                vector<string> args = { ".voxel/mesh/live_payload" };
                                Cloud::pack_repository(args); 
                            } else {
                                Cloud::pack_targeted_branch(req["target"], pack_target);
                            }
                        }

                        // 3. Send Payload (Using your robust chunked sender)
                        string targeted_bytes = FileSystem::read_file_to_string(pack_target);
                        string targeted_sha = Hashing::generate_sha256(targeted_bytes);
                        
                        uint32_t sha_len = targeted_sha.length();
                        send_exact(client_fd, &sha_len, sizeof(sha_len));
                        send_exact(client_fd, targeted_sha.c_str(), sha_len);
                        
                        uint64_t f_size = targeted_bytes.size();
                        send_exact(client_fd, &f_size, sizeof(f_size));
                        
                        size_t t_sent = 0;
                        while (t_sent < f_size) {
                            size_t chunk = min<size_t>(BUFFER_SIZE, f_size - t_sent);
                            ssize_t bytes_pushed = send(client_fd, targeted_bytes.data() + t_sent, chunk, 0);
                            if (bytes_pushed <= 0) break;
                            t_sent += bytes_pushed;
                        }
                        
                        fs::remove(pack_target); // Clean up temp file
                    }
                }
            } else {
                // ==========================================================
                // MODE B: INITIAL PAIRING (Host speaks first - YOUR ORIGINAL CODE)
                // ==========================================================
                uint32_t sha_len = payload_sha256.length();
                bool ok = send_exact(client_fd, &sha_len, sizeof(sha_len));
                ok = ok && send_exact(client_fd, payload_sha256.c_str(), sha_len);
                uint64_t file_size = payload_bytes.size();
                ok = ok && send_exact(client_fd, &file_size, sizeof(file_size));

                if (!ok)
                {
                    cerr << "\033[1;31m[Mesh Error] Client disconnected before header exchange completed.\033[0m\n";
                    close(client_fd);
                    continue;
                }

                size_t total_sent = 0;
                while (total_sent < file_size)
                {
                    size_t chunk = min<size_t>(BUFFER_SIZE, file_size - total_sent);
                    ssize_t bytes_pushed = send(client_fd, payload_bytes.data() + total_sent, chunk, 0);

                    if (bytes_pushed <= 0)
                    {
                        cerr << "\033[1;31m[Mesh Error] Client disconnected during transfer.\033[0m\n";
                        break; 
                    }

                    total_sent += bytes_pushed; 
                }
                
                uint32_t json_len = 0;
                if (recv_exact(client_fd, &json_len, sizeof(json_len)) && json_len > 0 && json_len < 65536)
                {
                    string json_buf(json_len, '\0');
                    if (recv_exact(client_fd, &json_buf[0], json_len))
                    {
                        try
                        {
                            json client_info = json::parse(json_buf);
                            client_info["ip_address"] = client_ip;
                            client_info["timestamp"] = Commands::get_current_timestamp();

                            connections_ledger["connections"].push_back(client_info);

                            ofstream out_json(".voxel/mesh/connections.json");
                            out_json << connections_ledger.dump(4);
                            out_json.close();

                            cout << "\033[1;32m[Mesh Registered] Node: "
                                 << client_info["hardware_id"].get<string>().substr(0, 10)
                                 << "... | MAC: " << client_info["mac_address"].get<string>() << "\033[0m\n";

                            json host_reply;
                            host_reply["hardware_id"] = get_node_hardware_id();
                            host_reply["mac_address"] = get_local_mac_address();
                            string host_reply_str = host_reply.dump();
                            uint32_t host_reply_len = host_reply_str.length();
                            send_exact(client_fd, &host_reply_len, sizeof(host_reply_len));
                            send_exact(client_fd, host_reply_str.c_str(), host_reply_len);
                        }
                        catch (...)
                        {
                            cerr << "\033[1;31m[Mesh Error] Failed to parse client identity payload.\033[0m\n";
                        }
                    }
                }
            }
            close(client_fd);
        }
    }
    close(udp_fd);
    close(tcp_fd);
    fs::remove(final_pack_file);
    fs::remove(".voxel/mesh/host.lock");
    cout << "\033[1;33m[Mesh] Host session closed.\033[0m\n";
}
void Cloud::client_mesh(const vector<string> &args)
{
    if (fs::exists(".voxel/mesh/host.lock"))
    {
        cerr << "\033[1;31mError: Cannot run client while hosting.\033[0m\n";
        return;
    }

    string target_token = "";
    string host_ip = "";

    // 1. Parse arguments for an explicit IP override
    for (size_t i = 0; i < args.size(); ++i)
    {
        if (args[i] == "--ip" && i + 1 < args.size())
        {
            host_ip = args[++i];
        }
        else if (target_token.empty())
        {
            target_token = args[i];
        }
    }

    if (host_ip.empty() && target_token.empty())
    {
        cerr << "\033[1;31mError: Please provide a host token or an IP address (--ip).\033[0m\n";
        return;
    }

    
    if (host_ip.empty())
    {
        cout << "\033[1;36mBroadcasting for Voxel Host with Token [" << target_token << "]...\033[0m\n";

        int udp_fd = socket(AF_INET, SOCK_DGRAM, 0);
        int broadcast_opt = 1;
        setsockopt(udp_fd, SOL_SOCKET, SO_BROADCAST, &broadcast_opt, sizeof(broadcast_opt));

        sockaddr_in broadcast_addr{};
        broadcast_addr.sin_family = AF_INET;
        broadcast_addr.sin_port = htons(UDP_DISCOVERY_PORT);
        broadcast_addr.sin_addr.s_addr = INADDR_BROADCAST;

        string ping_msg = "DISCOVER:" + target_token;
        sendto(udp_fd, ping_msg.c_str(), ping_msg.length(), 0, (struct sockaddr *)&broadcast_addr, sizeof(broadcast_addr));

        timeval tv{3, 0};
        setsockopt(udp_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        char resp_buf[256] = {0};
        sockaddr_in host_addr{};
        socklen_t addr_len = sizeof(host_addr);

        int recv_bytes = recvfrom(udp_fd, resp_buf, sizeof(resp_buf) - 1, 0, (struct sockaddr *)&host_addr, &addr_len);
        close(udp_fd);

        if (recv_bytes <= 0)
        {
            cerr << "\033[1;31mError: Host not found. If on a hotspot, try using the --ip flag.\033[0m\n";
            return;
        }
        host_ip = inet_ntoa(host_addr.sin_addr);
    }

    cout << "\033[1;32mHost targeted at IP: " << host_ip << ". Establishing TCP connection...\033[0m\n";

    // 3. Connect via TCP
    int tcp_fd = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in tcp_host_addr{};
    tcp_host_addr.sin_family = AF_INET;

    tcp_host_addr.sin_port = htons(TCP_TRANSFER_PORT);
    inet_pton(AF_INET, host_ip.c_str(), &tcp_host_addr.sin_addr);

    if (connect(tcp_fd, (struct sockaddr *)&tcp_host_addr, sizeof(tcp_host_addr)) < 0)
    {
        cerr << "\033[1;31mError: Failed to open data pipe to host.\033[0m\n";
        close(tcp_fd);
        return;
    }

    uint32_t sha_len = 0;
    if (!recv_exact(tcp_fd, &sha_len, sizeof(sha_len)))
    {
        cerr << "\033[1;31mError: Connection dropped while reading payload header.\033[0m\n";
        close(tcp_fd);
        return;
    }
    string expected_sha(sha_len, '\0');
    if (sha_len == 0 || !recv_exact(tcp_fd, &expected_sha[0], sha_len))
    {
        cerr << "\033[1;31mError: Connection dropped while reading payload hash.\033[0m\n";
        close(tcp_fd);
        return;
    }
    uint64_t file_size = 0;
    if (!recv_exact(tcp_fd, &file_size, sizeof(file_size)))
    {
        cerr << "\033[1;31mError: Connection dropped while reading payload size.\033[0m\n";
        close(tcp_fd);
        return;
    }
    const uint64_t MAX_PAYLOAD_BYTES = 5ULL * 1024 * 1024 * 1024; // 5GB sanity cap
    if (file_size > MAX_PAYLOAD_BYTES)
    {
        cerr << "\033[1;31mError: Reported payload size looks invalid (" << file_size << " bytes).\033[0m\n";
        close(tcp_fd);
        return;
    }
    string payload_data;
    payload_data.resize(file_size);

    if (!recv_exact(tcp_fd, &payload_data[0], file_size))
    {
        cerr << "\033[1;31mError: Failed to move data - connection dropped mid-transfer.\033[0m\n";
        close(tcp_fd);
        return;
    }
    string actual_sha = Hashing::generate_sha256(payload_data);
    if (actual_sha != expected_sha)
    {
        cerr << "\033[1;31mError: SHA-256 hash mismatch! Payload corrupted during Wi-Fi transit Run Again.\033[0m\n";
        close(tcp_fd);
        return;
    }
    cout << "\033[1;32mPayload received and verified. Unpacking workspace...\033[0m\n";

    string download_path = "incoming_mesh.vxlpack";
    ofstream out(download_path, ios::binary);
    out.write(payload_data.data(), payload_data.size());
    out.close();
    Cloud::unpack_repository(download_path);
    // Ensure mesh folder exists on the client side
    fs::create_directories(".voxel/mesh");
    json client_identity;
    client_identity["hardware_id"] = get_node_hardware_id();
    client_identity["mac_address"] = get_local_mac_address();
    string json_str = client_identity.dump();
    uint32_t json_len = json_str.length();

    send_exact(tcp_fd, &json_len, sizeof(json_len));
    send_exact(tcp_fd, json_str.c_str(), json_len);

    // Receive and persist host identity (ip, mac, hardware_id)
    uint32_t host_json_len = 0;
    if (recv_exact(tcp_fd, &host_json_len, sizeof(host_json_len)) && host_json_len > 0 && host_json_len < 65536)
    {
        string host_json_buf(host_json_len, '\0');
        if (recv_exact(tcp_fd, &host_json_buf[0], host_json_len))
        {
            try
            {
                json host_info = json::parse(host_json_buf);
                host_info["ip_address"] = host_ip;
                host_info["timestamp"] = Commands::get_current_timestamp();

                ofstream out_json(".voxel/mesh/host_info.json");
                out_json << host_info.dump(4);
                out_json.close();

                cout << "\033[1;36m[Mesh] Host identity recorded in .voxel/mesh/host_info.json\033[0m\n";
            }
            catch (...)
            {
                cerr << "\033[1;31m[Mesh Error] Failed to save host identity.\033[0m\n";
            }
        }
    }

    close(tcp_fd);
    cout << "\033[1;32mSuccessfully synced and registered node with Mesh Host!\033[0m\n";
}
void Cloud::mesh_off(const vector<string> &args)
{
    if (fs::exists(".voxel/mesh/host.lock")) {
        ifstream lock(".voxel/mesh/host.lock");
        int host_pid; 
        
        if (lock >> host_pid) {
            // Send a POSIX termination signal to the running Host process
            kill(host_pid, SIGINT); 
        }
        lock.close();
        
        // Clean up the lock and payload files
        fs::remove(".voxel/mesh/host.lock");
        if(fs::exists(".voxel/mesh/mesh_payload.vxlpack")) {
            fs::remove(".voxel/mesh/mesh_payload.vxlpack");
        }
        
        cout << "\033[1;33mVoxel Mesh session shut down. Machine is offline.\033[0m\n";
    } else {
        cout << "\033[1;33mNo active Mesh session found.\033[0m\n";
    }
}
void Cloud::pack_targeted_branch(const string &branch_name, const string &output_filename){
    string ref_path = ".voxel/refs/heads/" + branch_name;
    if (!fs::exists(ref_path)) {
        cerr << "\033[1;31mError: Branch '" << branch_name << "' not found on host.\033[0m\n";
        return;
    }
    cout << "\033[1;36m[Voxel Pack] Tracing graph lineage for '" << branch_name << "'...\033[0m\n";
    string latest_commit = FileSystem::read_file_to_string(ref_path);
    latest_commit.erase(std::remove_if(latest_commit.begin(), latest_commit.end(), ::isspace), latest_commit.end());
    set<string> required_objects;
    string current_commit = latest_commit;
    while (!current_commit.empty() && current_commit != string(64, '0')){
        required_objects.insert(current_commit);
        string obj_path = ".voxel/objects/" + current_commit;
        if (!fs::exists(obj_path)) break;
        string commit_data = FileSystem::read_file_to_string(obj_path);
        istringstream stream(commit_data);
        string line;
        string parent_hash = "";
        while (getline(stream, line)) {
            if (line.rfind("tree - ", 0) == 0) {
                string tree_hash = line.substr(7);
                tree_hash.erase(std::remove_if(tree_hash.begin(), tree_hash.end(), ::isspace), tree_hash.end());
                required_objects.insert(tree_hash);
                string tree_path = ".voxel/objects/" + tree_hash;
                if (fs::exists(tree_path)) {
                    istringstream tree_stream(FileSystem::read_file_to_string(tree_path));
                    string t_line;
                    while (getline(tree_stream, t_line)) {
                        stringstream ss(t_line);
                        string filepath, file_hash;
                        ss >> filepath >> file_hash;
                        if (!file_hash.empty()) required_objects.insert(file_hash);
                    }
                }
            } else if (line.rfind("parent - ", 0) == 0) {
                parent_hash = line.substr(9);
                parent_hash.erase(std::remove_if(parent_hash.begin(), parent_hash.end(), ::isspace), parent_hash.end());
            }
            
        }
        current_commit = parent_hash;
       
    }
    ofstream out(output_filename, std::ios::binary);
    out.write("VXLPAK01", 8);
    auto write_file_to_pack = [&](const std::string &filepath, const std::string &disk_path) {
        string content = FileSystem::read_file_to_string(disk_path);
        uint32_t path_len = filepath.size();
        out.write(reinterpret_cast<const char *>(&path_len), sizeof(path_len));
        out.write(filepath.c_str(), path_len);
        uint32_t content_len = content.size();
        out.write(reinterpret_cast<const char *>(&content_len), sizeof(content_len));
        out.write(content.c_str(), content_len);
    };
    for (const string& obj : required_objects) {
        write_file_to_pack(".voxel/objects/" + obj, ".voxel/objects/" + obj);
    }
    write_file_to_pack(".voxel/refs/heads/" + branch_name, ".voxel/refs/heads/" + branch_name);
    out.close();
}
void Cloud::pull_repository(const vector<string> &args) {
    string mesh_info_path = ".voxel/mesh/host_info.json";
    if (!fs::exists(mesh_info_path)) {
        cerr << "\033[1;31mError: No host connection found. Run 'voxel client <token>' first to pair.\033[0m\n";
        return;
    }

    string target_branch = "ALL";
    if (!args.empty()) {
        target_branch = args[0];
    }

    string live_mac = get_local_mac_address();
    string live_hwid = Commands::get_hardware_uuid() + "_voxel_p2p_node";
    string secured_hwid = Hashing::generate_sha256(live_hwid);
    
    json host_info;
    ifstream info_file(mesh_info_path);
    info_file >> host_info;
    info_file.close();
    
    string host_ip = host_info["ip_address"];
    
    int tcp_fd = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in tcp_host_addr{};
    tcp_host_addr.sin_family = AF_INET;
    tcp_host_addr.sin_port = htons(TCP_TRANSFER_PORT);
    inet_pton(AF_INET, host_ip.c_str(), &tcp_host_addr.sin_addr);

    if (connect(tcp_fd, (struct sockaddr *)&tcp_host_addr, sizeof(tcp_host_addr)) < 0) {
        cerr << "\033[1;31mError: Host offline or out of range. Connection refused.\033[0m\n";
        close(tcp_fd);
        return;
    }

    json request;
    request["action"] = "PULL";
    request["target"] = target_branch;
    request["hardware_id"] = secured_hwid;
    request["mac_address"] = live_mac;
    
    string req_str = request.dump();
    uint32_t req_len = req_str.length();
    send_exact(tcp_fd, &req_len, sizeof(req_len));
    send_exact(tcp_fd, req_str.c_str(), req_len);
    
    uint32_t sha_len = 0;
    if (!recv_exact(tcp_fd, &sha_len, sizeof(sha_len))) {
        cerr << "\033[1;31mError: Host dropped connection (Identity verification likely failed).\033[0m\n";
        close(tcp_fd);
        return;
    }
    
    string expected_sha(sha_len, '\0');
    recv_exact(tcp_fd, &expected_sha[0], sha_len);

    uint64_t file_size = 0;
    recv_exact(tcp_fd, &file_size, sizeof(file_size));

    string payload_data;
    payload_data.resize(file_size);
    if (!recv_exact(tcp_fd, &payload_data[0], file_size)) {
        cerr << "\033[1;31mError: Connection dropped mid-transfer.\033[0m\n";
        close(tcp_fd);
        return;
    }
    close(tcp_fd);
    
    if (Hashing::generate_sha256(payload_data) != expected_sha) {
        cerr << "\033[1;31mError: Payload corrupted during transit.\033[0m\n";
        return;
    }
    
    cout << "\033[1;32mSecure Payload received. Extracting to isolation tier...\033[0m\n";
    
    // 1. Save the downloaded pack
    string pull_temp = "incoming_pull.vxlpack";
    ofstream out(pull_temp, ios::binary);
    out.write(payload_data.data(), payload_data.size());
    out.close();
    
    // 2. Create the isolation sandbox
    string safe_sandbox = "incoming_mesh_sandbox";
    fs::create_directories(safe_sandbox);

    // 3. SHIELDED EXTRACTION: Unpack safely into the sandbox
    ifstream in(pull_temp, std::ios::binary);
    char magic[9] = {0};
    in.read(magic, 8);
    
    if (string(magic) == "VXLPAK01") {
        while (in.peek() != EOF) {
            uint32_t path_len = 0;
            if (!in.read(reinterpret_cast<char *>(&path_len), sizeof(path_len))) break;

            string filepath(path_len, '\0');
            in.read(&filepath[0], path_len);

            uint32_t content_len = 0;
            in.read(reinterpret_cast<char *>(&content_len), sizeof(content_len));

            string content(content_len, '\0');
            in.read(&content[0], content_len);

            // SHIELD: Ignore the host's configuration file so the client's identity remains untouched
            if (filepath == ".voxel/config" || filepath == ".voxelignore") {
                continue;
            }

            // Route paths from ".voxel/..." to our sandbox folder
            string sandbox_path = filepath;
            if (filepath.find(".voxel/") == 0) {
                sandbox_path = safe_sandbox + "/" + filepath.substr(7);
            } else {
                sandbox_path = safe_sandbox + "/" + filepath;
            }

            fs::path dest_path(sandbox_path);
            if (dest_path.has_parent_path()) {
                fs::create_directories(dest_path.parent_path());
            }

            ofstream out_file(sandbox_path, std::ios::binary);
            out_file.write(content.c_str(), content_len);
            out_file.close();
        }
    } else {
        cerr << "\033[1;31mError: Invalid payload signature.\033[0m\n";
        in.close();
        fs::remove(pull_temp);
        fs::remove_all(safe_sandbox);
        return;
    }
    in.close();

    // 4. Move the newly pulled, zstd-compressed objects into the LIVE database
    string sandbox_objects = safe_sandbox + "/objects";
    if (fs::exists(sandbox_objects)) {
        if (!fs::exists(".voxel/objects")) fs::create_directories(".voxel/objects");
        for (const auto& entry : fs::directory_iterator(sandbox_objects)) {
            fs::copy_file(entry.path(), ".voxel/objects/" + entry.path().filename().string(), fs::copy_options::overwrite_existing);
        }
    }

    // 5. Safely copy the pulled branch pointer to the live refs folder as "_mesh"
    string active_branch = Commands::get_current_branch_name();
    string incoming_branch_name = (target_branch == "ALL") ? active_branch : target_branch;
    string incoming_mesh_ref = incoming_branch_name + "_mesh";
    
    string sandbox_ref = safe_sandbox + "/refs/heads/" + incoming_branch_name;
    if (fs::exists(sandbox_ref)) {
        if (!fs::exists(".voxel/refs/heads")) fs::create_directories(".voxel/refs/heads");
        fs::copy_file(sandbox_ref, ".voxel/refs/heads/" + incoming_mesh_ref, fs::copy_options::overwrite_existing);
    } else {
        cerr << "\033[1;31mError: Target branch '" << incoming_branch_name << "' not found in the pulled payload.\033[0m\n";
        fs::remove(pull_temp);
        fs::remove_all(safe_sandbox);
        return;
    }

    // 6. Execute the Merge Engine
    cout << "\033[1;33mRouting updated code into sandbox conflict engine...\033[0m\n";
    merge::execute(active_branch, incoming_mesh_ref);

    // 7. Clean up the sandbox
    fs::remove(pull_temp);
    fs::remove_all(safe_sandbox);
}
