// ir.cpp - Phase 7: generates three-address code from the AST.
#include "ir.h"

#include <climits>
#include <iomanip>
#include <sstream>

namespace {

class IrGen {
public:
    std::vector<Instr> code;

    void genStatement(const NodePtr& n) {
        switch (n->kind) {
            case NodeKind::Program:
            case NodeKind::Block:
                for (size_t i = 0; i < n->kids.size(); i++) genStatement(n->kids[i]);
                break;

            case NodeKind::Let:
            case NodeKind::Assign: {
                std::string v = genExpr(n->kids[0]);
                emit(IrKind::Copy, "", v, "", n->text, n->line);
                break;
            }

            case NodeKind::Print: {
                std::string v = genExpr(n->kids[0]);
                emit(IrKind::Print, "", v, "", "", n->line);
                break;
            }

            case NodeKind::Input:
                emit(IrKind::Input, "", "", "", n->text, n->line);
                break;

            case NodeKind::If: {
                std::string c = genExpr(n->kids[0]);
                std::string elseLabel = newLabel();
                emit(IrKind::IfFalse, "", c, "", elseLabel, n->line);
                genStatement(n->kids[1]);
                if (n->kids.size() == 3) {
                    std::string endLabel = newLabel();
                    emit(IrKind::Goto, "", "", "", endLabel, n->line);
                    emit(IrKind::Label, "", "", "", elseLabel, n->line);
                    genStatement(n->kids[2]);
                    emit(IrKind::Label, "", "", "", endLabel, n->line);
                } else {
                    emit(IrKind::Label, "", "", "", elseLabel, n->line);
                }
                break;
            }

            case NodeKind::While: {
                std::string startLabel = newLabel();
                std::string endLabel = newLabel();
                emit(IrKind::Label, "", "", "", startLabel, n->line);
                std::string c = genExpr(n->kids[0]);
                emit(IrKind::IfFalse, "", c, "", endLabel, n->line);
                genStatement(n->kids[1]);
                emit(IrKind::Goto, "", "", "", startLabel, n->line);
                emit(IrKind::Label, "", "", "", endLabel, n->line);
                break;
            }

            default:
                break;
        }
    }

private:
    int tempCount = 0;
    int labelCount = 0;

    std::string newTemp() { return "t" + std::to_string(++tempCount); }
    std::string newLabel() { return "L" + std::to_string(++labelCount); }

    void emit(IrKind k, const std::string& op, const std::string& a1, const std::string& a2,
              const std::string& res, int line) {
        Instr i;
        i.kind = k; i.op = op; i.arg1 = a1; i.arg2 = a2; i.result = res; i.line = line;
        code.push_back(i);
    }

    // Returns the NAME or CONSTANT that holds the value of the expression.
    std::string genExpr(const NodePtr& n) {
        switch (n->kind) {
            case NodeKind::Num: return std::to_string(n->value);
            case NodeKind::Var: return n->text;
            case NodeKind::Neg: {
                std::string a = genExpr(n->kids[0]);
                std::string t = newTemp();
                emit(IrKind::Neg, "", a, "", t, n->line);
                return t;
            }
            case NodeKind::BinOp: {
                std::string a = genExpr(n->kids[0]);
                std::string b = genExpr(n->kids[1]);
                std::string t = newTemp();
                emit(IrKind::Binary, n->text, a, b, t, n->line);
                return t;
            }
            default: return "?";
        }
    }
};

}  // namespace

std::vector<Instr> generateIR(const NodePtr& ast) {
    IrGen g;
    g.genStatement(ast);
    return g.code;
}

std::string instrToString(const Instr& i) {
    switch (i.kind) {
        case IrKind::Copy:    return i.result + " = " + i.arg1;
        case IrKind::Binary:  return i.result + " = " + i.arg1 + " " + i.op + " " + i.arg2;
        case IrKind::Neg:     return i.result + " = neg " + i.arg1;
        case IrKind::Print:   return "print " + i.arg1;
        case IrKind::Input:   return "input " + i.result;
        case IrKind::Label:   return i.result + ":";
        case IrKind::Goto:    return "goto " + i.result;
        case IrKind::IfFalse: return "ifFalse " + i.arg1 + " goto " + i.result;
    }
    return "?";
}

std::string irToText(const std::vector<Instr>& code) {
    std::ostringstream out;
    if (code.empty()) return "(no instructions)\n";
    for (size_t i = 0; i < code.size(); i++) {
        std::string text = instrToString(code[i]);
        if (code[i].kind != IrKind::Label) text = "    " + text;
        std::ostringstream left;
        left << std::right << std::setw(3) << (i + 1) << "  " << text;
        std::string l = left.str();
        size_t pad = l.size() < 36 ? 36 - l.size() : 2;
        out << l << std::string(pad, ' ') << "; line " << code[i].line << "\n";
    }
    return out.str();
}

bool applyOp(const std::string& op, long long a, long long b, long long& out, std::string& err) {
    if (op == "+") {
        if (__builtin_add_overflow(a, b, &out)) { err = "arithmetic overflow"; return false; }
        return true;
    }
    if (op == "-") {
        if (__builtin_sub_overflow(a, b, &out)) { err = "arithmetic overflow"; return false; }
        return true;
    }
    if (op == "*") {
        if (__builtin_mul_overflow(a, b, &out)) { err = "arithmetic overflow"; return false; }
        return true;
    }
    if (op == "/") {
        if (b == 0) { err = "division by zero"; return false; }
        if (a == LLONG_MIN && b == -1) { err = "arithmetic overflow"; return false; }
        out = a / b;
        return true;
    }
    if (op == "==") { out = (a == b); return true; }
    if (op == "!=") { out = (a != b); return true; }
    if (op == "<")  { out = (a < b);  return true; }
    if (op == ">")  { out = (a > b);  return true; }
    if (op == "<=") { out = (a <= b); return true; }
    if (op == ">=") { out = (a >= b); return true; }
    err = "unknown operator " + op;
    return false;
}
