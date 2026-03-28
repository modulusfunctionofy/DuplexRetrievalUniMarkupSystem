#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <sstream>

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    char buffer[8192] = {0};

    // Create socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Forcefully attaching socket to the port 80
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET6;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(80);

    // Bind the socket to the network address and port
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0
    ) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    // simple routing: serve static files and handle tiny API
    while (true) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
            perror("accept");
            exit(EXIT_FAILURE);
        }
        read(new_socket, buffer, 4096);
        std::cout << "Received request: " << buffer << std::endl;

        // parse request line
        std::istringstream reqstream(buffer);
        std::string method, path, version;
        reqstream >> method >> path >> version;

        if (method == "GET") {
            std::string file_path = (path == "/") ? "index.html" : path.substr(1);
            std::ifstream file(file_path, std::ios::binary);
            if (file) {
                std::string content((std::istreambuf_iterator<char>(file)), {});
                std::string content_type = "text/plain";
                if (file_path.find(".html") != std::string::npos) content_type = "text/html";
                else if (file_path.find(".js") != std::string::npos) content_type = "application/javascript";
                else if (file_path.find(".css") != std::string::npos) content_type = "text/css";
                else if (file_path.find(".png") != std::string::npos) content_type = "image/png";
                std::string response = "HTTP/1.1 200 OK\r\nContent-Type: " + content_type + "\r\nContent-Length: " + std::to_string(content.size()) + "\r\n\r\n" + content;
                send(new_socket, response.c_str(), response.size(), 0);
            } else {
                std::string notfound = "HTTP/1.1 404 Not Found\r\n\r\n";
                send(new_socket, notfound.c_str(), notfound.size(), 0);
            }
        } else if (method == "POST" && path == "/api/login") {
            // very naive body extraction: find double CRLF then everything after
            std::string reqbuffer(buffer);
            auto pos = reqbuffer.find("\r\n\r\n");
            std::string body = (pos != std::string::npos) ? reqbuffer.substr(pos + 4) : "";
            // you could parse form data here, for now just send success
            std::string resp = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: 17\r\n\r\n{\"status\":\"ok\"}";
            send(new_socket, resp.c_str(), resp.size(), 0);
        } else {
            std::string bad = "HTTP/1.1 400 Bad Request\r\n\r\n";
            send(new_socket, bad.c_str(), bad.size(), 0);
        }

        close(new_socket);
    }
    return 0;
}