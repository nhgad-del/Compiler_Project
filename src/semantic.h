// semantic.h - Phases 5-6: semantic analysis and the symbol table.
#ifndef SEMANTIC_H
#define SEMANTIC_H

#include <string>
#include <vector>
#include "common.h"

struct Symbol {
    std::string name;
    std::string type;    // always "int" in this language
    int declLine;
    int reads;           // how many times the variable's value is used
    int writes;          // how many times it is assigned / read from INPUT
};

struct SemanticResult {
    std::vector<Symbol> symbols;          // the symbol table (in declaration order)
    std::vector<CompileError> errors;
    std::vector<CompileError> warnings;   // e.g. variable never read
};

SemanticResult analyze(const NodePtr& ast);
std::string symbolsToText(const std::vector<Symbol>& symbols);

#endif
