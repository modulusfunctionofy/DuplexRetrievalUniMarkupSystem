// ============================================================
// Windows-compatible version
// Changes from Linux original:
//   1. Replaced POSIX headers with <winsock2.h>, <ws2tcpip.h>, <windows.h>
//   2. Added WSAStartup() / WSACleanup()
//   3. Replaced close() with closesocket()
//   4. Replaced read() on sockets with recv()
//   5. Changed <mysql/mysql.h> to <mysql.h> (Windows MySQL Connector path)
//   6. Replaced std::filesystem with _access() / CreateDirectories helper
//      (avoids linker issues with older MSVC / MinGW builds)
//   7. rand() seed via srand(GetTickCount()) instead of relying on implicit seed
//   8. localtime() -> localtime_s() (MSVC safe variant, with fallback)
//   9. Added #pragma comment(lib,...) for automatic Winsock linking
// Everything else (logic, routes, MySQL queries) is unchanged.
// ============================================================

// --- Windows networking headers (replaces sys/socket.h, netinet/in.h, arpa/inet.h, unistd.h) ---
// Must be defined BEFORE any include — MinGW needs >= 0x0601 to expose
// IPV6_V6ONLY, inet_ntop, and other Vista+ symbols.
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0601   // Windows 7+
#undef WINVER
#define WINVER       0x0601

#include <winsock2.h>         // Must come before <windows.h>
#include <ws2tcpip.h>         // inet_ntop, IPV6_V6ONLY, IPv6 structures
#include <windows.h>          // General Windows API (GetTickCount, CreateDirectory)

#pragma comment(lib, "Ws2_32.lib")  // Auto-link Winsock (MSVC / compatible MinGW)

// --- Standard C headers (available on Windows too) ---
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <time.h>
#include <ctime>    // std::time_t, strftime — needed for MinGW

// --- Standard C++ headers (unchanged) ---
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <map>
#include <random>
#include <string>

// --- MySQL header (Windows Connector/C installs as <mysql.h>, not <mysql/mysql.h>) ---
#include <mysql.h>

// ============================================================
// Compatibility shims
// ============================================================

// closesocket() already exists in Winsock; no alias needed.
// recv() already exists in Winsock; we just use it instead of read().

// Windows does not have std::filesystem reliably in older toolchains,
// so we use a small helper + _access() from <io.h>.
#include <io.h>       // _access()
#include <direct.h>   // _mkdir()
#include <sys/stat.h> // _stat

static bool file_exists(const std::string& path) {
    return _access(path.c_str(), 0) == 0;
}

// Recursive directory creation (equivalent to std::filesystem::create_directories)
static void create_directories(const std::string& path) {
    std::string current;
    for (char c : path) {
        if (c == '/' || c == '\\') {
            if (!current.empty()) _mkdir(current.c_str());
        }
        current += c;
    }
    if (!current.empty()) _mkdir(current.c_str());
}

// ============================================================
// Session
// ============================================================

struct Session {
    int user_id;
    std::string username;
};

std::map<std::string, Session> sessions; // session_id -> Session

std::string generate_session_id() {
    static const char alphanum[] =
        "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    std::string tmp_s;
    tmp_s.reserve(32);
    for (int i = 0; i < 32; ++i)
        tmp_s += alphanum[rand() % (sizeof(alphanum) - 1)];
    return tmp_s;
}

// ============================================================
// Helpers (unchanged from original)
// ============================================================

std::string extract_json_field(const std::string& json, const std::string& key) {
    std::string search_key = "\"" + key + "\":";
    auto p = json.find(search_key);
    if (p == std::string::npos) return "";
    p += search_key.length();

    while (p < json.length() && isspace(json[p])) p++;

    if (p < json.length() && json[p] == '\"') {
        p++;
        std::string val;
        bool escaped = false;
        while (p < json.length()) {
            if (escaped) {
                val += json[p];
                escaped = false;
            } else if (json[p] == '\\') {
                escaped = true;
            } else if (json[p] == '\"') {
                break;
            } else {
                val += json[p];
            }
            p++;
        }
        return val;
    }
    return "";
}

std::string url_decode(const std::string& str) {
    std::string res;
    for (size_t i = 0; i < str.length(); ++i) {
        if (str[i] == '%') {
            if (i + 2 < str.length()) {
                char hex[3] = { str[i+1], str[i+2], '\0' };
                res += (char)strtol(hex, NULL, 16);
                i += 2;
            }
        } else if (str[i] == '+') {
            res += ' ';
        } else {
            res += str[i];
        }
    }
    return res;
}

