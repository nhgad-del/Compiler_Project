// parser.cpp - Phases 2-4: recursive-descent parser.
//
// One function per grammar rule. While parsing, each function
//   (a) adds nodes to the PARSE TREE (grammar rules + tokens), and
//   (b) returns a small AST node (operations and values only).
//
// Grammar (EBNF):
//   program   -> statement*
//   statement -> letStmt | assignStmt | printStmt | inputStmt | ifStmt | whileStmt
//   letStmt   -> "LET" ID "=" expr
//   assignStmt-> ID "=" expr
//   printStmt -> "PRINT" expr
//   inputStmt -> "INPUT" ID
//   ifStmt    -> "IF" condition block [ "ELSE" block ]
//   whileStmt -> "WHILE" condition block
//   block     -> "{" statement* "}"
//   condition -> expr relop expr
//   relop     -> "==" | "!=" | "<" | ">" | "<=" | ">="
//   expr      -> term ( ("+" | "-") term )*
//   term      -> factor ( ("*" | "/") factor )*
//   factor    -> NUMBER | ID | "(" expr ")" | "-" factor
#include "parser.h"

#include <sstream>

namespace {

struct Abort {};   // thrown after a syntax error has been recorded

class Parser {
public:
    explicit Parser(const std::vector<Token>& t) : toks(t), pos(0) {}

    ParseResult run() {
        TreePtr root = std::make_shared<TreeNode>();
        root->label = "Program";
        NodePtr prog = mk(NodeKind::Program, "", 1);

        while (!check(TokenType::END_OF_FILE)) {
            if (check(TokenType::RBRACE)) {
                // a '}' with no matching '{'. Only report it if nothing else went wrong
                // (otherwise it is probably a side effect of an earlier error).
                if (errors.empty())
                    errors.push_back(CompileError{"Syntax Analysis", peek().line, peek().col,
                        "Unexpected '}' with no matching '{'",
                        "Remove this '}' or add the missing '{' earlier in the program."});
                advance();
                continue;
            }
            NodePtr s = parseStatementSafe(root);
            if (s) prog->kids.push_back(s);
        }

        ParseResult r;
        r.parseTree = root;
        r.ast = prog;
        r.errors = errors;
        return r;
    }

private:
    const std::vector<Token>& toks;
    size_t pos;
    std::vector<CompileError> errors;

    // ------------------------------------------------ token helpers
    const Token& peek() const { return toks[pos]; }
    bool check(TokenType t) const { return toks[pos].type == t; }
    Token advance() {
        Token t = toks[pos];
        if (t.type != TokenType::END_OF_FILE) pos++;
        return t;
    }

    static std::string describe(const Token& t) {
        if (t.type == TokenType::END_OF_FILE) return "the end of the file";
        return "'" + t.text + "'";
    }

    static bool isRelop(TokenType t) {
        return t == TokenType::EQ || t == TokenType::NE || t == TokenType::LT ||
               t == TokenType::GT || t == TokenType::LE || t == TokenType::GE;
    }

    // tokens where we can safely restart parsing after an error
    static bool isSync(TokenType t) {
        return t == TokenType::LET || t == TokenType::PRINT || t == TokenType::INPUT ||
               t == TokenType::IF || t == TokenType::WHILE || t == TokenType::RBRACE ||
               t == TokenType::END_OF_FILE;
    }

    // ------------------------------------------------ node builders
    static NodePtr mk(NodeKind k, const std::string& text, int line) {
        NodePtr n = std::make_shared<Node>();
        n->kind = k;
        n->text = text;
        n->value = 0;
        n->line = line;
        return n;
    }

    static TreePtr tree(const std::string& label, const TreePtr& parent) {
        TreePtr n = std::make_shared<TreeNode>();
        n->label = label;
        parent->kids.push_back(n);
        return n;
    }

    static void leaf(const TreePtr& parent, const Token& t) {
        std::string label;
        if (t.type == TokenType::ID) label = "ID(" + t.text + ")";
        else if (t.type == TokenType::NUMBER) label = "NUMBER(" + t.text + ")";
        else label = "'" + t.text + "'";
        tree(label, parent);
    }

