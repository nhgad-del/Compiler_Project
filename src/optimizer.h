// optimizer.h - Phase 8: basic optimisation of the three-address code.
#ifndef OPTIMIZER_H
#define OPTIMIZER_H

#include <string>
#include <vector>
#include "ir.h"

struct OptResult {
    std::vector<Instr> code;        // the optimised instructions
    std::vector<std::string> log;   // human-readable list of what was changed
};

OptResult optimize(const std::vector<Instr>& input);

#endif
