#include <sys/socket.h> // It is used for socket programming (specifically for creating and managing sockets ), which is a way to send and receive data between two computers over a network.
#include <netinet/in.h> // It is used for defining structures and constants related to internet addresses and ports.
#include <arpa/inet.h> // It is used for converting IP addresses between different formats.
#include <unistd.h> // It is used for POSIX operating system API, which provides functions for file I/O, process control, and other system-level operations.
#include <string.h> // It is used for string manipulation functions.
#include <stdio.h> // It is used for standard input/output functions.
#include <sys/stat.h> // It is used for file system operations.
#include <fcntl.h> // It is used for file control operations.
#include <stdlib.h> // It is used for standard library functions.
#include <iostream> // It is used for input/output operations.
#include <fstream> // It is used for file stream operations.
#include <sstream> // It is used for string stream operations.
#include <filesystem> // It is used for file system operations.
#include <vector> // It is used for dynamic arrays.
#include <algorithm> // It is used for algorithms.
#include <map> // It is used for associative arrays.
#include <random> // It is used for random number generation.
#include <mysql/mysql.h> // It is used for MySQL database operations.
const std::string FRONTEND_ORIGIN = "https://drum-project-o4h3.onrender.com";

const std::string CORS_HEADERS =
    "Access-Control-Allow-Origin: " + FRONTEND_ORIGIN + "\r\n"
    "Access-Control-Allow-Credentials: true\r\n"
    "Access-Control-Allow-Headers: Content-Type\r\n"
    "Access-Control-Allow-Methods: GET, POST, OPTIONS, DELETE\r\n";
struct Session {
    int user_id;
    std::string username;
};

std::map<std::string, Session> sessions; // session_id -> Session

