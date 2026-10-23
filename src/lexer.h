// lexer.h - Phase 1: lexical analysis (source text -> tokens).
#ifndef LEXER_H
#define LEXER_H

#include <string>
#include <vector>
#include "common.h"

struct LexResult {
    std::vector<Token> tokens;          // always ends with an END_OF_FILE token
    std::vector<CompileError> errors;   // lexical errors (invalid characters etc.)
};

LexResult tokenize(const std::string& source);
std::string tokensToText(const std::vector<Token>& tokens);   // table for display

#endif
