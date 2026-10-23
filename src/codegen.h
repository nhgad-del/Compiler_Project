// codegen.h - Phase 9: target code generation (illustrative pseudo-assembly).
#ifndef CODEGEN_H
#define CODEGEN_H

#include <string>
#include <vector>
#include "ir.h"

struct AsmLine {
    std::string text;
    int line;      // source line this assembly line came from (0 = none)
};

std::vector<AsmLine> generateAssembly(const std::vector<Instr>& code);
std::string asmToText(const std::vector<AsmLine>& lines);

#endif
