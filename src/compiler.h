// compiler.h - runs all phases in order and prepares the "Learning Trail" report.
#ifndef COMPILER_H
#define COMPILER_H

#include <map>
#include <string>
#include <vector>
#include "common.h"

// Everything the user can see about ONE phase.
struct PhaseReport {
    std::string id;        // "lexer", "syntax", "parsetree", "ast", "semantic",
                           // "symbols", "ir", "opt", "codegen", "run"
    std::string title;     // "1. Lexical Analysis"
    std::string status;    // "ok", "warning", "error" or "skipped"
    std::string explain;   // what this phase does (beginner language)
    std::string output;    // the phase's result as text
    std::string changes;   // how the representation changed from the previous phase
    std::string next;      // what the next phase will do
    std::vector<CompileError> errors;
    std::vector<CompileError> warnings;
};

struct Report {
    bool success;                              // true if no phase reported an error
    std::vector<std::string> sourceLines;
    std::vector<PhaseReport> phases;           // always 10 entries
    std::map<int, std::string> lineTraces;     // source line number -> trace text
};

Report compileSource(const std::string& source, const std::vector<long long>& inputs);

// Turns "5 3 -2" into {5, 3, -2}. Returns false (and sets err) if something is not an integer.
bool parseInputValues(const std::string& text, std::vector<long long>& out, std::string& err);

// Output formats
std::string reportToText(const Report& r, const std::string& onlyPhase);   // "" = all phases
std::string reportToJson(const Report& r);

#endif