    // ------------------------------------------------ error handling
    [[noreturn]] void fail(const std::string& msg, const std::string& hint) {
        const Token& t = toks[pos];
        int line = t.line, col = t.col;
        std::string m = msg;
        // If the unexpected token is on a later line, the real mistake is usually
        // at the END of the previous line (something is missing there).
        if (pos > 0 && t.line > toks[pos - 1].line) {
            const Token& p = toks[pos - 1];
            line = p.line;
            col = p.col + static_cast<int>(p.text.size());
            if (t.type != TokenType::END_OF_FILE) m += " (it is on the next line)";
        }
        errors.push_back(CompileError{"Syntax Analysis", line, col, m, hint});
        throw Abort();
    }

    static std::string keywordHint(const Token& t) {
        std::string up = toUpper(t.text);
        if (t.text != up && (up == "LET" || up == "PRINT" || up == "INPUT" ||
                             up == "IF" || up == "ELSE" || up == "WHILE"))
            return "Keywords must be written in UPPERCASE. Did you mean '" + up + "'?";
        return "";
    }

    Token expectTok(TokenType type, const std::string& what, const std::string& hint,
                    const TreePtr& node) {
        if (!check(type)) fail("Expected " + what + " but found " + describe(peek()), hint);
        Token t = advance();
        leaf(node, t);
        return t;
    }

    // ------------------------------------------------ statements
    NodePtr parseStatementSafe(const TreePtr& parent) {
        size_t start = pos;
        size_t kidsBefore = parent->kids.size();
        try {
            return parseStatement(parent);
        } catch (const Abort&) {
            parent->kids.resize(kidsBefore);        // drop the half-built subtree
            if (pos == start) advance();            // always make progress
            while (!isSync(peek().type)) advance(); // skip to a safe restart point
            return NodePtr();
        }
    }

    NodePtr parseStatement(const TreePtr& parent) {
        switch (peek().type) {
            case TokenType::LET:   return parseLet(parent);
            case TokenType::ID:    return parseAssign(parent);
            case TokenType::PRINT: return parsePrint(parent);
            case TokenType::INPUT: return parseInput(parent);
            case TokenType::IF:    return parseIf(parent);
            case TokenType::WHILE: return parseWhile(parent);
            default: {
                std::string hint = "A statement must start with LET, PRINT, INPUT, IF, WHILE or a variable name.";
                if (check(TokenType::ELSE))
                    hint = "ELSE must come right after the closing '}' of an IF block.";
                fail("A statement cannot start with " + describe(peek()), hint);
            }
        }
    }

    NodePtr parseLet(const TreePtr& parent) {
        TreePtr n = tree("LetStmt", parent);
        Token kw = advance();
        leaf(n, kw);
        Token id = expectTok(TokenType::ID, "a variable name after LET",
                             "Write it like:  LET x = 10", n);
        expectTok(TokenType::ASSIGN, "'=' after the variable name",
                  "Write it like:  LET x = 10", n);
        NodePtr e = parseExpr(n);
        NodePtr s = mk(NodeKind::Let, id.text, kw.line);
        s->kids.push_back(e);
        return s;
    }

    NodePtr parseAssign(const TreePtr& parent) {
        TreePtr n = tree("AssignStmt", parent);
        Token id = advance();
        leaf(n, id);
        if (!check(TokenType::ASSIGN)) {
            std::string hint = keywordHint(id);
            if (hint.empty()) hint = "An assignment looks like:  x = x + 1";
            fail("Expected '=' after '" + id.text + "' but found " + describe(peek()), hint);
        }
        leaf(n, advance());
        NodePtr e = parseExpr(n);
        NodePtr s = mk(NodeKind::Assign, id.text, id.line);
        s->kids.push_back(e);
        return s;
    }

    NodePtr parsePrint(const TreePtr& parent) {
        TreePtr n = tree("PrintStmt", parent);
        Token kw = advance();
        leaf(n, kw);
        NodePtr e = parseExpr(n);
        NodePtr s = mk(NodeKind::Print, "", kw.line);
        s->kids.push_back(e);
        return s;
    }

    NodePtr parseInput(const TreePtr& parent) {
        TreePtr n = tree("InputStmt", parent);
        Token kw = advance();
        leaf(n, kw);
        Token id = expectTok(TokenType::ID, "a variable name after INPUT",
                             "Write it like:  INPUT n", n);
        return mk(NodeKind::Input, id.text, kw.line);
    }