std::string generate_session_id() {
    static const char alphanum[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    std::string tmp_s; // It is used to store the session ID.
    tmp_s.reserve(32); // Stack memory of server it reserve memory for 32 characters (bytes).
    for (int i = 0; i < 32; ++i) tmp_s += alphanum[rand() % (sizeof(alphanum) - 1)]; // It is used to generate the session ID.
    return tmp_s;
}

std::string extract_json_field(const std::string& json, const std::string& key) {
    std::string search_key = "\"" + key + "\":";
    auto p = json.find(search_key);
    if (p == std::string::npos) return "";
    p += search_key.length();
    
    // Skip whitespace
    while (p < json.length() && isspace(json[p])) p++;
    
    if (p < json.length() && json[p] == '\"') {
        p++; // skip start quote
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

const char* DB_HOST = getenv("DB_HOST");
const char* DB_USER = getenv("DB_USER");
const char* DB_PASS = getenv("DB_PASS");
const char* DB_NAME = getenv("DB_NAME");
const char* DB_PORT = getenv("DB_PORT");

MYSQL* conn; 

int main() {
    int server_fd, new_socket;
    struct sockaddr_in6 address;
    int opt = 1;
    int addrlen = sizeof(address);
    char buffer[65536] = {0}; // Increased buffer for larger JSON states
    char client_ip[INET6_ADDRSTRLEN];

    conn = mysql_init(NULL);

    unsigned int dbport = DB_PORT ? atoi(DB_PORT) : 3306;

    if (!mysql_real_connect(conn, DB_HOST, DB_USER, DB_PASS, DB_NAME, dbport, NULL, 0)) {
        fprintf(stderr, "MySQL connection failed: %s\n", mysql_error(conn));
    } else {
        std::cout << "Successfully connected to MySQL database: " << DB_NAME << std::endl;

        bool reconnect = true;
        mysql_options(conn, MYSQL_OPT_RECONNECT, &reconnect);

        mysql_query(conn,
            "CREATE TABLE IF NOT EXISTS boards ("
            "id INT AUTO_INCREMENT PRIMARY KEY,"
            "user_id INT NOT NULL,"
            "title VARCHAR(255) DEFAULT 'Untitled Board',"
            "state LONGTEXT,"
            "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
            "FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE)"
        );
    }
    if ((server_fd = socket(AF_INET6, SOCK_STREAM, 0)) == 0) { //socket funtion take 3 arguments (Domain, Type, Protocol): AF_INET6 (IPv6), SOCK_STREAM (TCP), 0 (IP) and returns a file descriptor
        perror("socket failed");
        exit(EXIT_FAILURE); // EXIT_FAILURE is a macro that is used to indicate that the program has failed, exit() is a function that is used to terminate the program
    }

    int ipv6only = 0; // ipv6only is a variable that is used to indicate that the program is using IPv6, if it is 1 then the program is using IPv6, if it is 0 then the program is using IPv4 
    if (setsockopt(server_fd, IPPROTO_IPV6, IPV6_V6ONLY, &ipv6only, sizeof(ipv6only)) < 0) { // setsockopt is a function that is used to set socket options
        perror("setsockopt IPV6_V6ONLY"); // perror is a function that is used to print the error message
        exit(EXIT_FAILURE); //Macro is  a identifier that is used to replace a value or expression
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) { // SOL_SOCKET is a macro that is used to indicate that the socket option is for the socket level, SO_REUSEADDR is a macro that is used to indicate that the socket option is for the socket level
        perror("setsockopt"); 
        exit(EXIT_FAILURE);
    }
    
    memset(&address, 0, sizeof(address)); // memset is a function that is used to fill a block of memory with a specified value, parameters are (pointer to the memory block, value to be filled, size of the memory block)
    address.sin6_family = AF_INET6; // AF_INET6 is a macro that is used to indicate that the address family is IPv6
    address.sin6_addr = in6addr_any; // in6addr_any is a macro that is used to indicate that the address is any
    int port = 10000;
    char* envPort = getenv("PORT");
    if (envPort) port = atoi(envPort);

    address.sin6_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) { // bind is a function that is used to bind a socket to a port
        perror("bind failed"); 
        exit(EXIT_FAILURE);// In operating system a socket is a endpoint for sending or receiving data across a computer network, under the hood it is a file descriptor
        // A file descriptor is a non-negative integer that is used to identify a file, socket, or other I/O resource
    }
    
    if (listen(server_fd, 10) < 0) { // listen is a function that is used to listen for incoming connections
        perror("listen");
        exit(EXIT_FAILURE);
    }
    // perror is a function that is used to print the error message
    std::cout << "Server listening on port " << port << std::endl;

    while (true) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
            perror("accept");
            continue;
        }
        
        inet_ntop(AF_INET6, &address.sin6_addr, client_ip, sizeof(client_ip)); // inet_ntop is a function that is used to convert an IPv6 address to a string
        
        memset(buffer, 0, sizeof(buffer)); // memset is a function that is used to fill a block of memory with a specified value, parameters are (pointer to the memory block, value to be filled, size of the memory block)
        int header_read = read(new_socket, buffer, sizeof(buffer) - 1); // read is a function that is used to read data from a file descriptor
        if (header_read <= 0) { close(new_socket); continue; }
        std::string reqbuffer(buffer, header_read);

        std::istringstream reqstream(reqbuffer); //Is reqstream is a function that is used to read data from a string stream
        std::string method, path;
        reqstream >> method >> path;
        if (method == "OPTIONS") {
            std::string response =
                "HTTP/1.1 204 No Content\r\n"
                "Access-Control-Allow-Origin: https://drum-project-o4h3.onrender.com\r\n"
                "Access-Control-Allow-Credentials: true\r\n"
                "Access-Control-Allow-Headers: Content-Type\r\n"
                "Access-Control-Allow-Methods: GET, POST, DELETE, OPTIONS\r\n"
                "\r\n";

            send(new_socket, response.c_str(), response.size(), 0);
            close(new_socket);
            continue;
        }
        int content_length = 0;
        {
            auto cl_pos = reqbuffer.find("Content-Length: ");
            if (cl_pos == std::string::npos) cl_pos = reqbuffer.find("content-length: ");
            if (cl_pos != std::string::npos) {
                content_length = std::stoi(reqbuffer.substr(cl_pos + 16));
            }
        }

        auto body_start = reqbuffer.find("\r\n\r\n");
        std::string body;
        if (body_start != std::string::npos) {
            body = reqbuffer.substr(body_start + 4);
        }

        if (content_length > 0 && (int)body.size() < content_length) {
            int remaining = content_length - body.size();
            std::vector<char> rest(remaining);
            int total_read = 0;
            while (total_read < remaining) {
                int n = read(new_socket, rest.data() + total_read, remaining - total_read);
                if (n <= 0) break;
                total_read += n;
            }
            body.append(rest.data(), total_read);
        }

        // --- Helper: Get Session ---
        Session cur_session = {-1, ""};
        auto sid_pos = reqbuffer.find("session_id=");
        if (sid_pos != std::string::npos) {
            auto end = reqbuffer.find(";", sid_pos);
            if (end == std::string::npos) end = reqbuffer.find("\r\n", sid_pos);
            std::string sid = reqbuffer.substr(sid_pos + 11, end - (sid_pos + 11));
            if (sessions.count(sid)) cur_session = sessions[sid];
        }

        // --- API Routes ---
        if (method == "GET" && path == "/api/me") {
            if (cur_session.user_id != -1) {
                std::string json = "{\"username\":\"" + cur_session.username + "\"}";
                std::string resp = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " + std::to_string(json.size()) + "\r\n\r\n" + json;
                send(new_socket, resp.c_str(), resp.size(), 0);
            } else {
                std::string resp = "HTTP/1.1 401 Unauthorized\r\n\r\n";
                send(new_socket, resp.c_str(), resp.size(), 0);
            }
        } else if (method == "GET" && path == "/api/page/login") {
            std::string html = "<div class=\"auth-card\"><h2>Login</h2><form id=\"loginForm\"><div class=\"form-group\"><label>Username</label><input name=\"username\" required></div><div class=\"form-group\"><label>Password</label><input name=\"password\" type=\"password\" required></div><button type=\"submit\" class=\"auth-btn\">Log in</button></form><p class=\"auth-switch\">Don't have an account? <a href=\"#\" id=\"goto-signup\">Sign up</a></p></div>";
            std::string resp =
                "HTTP/1.1 200 OK\r\n" +
                CORS_HEADERS +
                "Content-Type: text/html\r\nContent-Length: " +
                std::to_string(html.size()) +
                "\r\n\r\n" + html;
            send(new_socket, resp.c_str(), resp.size(), 0);
        } else if (method == "GET" && path == "/api/page/signup") {
            std::string html = "<div class=\"auth-card\"><h2>Sign Up</h2><form id=\"signupForm\"><div class=\"form-group\"><label>Username</label><input name=\"username\" required></div><div class=\"form-group\"><label>Password</label><input name=\"password\" type=\"password\" required></div><div class=\"form-group\"><label>Confirm</label><input name=\"confirm\" type=\"password\" required></div><button type=\"submit\" class=\"auth-btn\">Create Account</button></form><p class=\"auth-switch\">Already have an account? <a href=\"#\" id=\"goto-login\">Log in</a></p></div>";
            std::string resp =
                "HTTP/1.1 200 OK\r\n" +
                CORS_HEADERS +
                "Content-Type: text/html\r\nContent-Length: " +
                std::to_string(html.size()) +
                "\r\n\r\n" + html;
            send(new_socket, resp.c_str(), resp.size(), 0);
        } else if (method == "POST" && path == "/api/login") {
            std::string username = extract_json_field(body, "username");
            std::string password = extract_json_field(body, "password");

            std::string query =
                "SELECT id FROM users WHERE username='" +
                username +
                "' AND password='" +
                password +
                "'";

            if (mysql_query(conn, query.c_str()) == 0) {

                MYSQL_RES* res = mysql_store_result(conn);

                if (res && mysql_num_rows(res) > 0) {

                    MYSQL_ROW row = mysql_fetch_row(res);
                    int uid = atoi(row[0]);

                    std::string sid = generate_session_id();
                    sessions[sid] = {uid, username};

                    std::string resp =
                        "HTTP/1.1 200 OK\r\n" +
                        CORS_HEADERS +
                        "Set-Cookie: session_id=" + sid + "; Path=/; HttpOnly\r\n" +
                        "Content-Type: application/json\r\n" +
                        "Content-Length: 15\r\n\r\n" +
                        "{\"status\":\"ok\"}";

                    send(new_socket, resp.c_str(), resp.size(), 0);

                } else {

                    std::string resp =
                        "HTTP/1.1 401 Unauthorized\r\n" +
                        CORS_HEADERS +
                        "\r\n";

                    send(new_socket, resp.c_str(), resp.size(), 0);
                }

                if (res) mysql_free_result(res);

            } else {

                std::string resp =
                    "HTTP/1.1 500 Internal Error\r\n" +
                    CORS_HEADERS +
                    "\r\n";

                send(new_socket, resp.c_str(), resp.size(), 0);
            }
        } 
        else if (method == "POST" && path == "/api/signup") {

            std::string username = extract_json_field(body, "username");
            std::string password = extract_json_field(body, "password");

            std::string query =
                "INSERT INTO users (username, password) VALUES ('" +
                username + "', '" + password + "')";

            if (mysql_query(conn, query.c_str()) == 0) {

                std::string resp =
                    "HTTP/1.1 200 OK\r\n" +
                    CORS_HEADERS +
                    "Content-Type: application/json\r\n" +
                    "Content-Length: 15\r\n\r\n" +
                    "{\"status\":\"ok\"}";

                send(new_socket, resp.c_str(), resp.size(), 0);

            } else if (mysql_errno(conn) == 1062) {

                std::string resp =
                    "HTTP/1.1 409 Conflict\r\n" +
                    CORS_HEADERS +
                    "\r\n";

                send(new_socket, resp.c_str(), resp.size(), 0);

            } else {

                std::string resp =
                    "HTTP/1.1 500 Internal Error\r\n" +
                    CORS_HEADERS +
                    "\r\n";

                send(new_socket, resp.c_str(), resp.size(), 0);
            }
        }
        else if (method == "POST" && path == "/api/logout") {
            auto pos = reqbuffer.find("session_id=");
            if (pos != std::string::npos) {
                auto end = reqbuffer.find(";", pos);
                if (end == std::string::npos) end = reqbuffer.find("\r\n", pos);
                std::string sid = reqbuffer.substr(pos + 11, end - (pos + 11));
                sessions.erase(sid);
            }
            std::string resp = "HTTP/1.1 200 OK\r\nSet-Cookie: session_id=; Path=/; Expires=Thu, 01 Jan 1970 00:00:00 GMT\r\n\r\n";
            send(new_socket, resp.c_str(), resp.size(), 0);

        } else if (path == "/api/boards") {
            if (cur_session.user_id == -1) {
                std::string resp = "HTTP/1.1 401 Unauthorized\r\n\r\n";
                send(new_socket, resp.c_str(), resp.size(), 0);
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
                send(new_socket, resp.c_str(), resp.size(), 0);
            } else if (method == "POST") {
                std::string id = extract_json_field(body, "id");
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
                    send(new_socket, resp.c_str(), resp.size(), 0);
                } else {
                    std::string resp = "HTTP/1.1 500 Internal Error\r\n\r\n";
                    send(new_socket, resp.c_str(), resp.size(), 0);
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
                send(new_socket, resp.c_str(), resp.size(), 0);
            } else {
                std::string resp = "HTTP/1.1 404 Not Found\r\n\r\n";
                send(new_socket, resp.c_str(), resp.size(), 0);
            }
            if (res) mysql_free_result(res);
        } else if (method == "DELETE" && path.rfind("/api/boards/", 0) == 0) {
            if (cur_session.user_id == -1) {
                std::string resp = "HTTP/1.1 401 Unauthorized\r\n\r\n";
                send(new_socket, resp.c_str(), resp.size(), 0);
            } else {
                // Ensure connection is still alive
                mysql_ping(conn);
                
                std::string bid = path.substr(12);
                std::cout << "DELETING BOARD: Raw bid='" << bid << "' For User=" << cur_session.user_id << std::endl;
                
                // Trim potential whitespace
                bid.erase(bid.find_last_not_of(" \n\r\t") + 1);
                bid.erase(0, bid.find_first_not_of(" \n\r\t"));

                // Sanitize bid: only allow digits
                bool valid = !bid.empty();
                for (char c : bid) if (!isdigit(c)) valid = false;

                if (!valid) {
                    std::cerr << "INVALID BOARD ID: '" << bid << "'" << std::endl;
                    std::string json = "{\"status\":\"error\",\"message\":\"Invalid board ID: '" + bid + "'\"}";
                    std::string resp = "HTTP/1.1 400 Bad Request\r\nContent-Type: application/json\r\nContent-Length: " + std::to_string(json.size()) + "\r\n\r\n" + json;
                    send(new_socket, resp.c_str(), resp.size(), 0);
                } else {
                    std::string query = "DELETE FROM boards WHERE id=" + bid + " AND user_id=" + std::to_string(cur_session.user_id);
                    std::cout << "EXECUTING QUERY: " << query << std::endl;
                    if (mysql_query(conn, query.c_str()) == 0) {
                        int affected = (int)mysql_affected_rows(conn);
                        std::cout << "DELETE SUCCESS. AFFECTED ROWS: " << affected << std::endl;
                        std::string json = "{\"status\":\"ok\",\"affected\":" + std::to_string(affected) + "}";
                        std::string resp = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " + std::to_string(json.size()) + "\r\n\r\n" + json;
                        send(new_socket, resp.c_str(), resp.size(), 0);
                    } else {
                        std::string err = mysql_error(conn);
                        std::cerr << "MYSQL DELETE ERROR: " << err << std::endl;
                        std::string json = "{\"status\":\"error\",\"message\":\"MySQL Error: " + err + "\"}";
                        std::string resp = "HTTP/1.1 500 Internal Error\r\nContent-Type: application/json\r\nContent-Length: " + std::to_string(json.size()) + "\r\n\r\n" + json;
                        send(new_socket, resp.c_str(), resp.size(), 0);
                    }
                }
            }
        } else if (method == "GET" && path.rfind("/api/compile/status", 0) == 0) {
            auto pos = path.find("task_id=");
            if (pos == std::string::npos) {
                std::string res = "HTTP/1.1 400 Bad Request\r\n\r\n";
                send(new_socket, res.c_str(), res.size(), 0);
            } else {
                std::string tid = path.substr(pos + 8);
                // The task_id is filename like username_timestamp.json
                // We want just the name part before .json if it has it
                if (tid.find(".json") != std::string::npos) {
                    tid = tid.substr(0, tid.find(".json"));
                }
                
                std::string decoded_tid = url_decode(tid);
                std::string out_path = "folderUSER/rtimgsUSER/Results_" + decoded_tid + ".xlsx";
                
                if (std::filesystem::exists(out_path)) {
                    std::string url = "/" + out_path;
                    std::string json = "{\"status\":\"completed\",\"url\":\"" + url + "\",\"filename\":\"Results_" + tid + ".xlsx\"}";
                    std::string resp = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " + std::to_string(json.size()) + "\r\n\r\n" + json;
                    send(new_socket, resp.c_str(), resp.size(), 0);
                } else {
                    std::string json = "{\"status\":\"pending\"}";
                    std::string resp = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " + std::to_string(json.size()) + "\r\n\r\n" + json;
                    send(new_socket, resp.c_str(), resp.size(), 0);
                }
            }
        } else if (method == "POST" && path == "/api/compile") {
            if (cur_session.user_id == -1) {
                std::string resp = "HTTP/1.1 401 Unauthorized\r\n\r\n";
                send(new_socket, resp.c_str(), resp.size(), 0);
            } else {
                std::time_t now = std::time(nullptr);
                char timestamp[20];
                std::strftime(timestamp, sizeof(timestamp), "%Y%m%d%H%M%S", std::localtime(&now));
                
                std::string filename = cur_session.username + "_" + std::string(timestamp) + ".json";
                std::ofstream out(filename);
                out << body;
                out.close();

                std::string json = "{\"status\":\"ok\",\"task_id\":\"" + filename + "\"}";
                std::string resp = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " + std::to_string(json.size()) + "\r\n\r\n" + json;
                send(new_socket, resp.c_str(), resp.size(), 0);
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
                    auto dnext = body.find(boundary, dstart);
                    std::string data = body.substr(dstart, (dnext == std::string::npos ? body.size() : dnext) - dstart - 2);
                    if (headers.find("name=\"side\"") != std::string::npos) side = data;
                    else if (headers.find("name=\"file\"") != std::string::npos) {
                        auto fp = headers.find("filename=\"");
                        if (fp != std::string::npos) filename = headers.substr(fp + 10, headers.find("\"", fp + 10) - (fp + 10));
                        file_data = data;
                    }
                }
            }
            if (!filename.empty()) {
                std::string dir = (side == "a") ? "folderUSER/lfimgsUSER" : "folderUSER/rtimgsUSER";
                std::filesystem::create_directories(dir);
                std::string filepath = dir + "/" + filename;
                std::ofstream out(filepath, std::ios::binary);
                out.write(file_data.c_str(), file_data.size());
                std::string url = "/" + filepath;
                std::string json = "{\"status\":\"ok\",\"url\":\"" + url + "\"}";
                std::string resp = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " + std::to_string(json.size()) + "\r\n\r\n" + json;
                send(new_socket, resp.c_str(), resp.size(), 0);
            }
        } else {
            std::string file_path = (path == "/") ? "index.html" : path.substr(1);
            std::ifstream file(file_path, std::ios::binary);
            bool is_fallback = false;
            if (!file && file_path.find('.') == std::string::npos) {
                file.open("index.html", std::ios::binary); //ios::binary is a flag that is used to open a file in binary mode
                is_fallback = true;
            }

            if (file) {
                std::string c((std::istreambuf_iterator<char>(file)), {});
                std::string type = "text/plain";
                if (is_fallback || file_path.find(".html") != std::string::npos) type = "text/html";
                else if (file_path.find(".js") != std::string::npos) type = "application/javascript";
                else if (file_path.find(".css") != std::string::npos) type = "text/css";
                else if (file_path.find(".png") != std::string::npos) type = "image/png";
                else if (file_path.find(".xlsx") != std::string::npos) type = "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet";
                std::string response = "HTTP/1.1 200 OK\r\nContent-Type: " + type + "\r\nContent-Length: " + std::to_string(c.size()) + "\r\n\r\n" + c;
                send(new_socket, response.c_str(), response.size(), 0); // send() is a function that is used to send data to a socket, parameters are (socket descriptor, data to be sent, size of the data to be sent, flags)
            } else {
                std::string res = "HTTP/1.1 404 Not Found\r\n\r\n";
                send(new_socket, res.c_str(), res.size(), 0);
            }
        }
        close(new_socket);
    }
    return 0;
}
