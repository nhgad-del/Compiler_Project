// main.cpp - entry point.
//
//   ./explorer program.txt                 compile a file, print all 10 phases
//   ./explorer program.txt --phase ast     print only one phase (id or number 1-10)
//   ./explorer program.txt --input "5 3"   values for INPUT statements
//   ./explorer program.txt --trace 3       trace source line 3 through the phases
//   ./explorer --serve [port]              start the web interface (default port 8080)
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>

#include "compiler.h"
#include "server.h"

static void usage() {
    std::cout <<
        "Interactive Compiler Explorer\n\n"
        "Usage:\n"
        "  ./explorer <file>                  compile a file and show every phase\n"
        "  ./explorer <file> --phase <id>     show one phase only\n"
        "                                     ids: lexer syntax parsetree ast semantic symbols ir opt codegen run\n"
        "                                     (or the numbers 1-10)\n"
        "  ./explorer <file> --input \"5 3\"    values used by INPUT statements\n"
        "  ./explorer <file> --trace <n>      follow source line n through the phases\n"
        "  ./explorer --serve [port]          web interface at http://localhost:<port> (default 8080)\n"
        "  ./explorer --help                  this message\n\n"
        "Use '-' as the file name to read the program from standard input.\n";
}

int main(int argc, char** argv) {
    std::string file, phase, inputText;
    int trace = 0;
    bool serve = false;
    int port = 8080;
    std::string webDir = "web";

    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--help" || a == "-h") { usage(); return 0; }
        else if (a == "--serve") {
            serve = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') port = std::atoi(argv[++i]);
        }
        else if (a == "--phase" && i + 1 < argc) phase = argv[++i];
        else if (a == "--input" && i + 1 < argc) inputText = argv[++i];
        else if (a == "--trace" && i + 1 < argc) trace = std::atoi(argv[++i]);
        else if (a == "--web-dir" && i + 1 < argc) webDir = argv[++i];
        else if (a == "-" || a[0] != '-') file = a;
        else { std::cerr << "Unknown option: " << a << "\n\n"; usage(); return 2; }
    }

    if (serve) return runServer(port, webDir);
    if (file.empty()) { usage(); return 2; }

    // read the source program
    std::ostringstream ss;
    if (file == "-") {
        ss << std::cin.rdbuf();
    } else {
        std::ifstream in(file.c_str(), std::ios::binary);
        if (!in) { std::cerr << "Cannot open file: " << file << "\n"; return 2; }
        ss << in.rdbuf();
    }

    std::vector<long long> inputs;
    std::string err;
    if (!parseInputValues(inputText, inputs, err)) { std::cerr << err << "\n"; return 2; }

    Report r = compileSource(ss.str(), inputs);

    if (trace > 0) {
        if (r.lineTraces.count(trace) == 0) {
            std::cerr << "The program has no line " << trace << ".\n";
            return 2;
        }
        std::cout << r.lineTraces[trace];
        return r.success ? 0 : 1;
    }

    std::cout << reportToText(r, phase);
    return r.success ? 0 : 1;
}