    NodePtr parseIf(const TreePtr& parent) {
        TreePtr n = tree("IfStmt", parent);
        Token kw = advance();
        leaf(n, kw);
        NodePtr cond = parseCondition(n);
        NodePtr thenB = parseBlock(n);
        NodePtr s = mk(NodeKind::If, "", kw.line);
        s->kids.push_back(cond);
        s->kids.push_back(thenB);
        if (check(TokenType::ELSE)) {
            leaf(n, advance());
            s->kids.push_back(parseBlock(n));
        }
        return s;
    }

    NodePtr parseWhile(const TreePtr& parent) {
        TreePtr n = tree("WhileStmt", parent);
        Token kw = advance();
        leaf(n, kw);
        NodePtr cond = parseCondition(n);
        NodePtr body = parseBlock(n);
        NodePtr s = mk(NodeKind::While, "", kw.line);
        s->kids.push_back(cond);
        s->kids.push_back(body);
        return s;
    }

    NodePtr parseBlock(const TreePtr& parent) {
        TreePtr n = tree("Block", parent);
        Token lb = expectTok(TokenType::LBRACE, "'{' to start a block",
                             "Write it like:  IF x < 5 { PRINT x }", n);
        NodePtr b = mk(NodeKind::Block, "", lb.line);
        while (!check(TokenType::RBRACE) && !check(TokenType::END_OF_FILE)) {
            NodePtr s = parseStatementSafe(n);
            if (s) b->kids.push_back(s);
        }
        expectTok(TokenType::RBRACE, "'}' to close the block",
                  "Every '{' needs a matching '}'.", n);
        return b;
    }

    NodePtr parseCondition(const TreePtr& parent) {
        TreePtr n = tree("Condition", parent);
        NodePtr left = parseExpr(n);
        if (!isRelop(peek().type))
            fail("Expected a comparison operator (==, !=, <, >, <=, >=) but found " + describe(peek()),
                 "A condition compares two values, for example:  x < 10");
        Token op = advance();
        leaf(n, op);
        NodePtr right = parseExpr(n);
        NodePtr c = mk(NodeKind::BinOp, op.text, op.line);
        c->kids.push_back(left);
        c->kids.push_back(right);
        return c;
    }

    // ------------------------------------------------ expressions
    NodePtr parseExpr(const TreePtr& parent) {
        TreePtr n = tree("Expr", parent);
        NodePtr left = parseTerm(n);
        while (check(TokenType::PLUS) || check(TokenType::MINUS)) {
            Token op = advance();
            leaf(n, op);
            NodePtr right = parseTerm(n);
            NodePtr b = mk(NodeKind::BinOp, op.text, op.line);
            b->kids.push_back(left);
            b->kids.push_back(right);
            left = b;
        }
        return left;
    }

    NodePtr parseTerm(const TreePtr& parent) {
        TreePtr n = tree("Term", parent);
        NodePtr left = parseFactor(n);
        while (check(TokenType::STAR) || check(TokenType::SLASH)) {
            Token op = advance();
            leaf(n, op);
            NodePtr right = parseFactor(n);
            NodePtr b = mk(NodeKind::BinOp, op.text, op.line);
            b->kids.push_back(left);
            b->kids.push_back(right);
            left = b;
        }
        return left;
    }

