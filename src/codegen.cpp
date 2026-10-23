// codegen.cpp - Phase 9: translate TAC into pseudo-assembly for an imaginary machine.
//
// The imaginary machine has two registers R1, R2 and a memory location for every
// variable and temporary. '#5' means the constant 5; a plain name such as 'x' means
// "the memory location called x".
//
// THIS IS NOT REAL MACHINE CODE and cannot be run on a real CPU.
#include "codegen.h"

#include <set>

namespace {

std::string operand(const std::string& a) { return isNumberText(a) ? "#" + a : a; }

std::string opcode(const std::string& op) {
    if (op == "+") return "ADD";
    if (op == "-") return "SUB";
    if (op == "*") return "MUL";
    if (op == "/") return "DIV";
    if (op == "==") return "CMPEQ";
    if (op == "!=") return "CMPNE";
    if (op == "<") return "CMPLT";
    if (op == ">") return "CMPGT";
    if (op == "<=") return "CMPLE";
    return "CMPGE";   // ">="
}

// opcodes are padded to 6 characters so the columns line up
std::string padOp(const std::string& s) { return s + std::string(s.size() < 6 ? 6 - s.size() : 1, ' '); }

void addName(const std::string& n, std::vector<std::string>& names, std::set<std::string>& seen) {
    if (n.empty() || isNumberText(n)) return;
    if (seen.insert(n).second) names.push_back(n);
}

}  // namespace

std::vector<AsmLine> generateAssembly(const std::vector<Instr>& code) {
    std::vector<AsmLine> out;

    // ---- data section: every variable / temporary that is used
    std::vector<std::string> names;
    std::set<std::string> seen;
    for (size_t i = 0; i < code.size(); i++) {
        const Instr& c = code[i];
        if (c.kind == IrKind::Label || c.kind == IrKind::Goto) continue;
        addName(c.arg1, names, seen);
        addName(c.arg2, names, seen);
        if (c.kind != IrKind::IfFalse && c.kind != IrKind::Print) addName(c.result, names, seen);
    }
    out.push_back(AsmLine{".data", 0});
    for (size_t i = 0; i < names.size(); i++) out.push_back(AsmLine{"    " + names[i], 0});
    out.push_back(AsmLine{".text", 0});

    // ---- text section
    for (size_t i = 0; i < code.size(); i++) {
        const Instr& c = code[i];
        int ln = c.line;
        if (c.kind == IrKind::Label) {
            out.push_back(AsmLine{c.result + ":", ln});
            continue;
        }
        out.push_back(AsmLine{"    ; " + instrToString(c), ln});
        switch (c.kind) {
            case IrKind::Copy:
                out.push_back(AsmLine{"    LOAD  R1, " + operand(c.arg1), ln});
                out.push_back(AsmLine{"    STORE " + c.result + ", R1", ln});
                break;
            case IrKind::Binary:
                out.push_back(AsmLine{"    LOAD  R1, " + operand(c.arg1), ln});
                out.push_back(AsmLine{"    LOAD  R2, " + operand(c.arg2), ln});
                out.push_back(AsmLine{"    " + padOp(opcode(c.op)) + "R1, R2", ln});
                out.push_back(AsmLine{"    STORE " + c.result + ", R1", ln});
                break;
            case IrKind::Neg:
                out.push_back(AsmLine{"    LOAD  R1, " + operand(c.arg1), ln});
                out.push_back(AsmLine{"    NEG   R1", ln});
                out.push_back(AsmLine{"    STORE " + c.result + ", R1", ln});
                break;
            case IrKind::Print:
                out.push_back(AsmLine{"    LOAD  R1, " + operand(c.arg1), ln});
                out.push_back(AsmLine{"    OUT   R1", ln});
                break;
            case IrKind::Input:
                out.push_back(AsmLine{"    IN    R1", ln});
                out.push_back(AsmLine{"    STORE " + c.result + ", R1", ln});
                break;
            case IrKind::Goto:
                out.push_back(AsmLine{"    JMP   " + c.result, ln});
                break;
            case IrKind::IfFalse:
                out.push_back(AsmLine{"    LOAD  R1, " + operand(c.arg1), ln});
                out.push_back(AsmLine{"    JZ    R1, " + c.result, ln});
                break;
            case IrKind::Label:
                break;
        }
    }
    out.push_back(AsmLine{"    HALT", 0});
    return out;
}

std::string asmToText(const std::vector<AsmLine>& lines) {
    std::string s;
    for (size_t i = 0; i < lines.size(); i++) s += lines[i].text + "\n";
    return s;
}
