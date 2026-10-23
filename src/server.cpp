// server.cpp - tiny HTTP server so the compiler can be used from a web browser.
//
// Routes:
//   GET  /            -> web/index.html
//   GET  /style.css   -> web/style.css
//   POST /compile     -> body: first line = input values, remaining lines = source code
//                        reply: JSON describing every compiler phase
//
// It listens on 127.0.0.1 only, so other computers cannot connect to it.
#include "server.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <csignal>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>

#include "compiler.h"

namespace {

const size_t MAX_REQUEST = 1024 * 1024;   // 1 MB

bool readFile(const std::string& path, std::string& out) {
    std::ifstream f(path.c_str(), std::ios::binary);
    if (!f) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}

void sendAll(int fd, const std::string& data) {
    size_t sent = 0;
    while (sent < data.size()) {
        ssize_t n = send(fd, data.data() + sent, data.size() - sent, 0);
        if (n <= 0) return;
        sent += static_cast<size_t>(n);
    }
}

void sendResponse(int fd, int code, const std::string& statusText, const std::string& type,
                  const std::string& body) {
    std::ostringstream h;
    h << "HTTP/1.1 " << code << " " << statusText << "\r\n"
      << "Content-Type: " << type << "\r\n"
      << "Content-Length: " << body.size() << "\r\n"
      << "Cache-Control: no-store\r\n"
      << "Connection: close\r\n\r\n";
    sendAll(fd, h.str() + body);
}

std::string jsonError(const std::string& msg) {
    std::string s = "{\"error\":\"";
    for (size_t i = 0; i < msg.size(); i++) {
        if (msg[i] == '"' || msg[i] == '\\') s += '\\';
        s += msg[i];
    }
    return s + "\"}";
}

void handleClient(int fd, std::string webDir) {
    // a browser may open a connection and send nothing: do not wait forever
    struct timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);

    // ---- read the request headers
    std::string req;
    char buf[4096];
    size_t headerEnd = std::string::npos;
    while (headerEnd == std::string::npos && req.size() < MAX_REQUEST) {
        ssize_t n = recv(fd, buf, sizeof buf, 0);
        if (n <= 0) break;
        req.append(buf, static_cast<size_t>(n));
        headerEnd = req.find("\r\n\r\n");
    }
    if (headerEnd == std::string::npos) { close(fd); return; }

    std::string head = req.substr(0, headerEnd);
    std::istringstream line1(head);
    std::string method, path, version;
    line1 >> method >> path >> version;
    size_t q = path.find('?');
    if (q != std::string::npos) path = path.substr(0, q);

    // ---- read the body (if any)
    std::string lower = head;
    for (size_t i = 0; i < lower.size(); i++) lower[i] = static_cast<char>(tolower(static_cast<unsigned char>(lower[i])));
    size_t contentLength = 0;
    size_t cl = lower.find("content-length:");
    if (cl != std::string::npos) contentLength = std::strtoul(head.c_str() + cl + 15, NULL, 10);
    if (contentLength > MAX_REQUEST) {
        sendResponse(fd, 413, "Payload Too Large", "text/plain", "Program too large");
        close(fd);
        return;
    }
    std::string body = req.substr(headerEnd + 4);
    while (body.size() < contentLength) {
        ssize_t n = recv(fd, buf, sizeof buf, 0);
        if (n <= 0) break;
        body.append(buf, static_cast<size_t>(n));
    }

    // ---- routes
    std::string content;
    if (method == "GET" && (path == "/" || path == "/index.html")) {
        if (readFile(webDir + "/index.html", content))
            sendResponse(fd, 200, "OK", "text/html; charset=utf-8", content);
        else
            sendResponse(fd, 404, "Not Found", "text/plain",
                         "web/index.html not found. Start the server from the project folder.");
    } else if (method == "GET" && path == "/style.css") {
        if (readFile(webDir + "/style.css", content))
            sendResponse(fd, 200, "OK", "text/css; charset=utf-8", content);
        else
            sendResponse(fd, 404, "Not Found", "text/plain", "style.css not found");
    } else if (method == "POST" && path == "/compile") {
        size_t nl = body.find('\n');
        std::string inputLine = (nl == std::string::npos) ? body : body.substr(0, nl);
        std::string source = (nl == std::string::npos) ? "" : body.substr(nl + 1);
        std::vector<long long> inputs;
        std::string err;
        if (!parseInputValues(inputLine, inputs, err)) {
            sendResponse(fd, 400, "Bad Request", "application/json; charset=utf-8", jsonError(err));
        } else {
            Report r = compileSource(source, inputs);
            sendResponse(fd, 200, "OK", "application/json; charset=utf-8", reportToJson(r));
        }
    } else {
        sendResponse(fd, 404, "Not Found", "text/plain", "Not found");
    }
    close(fd);
}

}  // namespace

int runServer(int port, const std::string& webDir) {
    signal(SIGPIPE, SIG_IGN);   // do not crash if the browser closes a connection early

    int listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener < 0) { std::cerr << "Could not create socket\n"; return 1; }

    int yes = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);   // 127.0.0.1 only
    addr.sin_port = htons(static_cast<unsigned short>(port));
    for (size_t i = 0; i < sizeof addr.sin_zero; i++) addr.sin_zero[i] = 0;

    if (bind(listener, reinterpret_cast<sockaddr*>(&addr), sizeof addr) < 0) {
        std::cerr << "Could not use port " << port << " (is another program using it?). "
                  << "Try:  ./explorer --serve 8081\n";
        close(listener);
        return 1;
    }
    if (listen(listener, 16) < 0) { std::cerr << "listen() failed\n"; close(listener); return 1; }

    std::cout << "Interactive Compiler Explorer is running.\n"
              << "Open  http://localhost:" << port << "  in your browser.   (Press Ctrl+C to stop)\n"
              << std::flush;

    while (true) {
        int client = accept(listener, NULL, NULL);
        if (client < 0) continue;
        std::thread(handleClient, client, webDir).detach();   // one small thread per request
    }
    return 0;
}
