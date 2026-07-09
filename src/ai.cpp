#include "ai.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <filesystem>
#include <cstdlib>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
using namespace std;
namespace fs = std::filesystem;
string config_path = ".voxel/config";

bool ai::save_config(const ai::VoxelConfig &config)
{
    if (!fs::exists(".voxel"))
    {
        cout << "\033[1;31mError: .voxel directory does not exist. Please run 'voxel init' first.\033[0m\n";
        return false;
    }
    ofstream file(config_path);
    if (!file.is_open())
    {
        cout << "\033[31m[Config Error] Cannot open registry file for synchronization.\033[0m\n";
        return false;
    }
    file << "username=" << config.username << "\n";
    file << "email=" << config.email << "\n";
    file << "provider=" << config.provider << "\n";
    file << "model=" << config.model << "\n";
    file << "api_key=" << config.api_key << "\n";
    file << "is_connected=" << config.is_connected << "\n";

    return true;
}
bool ai::load_config(ai::VoxelConfig &config)
{
    if (!fs::exists(".voxel"))
    {
        cout << "\033[1;31mError: .voxel directory does not exist. Please run 'voxel init' first.\033[0m\n";
        return false;
    }
    ifstream file(config_path);
    if (!file.is_open())
    {
        cout << "\033[31m[Config Error] Cannot open registry file for synchronization.\033[0m\n";
        return false;
    }
    string line;
    while (getline(file, line))
    {
        istringstream line_stream(line);
        string key, value;
        if (std::getline(line_stream, key, '=') && std::getline(line_stream, value))
        {
            if (key == "username")
                config.username = value;
            else if (key == "email")
                config.email = value;
            else if (key == "provider")
                config.provider = value;
            else if (key == "model")
                config.model = value;
            else if (key == "api_key")
                config.api_key = value;
            else if (key == "is_connected")
                config.is_connected = value;
        }
    }
    return true;
}

bool ai::create_default_config()
{
    VoxelConfig default_config;
    default_config.username = "voxel_user";
    default_config.email = "unknown@voxel.internal";
    default_config.provider = "none";
    default_config.model = "none";
    default_config.api_key = "none";
    default_config.is_connected = "false";

    return save_config(default_config);
}

bool ai::is_ssh_session()
{
    return (std::getenv("SSH_CLIENT") != nullptr ||
            std::getenv("SSH_TTY") != nullptr ||
            std::getenv("SSH_CONNECTION") != nullptr);
}

void ai::open_browser(const std::string &url)
{
#if __APPLE__
    string command = "open \"" + url + "\"";
#else
    string command = "xdg-open \"" + url + "\"";
#endif
    system(command.c_str());
}

string extract_query_param(const string &request, const string &param_name)
{
    size_t target_pos = request.find(param_name + "=");
    if (target_pos == string::npos)
        return "none";
    target_pos += param_name.length() + 1;
    size_t end_pos = request.find_first_of(" &\r\n", target_pos);
    if (end_pos == string::npos)
        return request.substr(target_pos);
    string raw_val = request.substr(target_pos, end_pos - target_pos);

    // 1. Fix %40 -> @
    size_t repo_at = raw_val.find("%40");
    if (repo_at != string::npos)
        raw_val.replace(repo_at, 3, "@");

    // 2. Fix %20 -> standard space character (Loop handles multiple spaces)
    size_t repo_space;
    while ((repo_space = raw_val.find("%20")) != string::npos)
    {
        raw_val.replace(repo_space, 3, " ");
    }

    return raw_val;
}

