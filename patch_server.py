import re

with open("server.cpp", "r") as f:
    text = f.read()

# 1. Includes
text = text.replace("#include <random>", "#include <random>\n#include <thread>\n#include <mutex>")

# 2. Mutex definitions
text = text.replace("std::map<std::string, Session> sessions;", "std::map<std::string, Session> sessions;\nstd::mutex session_mutex;")
text = text.replace("MYSQL* conn; ", "MYSQL* conn; \nstd::mutex db_mutex;")

# 3. Lambda wrap for Threading
start_src = """        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
            perror("accept");
            continue;
        }"""
start_dst = """        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
            perror("accept");
            continue;
        }
        std::thread([new_socket, address]() {
            char buffer[65536] = {0};
            char client_ip[INET6_ADDRSTRLEN];"""
text = text.replace(start_src, start_dst)

# Note: We must REMOVE the existing declarations from main() that we just moved into lambda
decl_src = """    int server_fd, new_socket;
    struct sockaddr_in6 address;
    int opt = 1;
    int addrlen = sizeof(address);
    char buffer[65536] = {0}; // Increased buffer for larger JSON states
    char client_ip[INET6_ADDRSTRLEN];"""
decl_dst = """    int server_fd, new_socket;
    struct sockaddr_in6 address;
    int opt = 1;
    int addrlen = sizeof(address);"""
text = text.replace(decl_src, decl_dst)

end_src = """        close(new_socket);
    }
    return 0;"""
end_dst = """        close(new_socket);
        }).detach();
    }
    return 0;"""
text = text.replace(end_src, end_dst)

# 4. session mutex
text = text.replace("if (sessions.count(sid)) cur_session = sessions[sid];", "{ std::lock_guard<std::mutex> lock(session_mutex); if (sessions.count(sid)) cur_session = sessions[sid]; }")
text = text.replace("sessions[sid] = {uid, username};", "{ std::lock_guard<std::mutex> lock(session_mutex); sessions[sid] = {uid, username}; }")
text = text.replace("sessions.erase(sid);", "{ std::lock_guard<std::mutex> lock(session_mutex); sessions.erase(sid); }")

# 5. db mutex - wrap `mysql_query` and `mysql_store_result` in locks
# A quick way without refactoring too much is just using a macro for the entire query block:
# But we can't lock just around mysql_query, we need the lock to stay ALIVE until mysql_fetch is done!
# So we need to put lock guards at the start of every API endpoint block that hits DB.
# There are not many endpoints: login, signup, boards GET/POST/GET_ID/DELETE.
# I will just replace `if (mysql_query(conn` with `{ std::lock_guard<std::mutex> db_lock(db_mutex); if (mysql_query(conn`
# But wait, `mysql_free_result` needs to be inside the lock! If we just open `{`, we need perfectly matching `}`.
# This might be tricky with simple text replacement.

with open("patch_server.py", "w") as f:
    pass # Wait, EOF closing is used for bash.
