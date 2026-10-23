// interpreter.cpp - executes three-address code (the "execution check").
#include "interpreter.h"

#include <map>

namespace {
const long long MAX_STEPS = 200000;     // protects against infinite loops
const size_t MAX_OUTPUT = 1000;         // protects against endless output
}

RunResult runIR(const std::vector<Instr>& code, const std::vector<long long>& inputs) {
    RunResult r;
    r.steps = 0;

    // find where every label is
    std::map<std::string, size_t> labels;
    for (size_t i = 0; i < code.size(); i++)
        if (code[i].kind == IrKind::Label) labels[code[i].result] = i;

    std::map<std::string, long long> vars;
    size_t nextInput = 0;
    size_t pc = 0;   // program counter: index of the next instruction

    // value of an operand: a number, or the current value of a variable
    struct Val {
        std::map<std::string, long long>& vars;
        long long operator()(const std::string& a) const {
            if (isNumberText(a)) return std::stoll(a);
            return vars[a];
        }
    } val{vars};

    while (pc < code.size()) {
        if (++r.steps > MAX_STEPS) {
            r.error = "stopped after " + std::to_string(MAX_STEPS) +
                      " steps (the program may contain an infinite loop)";
            return r;
        }
        const Instr& c = code[pc];
        switch (c.kind) {
            case IrKind::Label:
                pc++;
                break;
            case IrKind::Copy:
                vars[c.result] = val(c.arg1);
                pc++;
                break;
            case IrKind::Neg:
                vars[c.result] = -val(c.arg1);
                pc++;
                break;
            case IrKind::Binary: {
                long long v = 0;
                std::string err;
                if (!applyOp(c.op, val(c.arg1), val(c.arg2), v, err)) {
                    r.error = "run-time error on line " + std::to_string(c.line) + ": " + err;
                    return r;
                }
                vars[c.result] = v;
                pc++;
                break;
            }
            case IrKind::Print:
                if (r.output.size() >= MAX_OUTPUT) {
                    r.error = "stopped: more than " + std::to_string(MAX_OUTPUT) + " lines of output";
                    return r;
                }
                r.output.push_back(std::to_string(val(c.arg1)));
                pc++;
                break;
            case IrKind::Input:
                if (nextInput >= inputs.size()) {
                    r.error = "run-time error on line " + std::to_string(c.line) +
                              ": INPUT needs a value but none is left (type values in the 'Input values' box)";
                    return r;
                }
                vars[c.result] = inputs[nextInput++];
                pc++;
                break;
            case IrKind::Goto:
                pc = labels[c.result];
                break;
            case IrKind::IfFalse:
                if (val(c.arg1) == 0) pc = labels[c.result];
                else pc++;
                break;
        }
    }
    return r;
}
