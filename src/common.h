// common.h - data structures shared by all compiler phases.
#ifndef COMMON_H
#define COMMON_H

#include <memory>
#include <string>
#include <vector>

// ---------------------------------------------------------------- Tokens
enum class TokenType {
    LET, PRINT, INPUT, IF, ELSE, WHILE,            // keywords
    ID, NUMBER,                                    // names and numbers
    PLUS, MINUS, STAR, SLASH,                      // + - * /
    ASSIGN, EQ, NE, LT, GT, LE, GE,                // = == != < > <= >=
    LPAREN, RPAREN, LBRACE, RBRACE,                // ( ) { }
    END_OF_FILE
};

struct Token {
    TokenType type;
    std::string text;   // the exact characters from the source
    int line;           // 1-based line number
    int col;            // 1-based column number
};

std::string tokenTypeName(TokenType t);

// ---------------------------------------------------------------- Errors
// One problem found by some phase. 'hint' is a beginner-friendly explanation.
struct CompileError {
    std::string phase;     // e.g. "Lexical Analysis"
    int line;
    int col;
    std::string message;
    std::string hint;
};

// ---------------------------------------------------------------- Parse tree
// The parse tree records every grammar rule (label) and every token (leaf).
struct TreeNode {
    std::string label;
    std::vector<std::shared_ptr<TreeNode>> kids;
};
typedef std::shared_ptr<TreeNode> TreePtr;

// ---------------------------------------------------------------- AST
enum class NodeKind {
    Program, Let, Assign, Print, Input, If, While, Block,   // statements
    BinOp, Neg, Num, Var                                    // expressions
};

// One struct is used for every kind of AST node:
//   Let/Assign : text = variable name, kids[0] = value expression
//   Print      : kids[0] = expression
//   Input      : text = variable name
//   If         : kids[0] = condition, kids[1] = then-block, kids[2] = else-block (optional)
//   While      : kids[0] = condition, kids[1] = body block
//   Block/Program : kids = statements
//   BinOp      : text = operator, kids[0] = left, kids[1] = right
//   Neg        : kids[0] = operand
//   Num        : value        Var : text = name
struct Node {
    NodeKind kind;
    std::string text;
    long long value;
    int line;
    std::vector<std::shared_ptr<Node>> kids;
};
typedef std::shared_ptr<Node> NodePtr;

// ---------------------------------------------------------------- Helpers
bool isNumberText(const std::string& s);   // "12" or "-5"
bool isTempName(const std::string& s);     // "t1", "t23" (compiler temporaries)
std::string toUpper(const std::string& s);

#endif
