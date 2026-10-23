// ir.h - Phase 7: intermediate code (three-address code, "TAC").
#ifndef IR_H
#define IR_H

#include <string>
#include <vector>
#include "common.h"

enum class IrKind { Copy, Binary, Neg, Print, Input, Label, Goto, IfFalse };

// One TAC instruction. Which fields are used depends on 'kind':
//   Copy    : result = arg1
//   Binary  : result = arg1 op arg2        (op is + - * / == != < > <= >=)
//   Neg     : result = neg arg1
//   Print   : print arg1
//   Input   : input result
//   Label   : result:
//   Goto    : goto result
//   IfFalse : ifFalse arg1 goto result
struct Instr {
    IrKind kind;
    std::string op;
    std::string arg1;
    std::string arg2;
    std::string result;
    int line;           // source line this instruction came from
};

std::vector<Instr> generateIR(const NodePtr& ast);
std::string instrToString(const Instr& ins);                // one instruction as text
std::string irToText(const std::vector<Instr>& code);       // numbered listing with line refs

// Shared arithmetic used by the optimiser (compile time) and interpreter (run time).
// Returns false and sets 'err' for division by zero or overflow.
bool applyOp(const std::string& op, long long a, long long b, long long& out, std::string& err);

#endif
