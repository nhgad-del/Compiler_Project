// optimizer.cpp - Phase 8: basic optimisations on three-address code.
//
//  Pass 1  Constant folding + constant propagation (one scan, top to bottom)
//            2 * 3        -> 6           (folding: calculate at compile time)
//            x = 5 ... x  -> ... 5       (propagation: replace a variable by its known value)
//          The table of "known" values is thrown away at every label, because a label can
//          be reached from several places (for example the top of a loop), so we cannot be
//          sure which value the variable holds there.
//  Pass 2  Remove temporaries (t1, t2, ...) whose value is never used.
//  Pass 3  Merge "t1 = a + b ; x = t1" into "x = a + b".
#include "optimizer.h"

#include <cctype>
#include <map>

namespace {

bool isIdent(const std::string& s) { return !s.empty() && isalpha(static_cast<unsigned char>(s[0])); }

std::string at(int line) { return "Line " + std::to_string(line) + ": "; }

std::map<std::string, int> countUses(const std::vector<Instr>& code) {
    std::map<std::string, int> uses;
    for (size_t i = 0; i < code.size(); i++) {
        const Instr& c = code[i];
        switch (c.kind) {
            case IrKind::Binary: uses[c.arg1]++; uses[c.arg2]++; break;
            case IrKind::Copy:
            case IrKind::Neg:
            case IrKind::Print:
            case IrKind::IfFalse: uses[c.arg1]++; break;
            default: break;
        }
    }
    return uses;
}

bool definesValue(const Instr& c) {
    return c.kind == IrKind::Copy || c.kind == IrKind::Binary || c.kind == IrKind::Neg;
}

}  // namespace

OptResult optimize(const std::vector<Instr>& input) {
    OptResult res;

    // ------------------------------------------------ pass 1: folding + propagation
    std::vector<Instr> code;
    std::map<std::string, long long> known;   // name -> constant value it currently holds

    for (size_t idx = 0; idx < input.size(); idx++) {
        Instr ins = input[idx];

        // replace a variable by its known constant value
        struct Sub {
            std::map<std::string, long long>& known;
            std::vector<std::string>& log;
            int line;
            void operator()(std::string& a) const {
                if (!isIdent(a)) return;
                std::map<std::string, long long>::const_iterator it = known.find(a);
                if (it == known.end()) return;
                log.push_back(at(line) + "constant propagation: '" + a + "' replaced by " +
                              std::to_string(it->second));
                a = std::to_string(it->second);
            }
        } sub{known, res.log, ins.line};

        switch (ins.kind) {
            case IrKind::Label:
                known.clear();
                code.push_back(ins);
                break;

            case IrKind::Copy:
                sub(ins.arg1);
                if (isNumberText(ins.arg1)) known[ins.result] = std::stoll(ins.arg1);
                else known.erase(ins.result);
                code.push_back(ins);
                break;

            case IrKind::Binary: {
                sub(ins.arg1);
                sub(ins.arg2);
                if (isNumberText(ins.arg1) && isNumberText(ins.arg2)) {
                    long long v = 0;
                    std::string err;
                    if (applyOp(ins.op, std::stoll(ins.arg1), std::stoll(ins.arg2), v, err)) {
                        res.log.push_back(at(ins.line) + "constant folding: " + ins.arg1 + " " +
                                          ins.op + " " + ins.arg2 + " = " + std::to_string(v));
                        ins.kind = IrKind::Copy;
                        ins.op = "";
                        ins.arg1 = std::to_string(v);
                        ins.arg2 = "";
                        known[ins.result] = v;
                        code.push_back(ins);
                        break;
                    }
                    // not foldable (e.g. division by zero): keep it, it fails at run time
                }
                known.erase(ins.result);
                code.push_back(ins);
                break;
            }

            case IrKind::Neg:
                sub(ins.arg1);
                if (isNumberText(ins.arg1)) {
                    long long v = -std::stoll(ins.arg1);
                    res.log.push_back(at(ins.line) + "constant folding: neg " + ins.arg1 + " = " +
                                      std::to_string(v));
                    ins.kind = IrKind::Copy;
                    ins.arg1 = std::to_string(v);
                    known[ins.result] = v;
                } else {
                    known.erase(ins.result);
                }
                code.push_back(ins);
                break;

            case IrKind::Print:
                sub(ins.arg1);
                code.push_back(ins);
                break;

            case IrKind::Input:
                known.erase(ins.result);
                code.push_back(ins);
                break;

            case IrKind::IfFalse:
                sub(ins.arg1);
                if (isNumberText(ins.arg1)) {
                    if (std::stoll(ins.arg1) == 0) {
                        res.log.push_back(at(ins.line) + "condition is always false: ifFalse becomes an unconditional goto");
                        ins.kind = IrKind::Goto;
                        ins.arg1 = "";
                        code.push_back(ins);
                    } else {
                        res.log.push_back(at(ins.line) + "condition is always true: ifFalse jump removed");
                    }
                } else {
                    code.push_back(ins);
                }
                break;

            case IrKind::Goto:
                code.push_back(ins);
                break;
        }
    }

    // ------------------------------------------------ pass 2: unused temporaries
    bool changed = true;
    while (changed) {
        changed = false;
        std::map<std::string, int> uses = countUses(code);
        for (size_t i = 0; i < code.size(); i++) {
            if (definesValue(code[i]) && isTempName(code[i].result) && uses[code[i].result] == 0) {
                res.log.push_back(at(code[i].line) + "removed unused temporary " + code[i].result +
                                  "  (" + instrToString(code[i]) + ")");
                code.erase(code.begin() + static_cast<long>(i));
                changed = true;
                break;
            }
        }
    }

    // ------------------------------------------------ pass 3: merge "t = ... ; x = t"
    std::map<std::string, int> uses = countUses(code);
    for (size_t i = 0; i + 1 < code.size();) {
        Instr& a = code[i];
        Instr& b = code[i + 1];
        if (definesValue(a) && isTempName(a.result) && b.kind == IrKind::Copy &&
            b.arg1 == a.result && uses[a.result] == 1) {
            res.log.push_back(at(a.line) + "merged temporary " + a.result + " into '" + b.result + "'");
            a.result = b.result;
            code.erase(code.begin() + static_cast<long>(i) + 1);
        } else {
            i++;
        }
    }

    res.code = code;
    return res;
}