    NodePtr parseFactor(const TreePtr& parent) {
        TreePtr n = tree("Factor", parent);
        if (check(TokenType::NUMBER)) {
            Token t = advance();
            leaf(n, t);
            NodePtr num = mk(NodeKind::Num, t.text, t.line);
            num->value = std::stoll(t.text);
            return num;
        }
        if (check(TokenType::ID)) {
            Token t = advance();
            leaf(n, t);
            return mk(NodeKind::Var, t.text, t.line);
        }
        if (check(TokenType::LPAREN)) {
            leaf(n, advance());
            NodePtr e = parseExpr(n);
            expectTok(TokenType::RPAREN, "')' to close the '('",
                      "Every '(' needs a matching ')'.", n);
            return e;   // the AST does not keep parentheses
        }
        if (check(TokenType::MINUS)) {
            Token t = advance();
            leaf(n, t);
            NodePtr operand = parseFactor(n);
            NodePtr neg = mk(NodeKind::Neg, "-", t.line);
            neg->kids.push_back(operand);
            return neg;
        }
        std::string hint = "An expression is made of numbers, variables, operators and parentheses.";
        if (pos > 0 && toks[pos - 1].type == TokenType::ASSIGN)
            hint = "Did you forget the value on the right side of '='?";
        else if (pos > 0 && (toks[pos - 1].type == TokenType::PLUS || toks[pos - 1].type == TokenType::MINUS ||
                             toks[pos - 1].type == TokenType::STAR || toks[pos - 1].type == TokenType::SLASH))
            hint = "An operator such as '" + toks[pos - 1].text + "' needs a value on its right.";
        fail("Expected a number, a variable or '(' but found " + describe(peek()), hint);
    }
};

// ------------------------------------------------ drawing trees as text
void drawTree(const TreePtr& n, const std::string& prefix, bool last, bool root, std::ostringstream& out) {
    if (root) out << n->label << "\n";
    else out << prefix << (last ? "└── " : "├── ") << n->label << "\n";
    std::string childPrefix = root ? "" : prefix + (last ? "    " : "│   ");
    for (size_t i = 0; i < n->kids.size(); i++)
        drawTree(n->kids[i], childPrefix, i + 1 == n->kids.size(), false, out);
}

std::string astLabel(const NodePtr& n) {
    std::string line = "  (line " + std::to_string(n->line) + ")";
    switch (n->kind) {
        case NodeKind::Program: return "Program";
        case NodeKind::Let:     return "Let " + n->text + line;
        case NodeKind::Assign:  return "Assign " + n->text + line;
        case NodeKind::Print:   return "Print" + line;
        case NodeKind::Input:   return "Input " + n->text + line;
        case NodeKind::If:      return "If" + line;
        case NodeKind::While:   return "While" + line;
        case NodeKind::Block:   return "Block";
        case NodeKind::BinOp:   return "BinOp " + n->text;
        case NodeKind::Neg:     return "Neg (unary -)";
        case NodeKind::Num:     return "Num " + std::to_string(n->value);
        case NodeKind::Var:     return "Var " + n->text;
    }
    return "?";
}

std::string astRole(const NodePtr& parent, size_t i) {
    switch (parent->kind) {
        case NodeKind::If:     return i == 0 ? "condition: " : (i == 1 ? "then: " : "else: ");
        case NodeKind::While:  return i == 0 ? "condition: " : "body: ";
        case NodeKind::Let:
        case NodeKind::Assign:
        case NodeKind::Print:  return "value: ";
        case NodeKind::BinOp:  return i == 0 ? "left: " : "right: ";
        case NodeKind::Neg:    return "operand: ";
        default:               return "";
    }
}

void drawAst(const NodePtr& n, const std::string& prefix, bool last, bool root,
             const std::string& role, std::ostringstream& out) {
    if (root) out << role << astLabel(n) << "\n";
    else out << prefix << (last ? "└── " : "├── ") << role << astLabel(n) << "\n";
    std::string childPrefix = root ? "" : prefix + (last ? "    " : "│   ");
    for (size_t i = 0; i < n->kids.size(); i++)
        drawAst(n->kids[i], childPrefix, i + 1 == n->kids.size(), false, astRole(n, i), out);
}

}  // namespace

ParseResult parseTokens(const std::vector<Token>& tokens) {
    Parser p(tokens);
    return p.run();
}

std::string treeToText(const TreePtr& root) {
    std::ostringstream out;
    drawTree(root, "", true, true, out);
    return out.str();
}

std::string astToText(const NodePtr& root) {
    std::ostringstream out;
    drawAst(root, "", true, true, "", out);
    return out.str();
}

int countTreeNodes(const TreePtr& root) {
    int n = 1;
    for (size_t i = 0; i < root->kids.size(); i++) n += countTreeNodes(root->kids[i]);
    return n;
}

int countAstNodes(const NodePtr& root) {
    int n = 1;
    for (size_t i = 0; i < root->kids.size(); i++) n += countAstNodes(root->kids[i]);
    return n;
}
