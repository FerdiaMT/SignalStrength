// Minimal localhost telemetry collector.
// It accepts POST /observations requests containing JSON and appends each
// validated body as one line in /data/observations.ndjson.

#include <arpa/inet.h>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

const int PORT = 8080;
const int BUFFER_SIZE = 16384;
volatile std::sig_atomic_t stop_requested = 0;

void handle_stop(int) {
    stop_requested = 1;
}

bool has_required_fields(const std::string& body) {
    return body.find("\"device\"") != std::string::npos &&
           body.find("\"location\"") != std::string::npos;
}

void send_response(int client_socket, int status, const std::string& body) {
    const std::string status_text = status == 201 ? "Created" : "Bad Request";
    const std::string response =
        "HTTP/1.1 " + std::to_string(status) + " " + status_text + "\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: " + std::to_string(body.size()) + "\r\n"
        "Connection: close\r\n\r\n" + body;
    send(client_socket, response.c_str(), response.size(), 0);
}

void save_observation(const std::string& body) {
    std::ofstream file("/data/observations.ndjson", std::ios::app);
    if (!file) {
        std::cerr << "Could not open /data/observations.ndjson\n";
        return;
    }
    file << body << '\n';
}

void handle_client(int client_socket) {
    char buffer[BUFFER_SIZE + 1];
    const ssize_t bytes_read = recv(client_socket, buffer, BUFFER_SIZE, 0);
    if (bytes_read <= 0) return;
    buffer[bytes_read] = '\0';

    const std::string request(buffer);
    const std::size_t body_start = request.find("\r\n\r\n");
    const bool is_observation_post =
        request.rfind("POST /observations ", 0) == 0;

    if (!is_observation_post || body_start == std::string::npos) {
        send_response(client_socket, 400, "{\"error\":\"expected POST /observations\"}");
        return;
    }

    const std::string body = request.substr(body_start + 4);
    if (!has_required_fields(body)) {
        send_response(client_socket, 400,
                      "{\"error\":\"JSON requires device and location\"}");
        return;
    }

    save_observation(body);
    send_response(client_socket, 201, "{\"status\":\"stored\"}");
}

int main() {
    std::signal(SIGINT, handle_stop);
    std::signal(SIGTERM, handle_stop);

    const int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        std::cerr << "socket failed: " << std::strerror(errno) << '\n';
        return 1;
    }

    int reuse_address = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &reuse_address,
               sizeof(reuse_address));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(PORT);

    if (bind(server_socket, reinterpret_cast<sockaddr*>(&address),
             sizeof(address)) < 0 ||
        listen(server_socket, 16) < 0) {
        std::cerr << "Could not listen on port " << PORT << ": "
                  << std::strerror(errno) << '\n';
        close(server_socket);
        return 1;
    }

    std::cout << "SignalStrength collector listening on port " << PORT << '\n';
    while (!stop_requested) {
        const int client_socket = accept(server_socket, nullptr, nullptr);
        if (client_socket < 0) {
            if (errno == EINTR) continue;
            std::cerr << "accept failed: " << std::strerror(errno) << '\n';
            continue;
        }
        handle_client(client_socket);
        close(client_socket);
    }

    close(server_socket);
    return 0;
}
