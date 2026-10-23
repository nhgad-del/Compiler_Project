// interpreter.h - Phase 10: a tiny interpreter that RUNS three-address code.
// Used to prove that optimisation did not change what the program does.
#ifndef INTERPRETER_H
#define INTERPRETER_H

#include <string>
#include <vector>
#include "ir.h"

struct RunResult {
    std::vector<std::string> output;   // one entry per PRINT
    std::string error;                 // empty if the run finished normally
    long long steps;                   // instructions executed
};

RunResult runIR(const std::vector<Instr>& code, const std::vector<long long>& inputs);

#endif
