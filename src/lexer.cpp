// lexer.cpp - Phase 1: lexical analysis.
// Reads the source one character at a time and groups characters into tokens.
#include "lexer.h"

#include <cctype>
#include <iomanip>
#include <sstream>

static TokenType keywordOrId(const std::string& w) {
    if (w == "LET") return TokenType::LET;
    if (w == "PRINT") return TokenType::PRINT;
    if (w == "INPUT") return TokenType::INPUT;
    if (w == "IF") return TokenType::IF;
    if (w == "ELSE") return TokenType::ELSE;
    if (w == "WHILE") return TokenType::WHILE;
    return TokenType::ID;
}

LexResult tokenize(const std::string& s) {
    LexResult r;
    int line = 1, col = 1;
    size_t i = 0;

    while (i < s.size()) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        int startCol = col;

        // ---- whitespace and new lines
        if (c == '\n') { line++; col = 1; i++; continue; }
        if (c == ' ' || c == '\t' || c == '\r') { i++; col++; continue; }

        // ---- comment: '#' until end of line
        if (c == '#') {
            while (i < s.size() && s[i] != '\n') { i++; col++; }
            continue;
        }

        // ---- identifier or keyword: letter, then letters/digits/underscore
        if (isalpha(c)) {
            size_t j = i;
            while (j < s.size() && (isalnum(static_cast<unsigned char>(s[j])) || s[j] == '_')) j++;
            std::string word = s.substr(i, j - i);
            r.tokens.push_back(Token{keywordOrId(word), word, line, startCol});
            col += static_cast<int>(j - i);
            i = j;
            continue;
        }

        // ---- number: digits only (we also swallow letters so "12abc" is one error)
        if (isdigit(c)) {
            size_t j = i;
            while (j < s.size() && isalnum(static_cast<unsigned char>(s[j]))) j++;
            std::string word = s.substr(i, j - i);
            bool allDigits = true;
            for (size_t k = 0; k < word.size(); k++)
                if (!isdigit(static_cast<unsigned char>(word[k]))) allDigits = false;

            if (!allDigits) {
                r.errors.push_back(CompileError{"Lexical Analysis", line, startCol,
                    "Malformed number '" + word + "'",
                    "A number may contain digits only. A name must start with a letter, "
                    "so '12abc' is neither a valid number nor a valid name."});
            } else if (word.size() > 18) {
                r.errors.push_back(CompileError{"Lexical Analysis", line, startCol,
                    "Number '" + word + "' is too large",
                    "This compiler stores numbers in 64 bits, so use at most 18 digits."});
            } else {
                r.tokens.push_back(Token{TokenType::NUMBER, word, line, startCol});
            }
            col += static_cast<int>(j - i);
            i = j;
            continue;
        }

        // ---- operators and punctuation
        char next = (i + 1 < s.size()) ? s[i + 1] : '\0';
        TokenType t = TokenType::END_OF_FILE;   // END_OF_FILE here means "no match yet"
        int len = 1;
        switch (c) {
            case '+': t = TokenType::PLUS; break;
            case '-': t = TokenType::MINUS; break;
            case '*': t = TokenType::STAR; break;
            case '/': t = TokenType::SLASH; break;
            case '(': t = TokenType::LPAREN; break;
            case ')': t = TokenType::RPAREN; break;
            case '{': t = TokenType::LBRACE; break;
            case '}': t = TokenType::RBRACE; break;
            case '=':
                if (next == '=') { t = TokenType::EQ; len = 2; } else t = TokenType::ASSIGN;
                break;
            case '!':
                if (next == '=') { t = TokenType::NE; len = 2; }
                break;
            case '<':
                if (next == '=') { t = TokenType::LE; len = 2; } else t = TokenType::LT;
                break;
            case '>':
                if (next == '=') { t = TokenType::GE; len = 2; } else t = TokenType::GT;
                break;
            default: break;
        }

        if (t != TokenType::END_OF_FILE) {
            r.tokens.push_back(Token{t, s.substr(i, len), line, startCol});
            i += len;
            col += len;
            continue;
        }

        // ---- anything else is an invalid character
        int bytes = 1;   // a UTF-8 character such as 'é' may use 2-4 bytes
        if ((c & 0xE0) == 0xC0) bytes = 2;
        else if ((c & 0xF0) == 0xE0) bytes = 3;
        else if ((c & 0xF8) == 0xF0) bytes = 4;
        std::string bad = s.substr(i, bytes);
        std::string hint = "This character is not part of the language. Allowed: letters, digits, "
                           "+ - * / = == != < > <= >= ( ) { } and # for comments.";
        if (c == '!') hint = "'!' is only allowed as part of '!=' (not equal).";
        r.errors.push_back(CompileError{"Lexical Analysis", line, startCol,
                                        "Invalid character '" + bad + "'", hint});
        i += bytes;
        col++;
    }

    r.tokens.push_back(Token{TokenType::END_OF_FILE, "", line, col});
    return r;
}

std::string tokensToText(const std::vector<Token>& tokens) {
    std::ostringstream out;
    out << std::left << std::setw(6) << "LINE" << std::setw(6) << "COL"
        << std::setw(10) << "TOKEN" << "LEXEME\n";
    for (size_t i = 0; i < tokens.size(); i++) {
        const Token& t = tokens[i];
        out << std::left << std::setw(6) << t.line << std::setw(6) << t.col
            << std::setw(10) << tokenTypeName(t.type)
            << (t.type == TokenType::END_OF_FILE ? "(end of file)" : t.text) << "\n";
    }
    return out.str();
}
