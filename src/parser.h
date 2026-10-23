// parser.h - Phases 2-4: syntax analysis, parse tree, AST.
#ifndef PARSER_H
#define PARSER_H

#include <string>
#include <vector>
#include "common.h"

struct ParseResult {
    TreePtr parseTree;                  // every grammar rule + every token
    NodePtr ast;                        // simplified tree (only valid if errors is empty)
    std::vector<CompileError> errors;   // syntax errors
};

ParseResult parseTokens(const std::vector<Token>& tokens);

std::string treeToText(const TreePtr& root);   // draw the parse tree
std::string astToText(const NodePtr& root);    // draw the AST
int countTreeNodes(const TreePtr& root);
int countAstNodes(const NodePtr& root);

#endif
