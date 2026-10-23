// common.cpp - small helper functions used by several phases.
#include "common.h"

#include <cctype>

std::string tokenTypeName(TokenType t) {
    switch (t) {
        case TokenType::LET: return "LET";
        case TokenType::PRINT: return "PRINT";
        case TokenType::INPUT: return "INPUT";
        case TokenType::IF: return "IF";
        case TokenType::ELSE: return "ELSE";
        case TokenType::WHILE: return "WHILE";
        case TokenType::ID: return "ID";
        case TokenType::NUMBER: return "NUMBER";
        case TokenType::PLUS: return "PLUS";
        case TokenType::MINUS: return "MINUS";
        case TokenType::STAR: return "STAR";
        case TokenType::SLASH: return "SLASH";
        case TokenType::ASSIGN: return "ASSIGN";
        case TokenType::EQ: return "EQ";
        case TokenType::NE: return "NE";
        case TokenType::LT: return "LT";
        case TokenType::GT: return "GT";
        case TokenType::LE: return "LE";
        case TokenType::GE: return "GE";
        case TokenType::LPAREN: return "LPAREN";
        case TokenType::RPAREN: return "RPAREN";
        case TokenType::LBRACE: return "LBRACE";
        case TokenType::RBRACE: return "RBRACE";
        case TokenType::END_OF_FILE: return "EOF";
    }
    return "?";
}

bool isNumberText(const std::string& s) {
    if (s.empty()) return false;
    size_t i = (s[0] == '-') ? 1 : 0;
    if (i >= s.size()) return false;
    for (; i < s.size(); i++)
        if (!isdigit(static_cast<unsigned char>(s[i]))) return false;
    return true;
}

bool isTempName(const std::string& s) {
    if (s.size() < 2 || s[0] != 't') return false;
    for (size_t i = 1; i < s.size(); i++)
        if (!isdigit(static_cast<unsigned char>(s[i]))) return false;
    return true;
}

std::string toUpper(const std::string& s) {
    std::string r = s;
    for (size_t i = 0; i < r.size(); i++)
        r[i] = static_cast<char>(toupper(static_cast<unsigned char>(r[i])));
    return r;
}
