#include "collector_client.h"

#include <cstring>
#include <netdb.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

namespace {

std::string json_escape(const std::string& value) {
  std::string escaped;
  for (char character : value) {
    if (character == '\\' || character == '"') escaped += '\\';
    escaped += character;
  }
  return escaped;
}

bool parse_collector_url(const std::string& url, std::string* host,
                         std::string* port) {
  const std::string prefix = "http://";
  if (url.rfind(prefix, 0) != 0) return false;

  const std::string address = url.substr(prefix.size());
  const std::size_t colon = address.rfind(':');
  if (colon == std::string::npos || colon == 0 ||
      colon == address.size() - 1) {
    return false;
  }
  *host = address.substr(0, colon);
  *port = address.substr(colon + 1);
  return true;
}

std::string device_name() {
  char hostname[256]{};
  if (gethostname(hostname, sizeof(hostname) - 1) != 0) {
    return "unknown-device";
  }
  return hostname;
}

}  // namespace

bool send_observation_to_collector(const std::string& collector_url,
                                   const std::string& location,
                                   const std::string& target,
                                   int quality_score, double latency_ms,
                                   double loss_percent, double jitter_ms,
                                   bool dns_ok) {
  std::string host;
  std::string port;
  if (!parse_collector_url(collector_url, &host, &port)) return false;

  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  addrinfo* addresses = nullptr;
  if (getaddrinfo(host.c_str(), port.c_str(), &hints, &addresses) != 0) {
    return false;
  }

  int socket_fd = -1;
  for (addrinfo* address = addresses; address != nullptr;
       address = address->ai_next) {
    socket_fd = socket(address->ai_family, address->ai_socktype,
                       address->ai_protocol);
    if (socket_fd < 0) continue;
    if (connect(socket_fd, address->ai_addr, address->ai_addrlen) == 0) break;
    close(socket_fd);
    socket_fd = -1;
  }
  freeaddrinfo(addresses);
  if (socket_fd < 0) return false;

  const std::string body =
      "{\"device\":\"" + json_escape(device_name()) +
      "\",\"location\":\"" + json_escape(location) +
      "\",\"target\":\"" + json_escape(target) +
      "\",\"quality_score\":" + std::to_string(quality_score) +
      ",\"latency_ms\":" + std::to_string(latency_ms) +
      ",\"loss_percent\":" + std::to_string(loss_percent) +
      ",\"jitter_ms\":" + std::to_string(jitter_ms) +
      ",\"dns_ok\":" + (dns_ok ? "true" : "false") + "}";
  const std::string request =
      "POST /observations HTTP/1.1\r\n"
      "Host: " + host + "\r\n"
      "Content-Type: application/json\r\n"
      "Content-Length: " + std::to_string(body.size()) + "\r\n"
      "Connection: close\r\n\r\n" + body;

  const ssize_t sent = send(socket_fd, request.c_str(), request.size(), 0);
  char response[128]{};
  const ssize_t received = recv(socket_fd, response, sizeof(response) - 1, 0);
  close(socket_fd);
  return sent == static_cast<ssize_t>(request.size()) && received > 0 &&
         std::strncmp(response, "HTTP/1.1 201", 12) == 0;
}
