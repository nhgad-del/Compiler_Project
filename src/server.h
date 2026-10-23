// server.h - a very small local web server (plain POSIX sockets, no libraries).
#ifndef SERVER_H
#define SERVER_H

#include <string>

// Starts the server on http://127.0.0.1:<port> and never returns (until Ctrl+C).
// 'webDir' is the folder that contains index.html and style.css.
int runServer(int port, const std::string& webDir);

#endif
