// semantic.cpp - Phases 5-6: semantic analysis + symbol table.
// Walks the AST once, top to bottom, in program order. There is one global scope.
#include "semantic.h"

#include <iomanip>
#include <sstream>

namespace {

class Analyzer {
public:
    SemanticResult run(const NodePtr& ast) {
        checkStatement(ast);   // the Program node: checks every statement inside it
        for (size_t i = 0; i < res.symbols.size(); i++) {
            const Symbol& s = res.symbols[i];
            if (s.reads == 0)
                res.warnings.push_back(CompileError{"Semantic Analysis", s.declLine, 0,
                    "Variable '" + s.name + "' is declared but its value is never used",
                    "This is only a warning. Remove the variable or use it (for example PRINT it)."});
        }
        return res;
    }

private:
    SemanticResult res;

    Symbol* find(const std::string& name) {
        for (size_t i = 0; i < res.symbols.size(); i++)
            if (res.symbols[i].name == name) return &res.symbols[i];
        return NULL;
    }

    void error(int line, const std::string& msg, const std::string& hint) {
        res.errors.push_back(CompileError{"Semantic Analysis", line, 0, msg, hint});
    }

    void checkExpr(const NodePtr& n) {
        switch (n->kind) {
            case NodeKind::Num:
                break;
            case NodeKind::Var: {
                Symbol* s = find(n->text);
                if (!s) error(n->line, "Variable '" + n->text + "' is used before it is declared",
                              "Declare it first, for example:  LET " + n->text + " = 0");
                else s->reads++;
                break;
            }
            case NodeKind::Neg:
                checkExpr(n->kids[0]);
                break;
            case NodeKind::BinOp:
                checkExpr(n->kids[0]);
                checkExpr(n->kids[1]);
                if (n->text == "/" && n->kids[1]->kind == NodeKind::Num && n->kids[1]->value == 0)
                    error(n->line, "Division by zero",
                          "The right side of '/' is the constant 0. Division by zero is undefined.");
                break;
            default:
                break;
        }
    }

    void checkStatement(const NodePtr& n) {
        switch (n->kind) {
            case NodeKind::Program:
            case NodeKind::Block:
                for (size_t i = 0; i < n->kids.size(); i++) checkStatement(n->kids[i]);
                break;

            case NodeKind::Let: {
                checkExpr(n->kids[0]);   // the value is checked BEFORE the name is declared
                if (find(n->text)) {
                    error(n->line, "Variable '" + n->text + "' is already declared",
                          "LET declares a variable only once. To change its value write:  " +
                          n->text + " = ...");
                } else if (isTempName(n->text)) {
                    error(n->line, "The name '" + n->text + "' is reserved",
                          "Names like t1, t2, t3 are used by the compiler for temporary values. Choose another name.");
                } else {
                    res.symbols.push_back(Symbol{n->text, "int", n->line, 0, 1});
                }
                break;
            }

            case NodeKind::Assign: {
                checkExpr(n->kids[0]);
                Symbol* s = find(n->text);
                if (!s) error(n->line, "Assignment to undeclared variable '" + n->text + "'",
                              "Declare it first with:  LET " + n->text + " = ...");
                else s->writes++;
                break;
            }

            case NodeKind::Input: {
                Symbol* s = find(n->text);
                if (!s) error(n->line, "INPUT into undeclared variable '" + n->text + "'",
                              "Declare it first with:  LET " + n->text + " = 0");
                else s->writes++;
                break;
            }

            case NodeKind::Print:
                checkExpr(n->kids[0]);
                break;

            case NodeKind::If:
            case NodeKind::While:
                checkExpr(n->kids[0]);
                for (size_t i = 1; i < n->kids.size(); i++) checkStatement(n->kids[i]);
                break;

            default:
                break;
        }
    }
};

}  // namespace

SemanticResult analyze(const NodePtr& ast) {
    Analyzer a;
    return a.run(ast);
}

std::string symbolsToText(const std::vector<Symbol>& symbols) {
    std::ostringstream out;
    out << std::left << std::setw(14) << "NAME" << std::setw(8) << "TYPE"
        << std::setw(10) << "DECL LINE" << std::setw(8) << "READS" << "WRITES\n";
    if (symbols.empty()) out << "(no variables declared)\n";
    for (size_t i = 0; i < symbols.size(); i++) {
        const Symbol& s = symbols[i];
        out << std::left << std::setw(14) << s.name << std::setw(8) << s.type
            << std::setw(10) << s.declLine << std::setw(8) << s.reads << s.writes << "\n";
    }
    return out.str();
}