// ============================================================
// MySQL config
// ============================================================

const char* DB_HOST = "localhost";
const char* DB_USER = "root";
const char* DB_PASS = "";
const char* DB_NAME = "drums";

MYSQL* conn;

// ============================================================
// main()
// ============================================================

int main() {
    // ----- 1. Initialise Winsock (Windows-specific) -----
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        fprintf(stderr, "WSAStartup failed.\n");
        return 1;
    }

    // ----- 2. Seed random (Windows uses GetTickCount for better entropy) -----
    srand((unsigned int)GetTickCount());

    // ----- 3. Connect to MySQL (unchanged logic) -----
    conn = mysql_init(NULL);
    if (!mysql_real_connect(conn, DB_HOST, DB_USER, DB_PASS, DB_NAME, 0, NULL, 0)) {
        fprintf(stderr, "MySQL connection failed: %s\n", mysql_error(conn));
    } else {
        std::cout << "Successfully connected to MySQL database: " << DB_NAME << std::endl;
        bool reconnect = true;
        mysql_options(conn, MYSQL_OPT_RECONNECT, &reconnect);
        mysql_query(conn,
            "CREATE TABLE IF NOT EXISTS boards ("
            "id INT AUTO_INCREMENT PRIMARY KEY, "
            "user_id INT NOT NULL, "
            "title VARCHAR(255) DEFAULT 'Untitled Board', "
            "state LONGTEXT, "
            "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP, "
            "FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE)");
    }

    // ----- 4. Create socket (AF_INET6 dual-stack, same as original) -----
    SOCKET server_fd, new_socket;   // SOCKET type instead of int on Windows
    struct sockaddr_in6 address;
    int opt = 1;
    int addrlen = sizeof(address);
    char buffer[65536] = {0};
    char client_ip[INET6_ADDRSTRLEN];

    if ((server_fd = socket(AF_INET6, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        perror("socket failed");
        WSACleanup();
        exit(EXIT_FAILURE);
    }

    int ipv6only = 0;
    if (setsockopt(server_fd, IPPROTO_IPV6, IPV6_V6ONLY,
                   (const char*)&ipv6only, sizeof(ipv6only)) < 0) {
        perror("setsockopt IPV6_V6ONLY");
        WSACleanup();
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR,
                   (const char*)&opt, sizeof(opt))) {
        perror("setsockopt");
        WSACleanup();
        exit(EXIT_FAILURE);
    }

    memset(&address, 0, sizeof(address));
    address.sin6_family = AF_INET6;
    address.sin6_addr   = in6addr_any;
    address.sin6_port   = htons(80);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind failed");
        WSACleanup();
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 10) < 0) {
        perror("listen");
        WSACleanup();
        exit(EXIT_FAILURE);
    }

    std::cout << "Server listening on port 80 (IPv4 and IPv6)" << std::endl;

    // ----- 5. Accept loop -----
    while (true) {
        if ((new_socket = accept(server_fd, (struct sockaddr*)&address,
                                 (socklen_t*)&addrlen)) == INVALID_SOCKET) {
            perror("accept");
            continue;
        }

        inet_ntop(AF_INET6, &address.sin6_addr, client_ip, sizeof(client_ip));

        memset(buffer, 0, sizeof(buffer));
        // --- Changed: recv() instead of read() ---
        int header_read = recv(new_socket, buffer, sizeof(buffer) - 1, 0);
        if (header_read <= 0) { closesocket(new_socket); continue; }
        std::string reqbuffer(buffer, header_read);

        std::istringstream reqstream(reqbuffer);
        std::string method, path;
        reqstream >> method >> path;

        int content_length = 0;
        {
            auto cl_pos = reqbuffer.find("Content-Length: ");
            if (cl_pos == std::string::npos) cl_pos = reqbuffer.find("content-length: ");
            if (cl_pos != std::string::npos)
                content_length = std::stoi(reqbuffer.substr(cl_pos + 16));
        }

        auto body_start = reqbuffer.find("\r\n\r\n");
        std::string body;
        if (body_start != std::string::npos)
            body = reqbuffer.substr(body_start + 4);

        if (content_length > 0 && (int)body.size() < content_length) {
            int remaining = content_length - (int)body.size();
            std::vector<char> rest(remaining);
            int total_read = 0;
            while (total_read < remaining) {
                // --- Changed: recv() instead of read() ---
                int n = recv(new_socket, rest.data() + total_read,
                             remaining - total_read, 0);
                if (n <= 0) break;
                total_read += n;
            }
            body.append(rest.data(), total_read);
        }

        // --- Session extraction (unchanged) ---
        Session cur_session = {-1, ""};
        auto sid_pos = reqbuffer.find("session_id=");
        if (sid_pos != std::string::npos) {
            auto end = reqbuffer.find(";", sid_pos);
            if (end == std::string::npos) end = reqbuffer.find("\r\n", sid_pos);
            std::string sid = reqbuffer.substr(sid_pos + 11, end - (sid_pos + 11));
            if (sessions.count(sid)) cur_session = sessions[sid];
        }

        // ============================================================
        // API Routes (all logic unchanged — only file_exists and
        // create_directories calls use our shims instead of std::filesystem)
        // ============================================================

        if (method == "GET" && path == "/api/me") {
            if (cur_session.user_id != -1) {
                std::string json = "{\"username\":\"" + cur_session.username + "\"}";
                std::string resp = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " + std::to_string(json.size()) + "\r\n\r\n" + json;
                send(new_socket, resp.c_str(), (int)resp.size(), 0);
            } else {
                std::string resp = "HTTP/1.1 401 Unauthorized\r\n\r\n";
                send(new_socket, resp.c_str(), (int)resp.size(), 0);
            }
        } else if (method == "GET" && path == "/api/page/login") {
            std::string html = "<div class=\"auth-card\"><h2>Login</h2><form id=\"loginForm\"><div class=\"form-group\"><label>Username</label><input name=\"username\" required></div><div class=\"form-group\"><label>Password</label><input name=\"password\" type=\"password\" required></div><button type=\"submit\" class=\"auth-btn\">Log in</button></form><p class=\"auth-switch\">Don't have an account? <a href=\"#\" id=\"goto-signup\">Sign up</a></p></div>";
            std::string resp = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: " + std::to_string(html.size()) + "\r\n\r\n" + html;
            send(new_socket, resp.c_str(), (int)resp.size(), 0);
        } else if (method == "GET" && path == "/api/page/signup") {
            std::string html = "<div class=\"auth-card\"><h2>Sign Up</h2><form id=\"signupForm\"><div class=\"form-group\"><label>Username</label><input name=\"username\" required></div><div class=\"form-group\"><label>Password</label><input name=\"password\" type=\"password\" required></div><div class=\"form-group\"><label>Confirm</label><input name=\"confirm\" type=\"password\" required></div><button type=\"submit\" class=\"auth-btn\">Create Account</button></form><p class=\"auth-switch\">Already have an account? <a href=\"#\" id=\"goto-login\">Log in</a></p></div>";
            std::string resp = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: " + std::to_string(html.size()) + "\r\n\r\n" + html;
            send(new_socket, resp.c_str(), (int)resp.size(), 0);
        } else if (method == "POST" && path == "/api/login") {
            std::string username = extract_json_field(body, "username");
            std::string password = extract_json_field(body, "password");
            std::string query = "SELECT id FROM users WHERE username='" + username + "' AND password='" + password + "'";
            if (mysql_query(conn, query.c_str()) == 0) {
                MYSQL_RES* res = mysql_store_result(conn);
                if (res && mysql_num_rows(res) > 0) {
                    MYSQL_ROW row = mysql_fetch_row(res);
                    int uid = atoi(row[0]);
                    std::string sid = generate_session_id();
                    sessions[sid] = {uid, username};
                    std::string resp = "HTTP/1.1 200 OK\r\nSet-Cookie: session_id=" + sid + "; Path=/; HttpOnly\r\nContent-Type: application/json\r\nContent-Length: 15\r\n\r\n{\"status\":\"ok\"}";
                    send(new_socket, resp.c_str(), (int)resp.size(), 0);
                } else {
                    std::string resp = "HTTP/1.1 401 Unauthorized\r\n\r\n";
                    send(new_socket, resp.c_str(), (int)resp.size(), 0);
                }
                if (res) mysql_free_result(res);
            }
        } else if (method == "POST" && path == "/api/signup") {
            std::string username = extract_json_field(body, "username");
            std::string password = extract_json_field(body, "password");
            std::string query = "INSERT INTO users (username, password) VALUES ('" + username + "', '" + password + "')";
            if (mysql_query(conn, query.c_str()) == 0) {
                std::string resp = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: 15\r\n\r\n{\"status\":\"ok\"}";
                send(new_socket, resp.c_str(), (int)resp.size(), 0);
            } else if (mysql_errno(conn) == 1062) {
                std::string resp = "HTTP/1.1 409 Conflict\r\n\r\n";
                send(new_socket, resp.c_str(), (int)resp.size(), 0);
            } else {
                std::string resp = "HTTP/1.1 500 Internal Error\r\n\r\n";
                send(new_socket, resp.c_str(), (int)resp.size(), 0);
            }
        } else if (method == "POST" && path == "/api/logout") {
            auto pos = reqbuffer.find("session_id=");
            if (pos != std::string::npos) {
                auto end = reqbuffer.find(";", pos);
                if (end == std::string::npos) end = reqbuffer.find("\r\n", pos);
                std::string sid = reqbuffer.substr(pos + 11, end - (pos + 11));
                sessions.erase(sid);
            }
            std::string resp = "HTTP/1.1 200 OK\r\nSet-Cookie: session_id=; Path=/; Expires=Thu, 01 Jan 1970 00:00:00 GMT\r\n\r\n";
            send(new_socket, resp.c_str(), (int)resp.size(), 0);

        } else if (path == "/api/boards") {
            if (cur_session.user_id == -1) {
                std::string resp = "HTTP/1.1 401 Unauthorized\r\n\r\n";
                send(new_socket, resp.c_str(), (int)resp.size(), 0);
            } else if (method == "GET") {
                std::string query = "SELECT id, title FROM boards WHERE user_id=" + std::to_string(cur_session.user_id);
                mysql_query(conn, query.c_str());
                MYSQL_RES* res = mysql_store_result(conn);
                std::string json = "[";
                MYSQL_ROW row;
                while (res && (row = mysql_fetch_row(res))) {
                    if (json.size() > 1) json += ",";
                    json += "{\"id\":" + std::string(row[0]) + ",\"title\":\"" + std::string(row[1]) + "\"}";
                }
                json += "]";
                if (res) mysql_free_result(res);
                std::string resp = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " + std::to_string(json.size()) + "\r\n\r\n" + json;
                send(new_socket, resp.c_str(), (int)resp.size(), 0);
            } else if (method == "POST") {
                std::string id    = extract_json_field(body, "id");
                std::string title = extract_json_field(body, "title");
                std::string state = extract_json_field(body, "state");

                char* escaped = new char[state.size() * 2 + 1];
                mysql_real_escape_string(conn, escaped, state.c_str(), state.size());

                std::string query;
                if (!id.empty()) {
                    query = "UPDATE boards SET title='" + title + "', state='" + std::string(escaped) + "' WHERE id=" + id + " AND user_id=" + std::to_string(cur_session.user_id);
                } else {
                    query = "INSERT INTO boards (user_id, title, state) VALUES (" + std::to_string(cur_session.user_id) + ",'" + title + "','" + std::string(escaped) + "')";
                }
                delete[] escaped;

                if (mysql_query(conn, query.c_str()) == 0) {
                    std::string last_id = id.empty() ? std::to_string(mysql_insert_id(conn)) : id;
                    std::string json = "{\"status\":\"ok\",\"id\":" + last_id + "}";
                    std::string resp = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " + std::to_string(json.size()) + "\r\n\r\n" + json;
                    send(new_socket, resp.c_str(), (int)resp.size(), 0);
                } else {
                    std::string resp = "HTTP/1.1 500 Internal Error\r\n\r\n";
                    send(new_socket, resp.c_str(), (int)resp.size(), 0);
                }
            }
        } else if (method == "GET" && path.rfind("/api/boards/", 0) == 0) {
            std::string bid = path.substr(12);
            std::string query = "SELECT state FROM boards WHERE id=" + bid + " AND user_id=" + std::to_string(cur_session.user_id);
            mysql_query(conn, query.c_str());
            MYSQL_RES* res = mysql_store_result(conn);
            if (res && mysql_num_rows(res) > 0) {
                MYSQL_ROW row = mysql_fetch_row(res);
                std::string state = row[0];
                std::string resp = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " + std::to_string(state.size()) + "\r\n\r\n" + state;
                send(new_socket, resp.c_str(), (int)resp.size(), 0);
            } else {
                std::string resp = "HTTP/1.1 404 Not Found\r\n\r\n";
                send(new_socket, resp.c_str(), (int)resp.size(), 0);
            }
            if (res) mysql_free_result(res);
        } else if (method == "DELETE" && path.rfind("/api/boards/", 0) == 0) {
            if (cur_session.user_id == -1) {
                std::string resp = "HTTP/1.1 401 Unauthorized\r\n\r\n";
                send(new_socket, resp.c_str(), (int)resp.size(), 0);
            } else {
                mysql_ping(conn);

                std::string bid = path.substr(12);
                std::cout << "DELETING BOARD: Raw bid='" << bid << "' For User=" << cur_session.user_id << std::endl;

                bid.erase(bid.find_last_not_of(" \n\r\t") + 1);
                bid.erase(0, bid.find_first_not_of(" \n\r\t"));

                bool valid = !bid.empty();
                for (char c : bid) if (!isdigit(c)) valid = false;

                if (!valid) {
                    std::cerr << "INVALID BOARD ID: '" << bid << "'" << std::endl;
                    std::string json = "{\"status\":\"error\",\"message\":\"Invalid board ID: '" + bid + "'\"}";
                    std::string resp = "HTTP/1.1 400 Bad Request\r\nContent-Type: application/json\r\nContent-Length: " + std::to_string(json.size()) + "\r\n\r\n" + json;
                    send(new_socket, resp.c_str(), (int)resp.size(), 0);
                } else {
                    std::string query = "DELETE FROM boards WHERE id=" + bid + " AND user_id=" + std::to_string(cur_session.user_id);
                    std::cout << "EXECUTING QUERY: " << query << std::endl;
                    if (mysql_query(conn, query.c_str()) == 0) {
                        int affected = (int)mysql_affected_rows(conn);
                        std::cout << "DELETE SUCCESS. AFFECTED ROWS: " << affected << std::endl;
                        std::string json = "{\"status\":\"ok\",\"affected\":" + std::to_string(affected) + "}";
                        std::string resp = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " + std::to_string(json.size()) + "\r\n\r\n" + json;
                        send(new_socket, resp.c_str(), (int)resp.size(), 0);
                    } else {
                        std::string err = mysql_error(conn);
                        std::cerr << "MYSQL DELETE ERROR: " << err << std::endl;
                        std::string json = "{\"status\":\"error\",\"message\":\"MySQL Error: " + err + "\"}";
                        std::string resp = "HTTP/1.1 500 Internal Error\r\nContent-Type: application/json\r\nContent-Length: " + std::to_string(json.size()) + "\r\n\r\n" + json;
                        send(new_socket, resp.c_str(), (int)resp.size(), 0);
                    }
                }
            }
        } else if (method == "GET" && path.rfind("/api/compile/status", 0) == 0) {
            auto pos = path.find("task_id=");
            if (pos == std::string::npos) {
                std::string res = "HTTP/1.1 400 Bad Request\r\n\r\n";
                send(new_socket, res.c_str(), (int)res.size(), 0);
            } else {
                std::string tid = path.substr(pos + 8);
                if (tid.find(".json") != std::string::npos)
                    tid = tid.substr(0, tid.find(".json"));

                std::string decoded_tid = url_decode(tid);
                std::string out_path = "folderUSER/rtimgsUSER/Results_" + decoded_tid + ".xlsx";

                // --- Changed: file_exists() shim instead of std::filesystem::exists() ---
                if (file_exists(out_path)) {
                    std::string url  = "/" + out_path;
                    std::string json = "{\"status\":\"completed\",\"url\":\"" + url + "\",\"filename\":\"Results_" + tid + ".xlsx\"}";
                    std::string resp = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " + std::to_string(json.size()) + "\r\n\r\n" + json;
                    send(new_socket, resp.c_str(), (int)resp.size(), 0);
                } else {
                    std::string json = "{\"status\":\"pending\"}";
                    std::string resp = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " + std::to_string(json.size()) + "\r\n\r\n" + json;
                    send(new_socket, resp.c_str(), (int)resp.size(), 0);
                }
            }
        } else if (method == "POST" && path == "/api/compile") {
            if (cur_session.user_id == -1) {
                std::string resp = "HTTP/1.1 401 Unauthorized\r\n\r\n";
                send(new_socket, resp.c_str(), (int)resp.size(), 0);
            } else {
                time_t now = time(NULL);
                char timestamp[20];
                // localtime_s is MSVC-only; MinGW uses localtime() which is fine here
                struct tm* timeinfo = localtime(&now);
                strftime(timestamp, sizeof(timestamp), "%Y%m%d%H%M%S", timeinfo);

                std::string filename = cur_session.username + "_" + std::string(timestamp) + ".json";
                std::ofstream out(filename);
                out << body;
                out.close();

                std::string json = "{\"status\":\"ok\",\"task_id\":\"" + filename + "\"}";
                std::string resp = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " + std::to_string(json.size()) + "\r\n\r\n" + json;
                send(new_socket, resp.c_str(), (int)resp.size(), 0);
            }
        } else if (method == "POST" && path == "/api/upload") {
            std::string boundary;
            auto ct_pos = reqbuffer.find("boundary=");
            if (ct_pos != std::string::npos) {
                boundary = "--" + reqbuffer.substr(ct_pos + 9, reqbuffer.find("\r\n", ct_pos) - (ct_pos + 9));
            }
            std::string side = "a", filename, file_data;
            if (!boundary.empty()) {
                size_t p = 0;
                while ((p = body.find(boundary, p)) != std::string::npos) {
                    p += boundary.size();
                    if (body.substr(p, 2) == "--") break;
                    auto hend = body.find("\r\n\r\n", p);
                    if (hend == std::string::npos) break;
                    std::string headers = body.substr(p, hend - p);
                    auto dstart = hend + 4;
                    auto dnext  = body.find(boundary, dstart);
                    std::string data = body.substr(dstart, (dnext == std::string::npos ? body.size() : dnext) - dstart - 2);
                    if (headers.find("name=\"side\"") != std::string::npos) side = data;
                    else if (headers.find("name=\"file\"") != std::string::npos) {
                        auto fp = headers.find("filename=\"");
                        if (fp != std::string::npos)
                            filename = headers.substr(fp + 10, headers.find("\"", fp + 10) - (fp + 10));
                        file_data = data;
                    }
                }
            }
            if (!filename.empty()) {
                std::string dir = (side == "a") ? "folderUSER/lfimgsUSER" : "folderUSER/rtimgsUSER";
                // --- Changed: create_directories() shim instead of std::filesystem::create_directories() ---
                create_directories(dir);
                std::string filepath = dir + "/" + filename;
                std::ofstream out(filepath, std::ios::binary);
                out.write(file_data.c_str(), file_data.size());
                std::string url  = "/" + filepath;
                std::string json = "{\"status\":\"ok\",\"url\":\"" + url + "\"}";
                std::string resp = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " + std::to_string(json.size()) + "\r\n\r\n" + json;
                send(new_socket, resp.c_str(), (int)resp.size(), 0);
            }
        } else {
            // --- Static file serving (unchanged logic) ---
            std::string file_path = (path == "/") ? "index.html" : path.substr(1);
            std::ifstream file(file_path, std::ios::binary);
            bool is_fallback = false;
            if (!file && file_path.find('.') == std::string::npos) {
                file.open("index.html", std::ios::binary);
                is_fallback = true;
            }

            if (file) {
                std::string c((std::istreambuf_iterator<char>(file)), {});
                std::string type = "text/plain";
                if (is_fallback || file_path.find(".html") != std::string::npos) type = "text/html";
                else if (file_path.find(".js")   != std::string::npos) type = "application/javascript";
                else if (file_path.find(".css")  != std::string::npos) type = "text/css";
                else if (file_path.find(".png")  != std::string::npos) type = "image/png";
                else if (file_path.find(".xlsx") != std::string::npos) type = "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet";
                std::string response = "HTTP/1.1 200 OK\r\nContent-Type: " + type + "\r\nContent-Length: " + std::to_string(c.size()) + "\r\n\r\n" + c;
                send(new_socket, response.c_str(), (int)response.size(), 0);
            } else {
                std::string res = "HTTP/1.1 404 Not Found\r\n\r\n";
                send(new_socket, res.c_str(), (int)res.size(), 0);
            }
        }

        // --- Changed: closesocket() instead of close() ---
        closesocket(new_socket);
    }

    // --- Cleanup (Windows-specific) ---
    closesocket(server_fd);
    WSACleanup();
    return 0;
}