bool ai::run_local_server(ai::VoxelConfig &config)
{
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
        return false;

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(1177);

    if (::bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        close(server_fd);
        return false;
    }

    if (listen(server_fd, 5) < 0)
    {
        close(server_fd);
        return false;
    }

    cout << "\033[1;34m[Web Server] Authentication gateway online at http://localhost:1177/login\033[0m\n";

    string http_headers = "HTTP/1.1 200 OK\r\n"
                          "Content-Type: text/html\r\n"
                          "Connection: close\r\n\r\n";

    // Fault-tolerant HTML interface block
    string html_body = R"raw(<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>Voxel AI Authentication Hub</title>
    <style>
        :root { --bg: #1e1e2e; --surface: #252538; --text: #cdd6f4; --subtext: #a6adc8; --green: #a6e3a1; --blue: #89b4fa; --red: #f38ba8; }
        body { font-family: sans-serif; background-color: var(--bg); color: var(--text); display: flex; justify-content: center; align-items: center; height: 100vh; margin: 0; }
        .auth-card { background-color: var(--surface); padding: 40px; border-radius: 12px; text-align: center; max-width: 400px; width: 100%; box-shadow: 0 8px 24px rgba(0,0,0,0.3); }
        h1 { color: var(--green); margin-bottom: 5px; }
        .btn { width: 100%; padding: 12px; border: none; border-radius: 6px; font-size: 1rem; font-weight: bold; cursor: pointer; margin-bottom: 12px; transition: opacity 0.2s; }
        .btn:hover { opacity: 0.9; }
        .btn-github { background-color: #24292f; color: white; }
        .btn-google { background-color: white; color: #1f2328; }
        .status { margin-top: 15px; color: #cba6f7; font-size: 0.9rem; word-break: break-word; line-height: 1.4; }
    </style>
</head>
<body>
<div class="auth-card">
    <h1>Voxel AI Subsystem</h1>
    <p style="color: var(--subtext); margin-bottom: 25px;">Secure Single-Click Cloud Identity Workspace Link</p>
    <button class="btn btn-github" id="github-btn">Sign in with GitHub</button>
    <button class="btn btn-google" id="google-btn">Sign in with Google</button>
    <div class="status" id="status-msg">Initializing cloud gateways...</div>
</div>

<script type="module">
    import { initializeApp } from "https://www.gstatic.com/firebasejs/10.12.0/firebase-app.js";
    import { getAuth, signInWithPopup, GoogleAuthProvider, GithubAuthProvider } from "https://www.gstatic.com/firebasejs/10.12.0/firebase-auth.js";

    const status = document.getElementById("status-msg");

    // ⚠️ CRITICAL STEP: Paste your real web app keys from Firebase Settings here!
    const firebaseConfig = {
        apiKey: "AIzaSyCU16yogkahY2GyskLfTOB4xq1AoTT4tQY",
        authDomain: "voxelai-a1482.firebaseapp.com",
        projectId: "voxelai-a1482",
        storageBucket: "voxelai-a1482.firebasestorage.app",
        messagingSenderId: "811147042814",
        appId: "1:811147042814:web:763f59e0d0088bd4d5eaaf",
        measurementId: "G-PR3VLLBJSQ"
    };

    let auth = null;

    // Direct redirection pipeline to the C++ POSIX background socket
    async function getPayload(user) {
        try {
            status.style.color = "#cba6f7";
            status.innerText = "Generating encrypted cloud gateway tokens...";
            const firebaseToken = await user.getIdToken();
            const username = user.displayName || user.email.split('@')[0];
            
            window.location.href = `http://localhost:1177/?key=${encodeURIComponent(firebaseToken)}&username=${encodeURIComponent(username)}&email=${encodeURIComponent(user.email)}`;
        } catch (err) {
            status.style.color = "var(--red)";
            status.innerText = "Token Extraction Error: " + err.message;
        }
    }

    // Protection Wrapper to catch configuration exceptions cleanly
    try {
        if (firebaseConfig.apiKey.includes("YOUR_REAL")) {
            throw new Error("Please paste your true Firebase API Key and App ID into your C++ source code before compiling!");
        }
        const app = initializeApp(firebaseConfig);
        auth = getAuth(app);
        status.innerText = "Awaiting secure session authorization...";
    } catch (err) {
        status.style.color = "var(--red)";
        status.innerText = "Firebase Setup Failed: " + err.message;
    }

    // Only attach click events if the Firebase core setup succeeded
    if (auth) {
        document.getElementById("github-btn").addEventListener("click", async () => {
            status.style.color = "var(--blue)";
            status.innerText = "Contacting GitHub Authentication Portal...";
            try {
                const result = await signInWithPopup(auth, new GithubAuthProvider());
                getPayload(result.user);
            } catch(e) { 
                status.style.color = "var(--red)";
                status.innerText = "GitHub Error: " + e.message; 
            }
        });

        document.getElementById("google-btn").addEventListener("click", async () => {
            status.style.color = "var(--blue)";
            status.innerText = "Contacting Google Cloud Gate...";
            try {
                const result = await signInWithPopup(auth, new GoogleAuthProvider());
                getPayload(result.user);
            } catch(e) { 
                status.style.color = "var(--red)";
                status.innerText = "Google Error: " + e.message; 
            }
        });
    }
</script>
</body>
</html>)raw";
    // Combine them into a compliant stream packet
    string full_response = http_headers + html_body;

    while (true)
    {
        int client_socket = accept(server_fd, nullptr, nullptr);
        if (client_socket < 0)
            continue;

        char buffer[2048] = {0};
        read(client_socket, buffer, 2047);
        string request(buffer);

        if (request.find("GET /login") != string::npos)
        {
            // Write out the valid, properly padded buffer packet
            write(client_socket, full_response.c_str(), full_response.length());
            close(client_socket);
        }
        else if (request.find("key=") != string::npos)
        {
            config.api_key = extract_query_param(request, "key");
            config.username = extract_query_param(request, "username");
            config.email = extract_query_param(request, "email");
            config.provider = "gemini";
            config.model = "gemini-2.5-flash";
            config.is_connected = "true";

            string success_html =
                "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n"
                "<html><body style='font-family:sans-serif; text-align:center; padding-top:60px; background:#1e1e2e; color:#cdd6f4;'>"
                "<h1 style='color:#a6e3a1;'>✔ Voxel Authorized Successfully!</h1>"
                "<p>Welcome back, <b>" +
                config.username + "</b>. You can close this tab safely.</p>"
                                  "</body></html>";

            write(client_socket, success_html.c_str(), success_html.length());
            close(client_socket);
            close(server_fd);
            return true;
        }
        else
        {
            close(client_socket);
        }
    }
    return false;
}

bool ai::init_ai()
{
    cout << "\033[1;36m Upgrading Voxel Environment with AI Services...\033[0m\n";

    ai::VoxelConfig config;
    load_config(config);

    if (is_ssh_session())
    {
        cout << "\033[1;33m[SSH Mode Detected] Headless environment active.\033[0m\n";
        cout << "Enter username: ";
        cin >> config.username;
        cout << "Enter email:    ";
        cin >> config.email;
        cout << "Enter API Key:  ";
        cin >> config.api_key;
        config.provider = "gemini";
        config.model = "gemini-2.5-flash";
        config.is_connected = "true";
    }
    else
    {
        // Trigger browser directly to the C++ server's own endpoint!
        open_browser("http://localhost:1177/login");

        if (!run_local_server(config))
        {
            cout << "\033[1;31mAuthentication loop timed out.\033[0m\n";
            return false;
        }
    }

    if (save_config(config))
    {
        cout << "\n\033[1;32m✔ Voxel AI successfully configured!\033[0m\n";
        cout << "Active Profile: " << config.username << " <" << config.email << ">\n";
        return true;
    }
    return false;
}