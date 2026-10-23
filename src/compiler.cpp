// compiler.cpp - connects all phases into one pipeline and builds the Learning Trail.
#include "compiler.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <sstream>

#include "codegen.h"
#include "interpreter.h"
#include "ir.h"
#include "lexer.h"
#include "optimizer.h"
#include "parser.h"
#include "semantic.h"

namespace {

// ------------------------------------------------------------------
// Beginner-friendly text for each phase (the "Learning Trail" content)
// ------------------------------------------------------------------
struct PhaseInfo {
    const char* id;
    const char* title;
    const char* explain;
    const char* next;
};

const PhaseInfo INFO[10] = {
    {"lexer", "1. Lexical Analysis",
     "The lexer reads your program one character at a time and groups the characters into TOKENS - "
     "the smallest meaningful pieces: keywords (LET), names (x), numbers (10) and operators (+). "
     "Spaces, new lines and # comments are thrown away. A character that does not belong to the "
     "language (such as @) is a LEXICAL ERROR.",
     "The parser will take this list of tokens and check that they are in a legal order."},

    {"syntax", "2. Syntax Analysis",
     "The parser checks that the tokens follow the GRAMMAR of the language - for example LET must be "
     "followed by a name, '=' and an expression. This compiler uses recursive descent: one small "
     "function for every grammar rule. If the order of tokens breaks a rule, that is a SYNTAX ERROR. "
     "After an error the parser skips ahead to the next statement so that several errors can be reported at once.",
     "While checking the grammar the parser also recorded which rules it used. That record is the parse tree."},

    {"parsetree", "3. Parse Tree",
     "The parse tree shows exactly which grammar rules were used. Every rule (Expr, Term, Factor, ...) "
     "is a node and every token is a leaf, so keywords, '=' and parentheses all appear. "
     "Notice how Expr -> Term -> Factor makes '*' bind tighter than '+': that is operator precedence.",
     "The parse tree contains many details that later phases do not need. The AST keeps only the meaning."},

    {"ast", "4. Abstract Syntax Tree (AST)",
     "The AST is a simplified tree. It keeps operations and values but drops grammar-rule nodes, "
     "keywords, '=' and parentheses - the SHAPE of the tree already shows the order of evaluation. "
     "Later phases work on the AST because it is much smaller than the parse tree.",
     "Semantic analysis walks over the AST and checks the MEANING of the program."},

    {"semantic", "5. Semantic Analysis",
     "A program can have perfect grammar and still make no sense. Semantic analysis checks the meaning: "
     "variables must be declared before use, not declared twice, assignments must target declared "
     "variables, and you may not divide by the constant 0. (All values are integers, so there is no "
     "type-mismatch check in this language.) Warnings are shown for variables that are never used.",
     "While checking, the analyser recorded every variable in the symbol table."},

    {"symbols", "6. Symbol Table",
     "The symbol table is the compiler's notebook about variables: name, type, the line where it was "
     "declared, and how many times it is read and written. Later phases and tools use it to know "
     "which names exist.",
     "The code generator now translates the checked AST into intermediate code."},

    {"ir", "7. Intermediate Code (Three-Address Code)",
     "Three-address code (TAC) breaks big expressions into simple steps with at most one operator: "
     "result = a op b. The compiler invents temporary names t1, t2, ... for partial results. "
     "IF and WHILE become labels (L1:) and jumps (goto, ifFalse), just like real machine code. "
     "TAC does not depend on any particular computer.",
     "The optimiser will improve this code without changing what the program does."},

    {"opt", "8. Code Optimization",
     "This compiler performs: (1) CONSTANT FOLDING - calculating constant expressions at compile time "
     "(2 * 3 becomes 6); (2) CONSTANT PROPAGATION - replacing a variable by its known constant value "
     "inside straight-line code; (3) removing temporaries that are no longer used and merging "
     "'t1 = a + b ; x = t1' into 'x = a + b'. Optimisation must NEVER change the meaning of the "
     "program - step 10 proves it by running both versions.",
     "The code generator turns the optimised TAC into pseudo-assembly."},

    {"codegen", "9. Target Code Generation (pseudo-assembly)",
     "Each TAC instruction is translated into instructions for an IMAGINARY machine with two registers "
     "(R1, R2) and named memory locations. '#5' means the constant 5. IMPORTANT: this is illustrative "
     "pseudo-assembly, NOT real machine code - it cannot be run on a real CPU. It shows how a compiler "
     "back end works.",
     "Pseudo-assembly cannot run on a real computer, so step 10 executes the TAC instead."},

    {"run", "10. Execution Check (interpreter)",
     "This compiler includes a tiny interpreter that executes the three-address code. It runs the "
     "ORIGINAL TAC and the OPTIMISED TAC with the same input values and compares the output. "
     "If they match, the optimisation preserved the meaning of the program.",
     "That is the end of the pipeline. Try introducing a mistake in your source code and see which phase catches it."},
};

std::vector<std::string> splitLines(const std::string& s) {
    std::vector<std::string> lines;
    std::string cur;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '\n') { lines.push_back(cur); cur.clear(); }
        else if (s[i] != '\r') cur += s[i];
    }
    lines.push_back(cur);
    return lines;
}

std::string joinValues(const std::vector<std::string>& v) {
    if (v.empty()) return "(no output)";
    std::string s;
    for (size_t i = 0; i < v.size(); i++) s += (i ? " " : "") + v[i];
    return s;
}

// ------------------------------------------------------------------
// Line tracer helpers
// ------------------------------------------------------------------
void collectStatements(const NodePtr& n, int line, std::vector<NodePtr>& out) {
    switch (n->kind) {
        case NodeKind::Program:
        case NodeKind::Block:
            for (size_t i = 0; i < n->kids.size(); i++) collectStatements(n->kids[i], line, out);
            break;
        case NodeKind::Let: case NodeKind::Assign: case NodeKind::Print:
        case NodeKind::Input: case NodeKind::If: case NodeKind::While:
            if (n->line == line) { out.push_back(n); return; }
            for (size_t i = 1; i < n->kids.size(); i++) collectStatements(n->kids[i], line, out);
            break;
        default:
            break;
    }
}

std::string tokenLabel(const Token& t) {
    if (t.type <= TokenType::WHILE) return tokenTypeName(t.type);
    return tokenTypeName(t.type) + "(" + t.text + ")";
}

std::string indentLines(const std::string& text, const std::string& pad) {
    std::istringstream in(text);
    std::string line, out;
    while (std::getline(in, line)) out += pad + line + "\n";
    return out;
}

std::string codeForLine(const std::vector<Instr>& code, int line) {
    std::ostringstream out;
    bool any = false;
    for (size_t i = 0; i < code.size(); i++)
        if (code[i].line == line) {
            out << "    " << (i + 1) << ":  " << instrToString(code[i]) << "\n";
            any = true;
        }
    if (!any) out << "    (no instructions for this line)\n";
    return out.str();
}

// ------------------------------------------------------------------
// Error formatting
// ------------------------------------------------------------------
std::string sourceOf(const CompileError& e, const std::vector<std::string>& lines) {
    if (e.line >= 1 && e.line <= static_cast<int>(lines.size())) return lines[e.line - 1];
    return "";
}

std::string formatError(const CompileError& e, const std::vector<std::string>& lines) {
    std::ostringstream out;
    out << "  [" << e.phase << "] line " << e.line;
    if (e.col > 0) out << ", col " << e.col;
    out << ": " << e.message << "\n";
    std::string src = sourceOf(e, lines);
    out << "      " << e.line << " | " << src << "\n";
    if (e.col > 0) {
        std::string numPad(std::to_string(e.line).size(), ' ');
        out << "      " << numPad << " | " << std::string(e.col - 1, ' ') << "^\n";
    }
    if (!e.hint.empty()) out << "      Hint: " << e.hint << "\n";
    return out.str();
}

// ------------------------------------------------------------------
// JSON helpers
// ------------------------------------------------------------------
std::string jstr(const std::string& s) {
    std::string o = "\"";
    for (size_t i = 0; i < s.size(); i++) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        switch (c) {
            case '"': o += "\\\""; break;
            case '\\': o += "\\\\"; break;
            case '\n': o += "\\n"; break;
            case '\r': o += "\\r"; break;
            case '\t': o += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof buf, "\\u%04x", c);
                    o += buf;
                } else {
                    o += static_cast<char>(c);
                }
        }
    }
    return o + "\"";
}

std::string errorsToJson(const std::vector<CompileError>& v, const std::vector<std::string>& lines) {
    std::string o = "[";
    for (size_t i = 0; i < v.size(); i++) {
        if (i) o += ",";
        o += "{\"phase\":" + jstr(v[i].phase) + ",\"line\":" + std::to_string(v[i].line) +
             ",\"col\":" + std::to_string(v[i].col) + ",\"message\":" + jstr(v[i].message) +
             ",\"hint\":" + jstr(v[i].hint) + ",\"source\":" + jstr(sourceOf(v[i], lines)) + "}";
    }
    return o + "]";
}

}  // namespace

// ==================================================================
// The pipeline
// ==================================================================
Report compileSource(const std::string& src, const std::vector<long long>& inputs) {
    Report rep;
    rep.success = false;
    rep.sourceLines = splitLines(src);

    for (int i = 0; i < 10; i++) {
        PhaseReport p;
        p.id = INFO[i].id;
        p.title = INFO[i].title;
        p.status = "skipped";
        p.explain = INFO[i].explain;
        p.next = INFO[i].next;
        rep.phases.push_back(p);
    }
    std::vector<PhaseReport>& P = rep.phases;

    std::string blocked;   // name of the first phase that reported errors
    bool haveAst = false, haveIr = false, haveOpt = false, haveAsm = false;
    NodePtr ast;
    std::vector<Instr> ir, optCode;
    std::vector<AsmLine> asmLines;

    // ---------------- 1. lexical analysis
    LexResult lx = tokenize(src);
    P[0].output = tokensToText(lx.tokens);
    P[0].errors = lx.errors;
    P[0].status = lx.errors.empty() ? "ok" : "error";
    P[0].changes = "Source code (" + std::to_string(src.size()) + " characters, " +
                   std::to_string(rep.sourceLines.size()) + " lines)  ->  " +
                   std::to_string(lx.tokens.size() - 1) + " tokens. Spaces, new lines and comments were discarded.";

    do {
        if (!lx.errors.empty()) { blocked = "Lexical Analysis"; break; }

        // ---------------- 2-4. syntax analysis, parse tree, AST
        ParseResult pr = parseTokens(lx.tokens);
        P[1].errors = pr.errors;
        if (!pr.errors.empty()) {
            P[1].status = "error";
            P[1].output = "The parser found " + std::to_string(pr.errors.size()) +
                          " syntax error(s) - see the errors below.\n";
            P[1].changes = "The token list does not follow the grammar, so no parse tree can be built.";
            blocked = "Syntax Analysis";
            break;
        }
        P[1].status = "ok";
        P[1].output = "Syntax is correct. The parser accepted " + std::to_string(pr.ast->kids.size()) +
                      " top-level statement(s).\n";
        P[1].changes = std::to_string(lx.tokens.size() - 1) + " tokens  ->  grammar check passed (a flat list became a structured program).";

        P[2].status = "ok";
        P[2].output = treeToText(pr.parseTree);
        P[2].changes = "Tokens  ->  parse tree with " + std::to_string(countTreeNodes(pr.parseTree)) +
                       " nodes (grammar rules + tokens).";

        ast = pr.ast;
        haveAst = true;
        P[3].status = "ok";
        P[3].output = astToText(ast);
        P[3].changes = "Parse tree (" + std::to_string(countTreeNodes(pr.parseTree)) + " nodes)  ->  AST (" +
                       std::to_string(countAstNodes(ast)) +
                       " nodes). Dropped: grammar-rule nodes, keywords, '=' and parentheses.";

        // ---------------- 5-6. semantic analysis, symbol table
        SemanticResult sr = analyze(ast);
        P[4].errors = sr.errors;
        P[4].warnings = sr.warnings;
        P[4].status = !sr.errors.empty() ? "error" : (!sr.warnings.empty() ? "warning" : "ok");
        std::ostringstream so;
        so << "Checks performed:\n"
           << "  - every variable is declared before it is used\n"
           << "  - no variable is declared twice\n"
           << "  - assignments and INPUT only target declared variables\n"
           << "  - no division by the constant 0\n"
           << "  - the names t1, t2, ... are not used (reserved for the compiler)\n\n"
           << "Result: " << sr.errors.size() << " error(s), " << sr.warnings.size() << " warning(s).\n";
        P[4].output = so.str();
        P[4].changes = "The AST itself is unchanged, but it has now been checked and every variable has been recorded.";

        P[5].status = "ok";
        P[5].output = symbolsToText(sr.symbols);
        if (!sr.errors.empty()) P[5].output += "\n(partial table: semantic errors were found, see phase 5)\n";
        P[5].changes = "New data structure: a symbol table with " + std::to_string(sr.symbols.size()) +
                       " variable(s).";
        if (!sr.errors.empty()) { blocked = "Semantic Analysis"; break; }

        // ---------------- 7. intermediate code
        ir = generateIR(ast);
        haveIr = true;
        P[6].status = "ok";
        P[6].output = irToText(ir);
        P[6].changes = "AST (" + std::to_string(countAstNodes(ast)) + " nodes)  ->  " + std::to_string(ir.size()) +
                       " three-address instructions. Nested expressions became single steps; IF/WHILE became labels and jumps.";

        // ---------------- 8. optimisation
        OptResult opt = optimize(ir);
        optCode = opt.code;
        haveOpt = true;
        P[7].status = "ok";
        std::ostringstream oo;
        oo << "Instructions before: " << ir.size() << "    after: " << optCode.size() << "\n\n";
        oo << "--- What the optimiser did ---\n";
        if (opt.log.empty()) oo << "(nothing to optimise in this program)\n";
        for (size_t i = 0; i < opt.log.size(); i++) oo << "  * " << opt.log[i] << "\n";
        oo << "\n--- Optimised code ---\n" << irToText(optCode);
        P[7].output = oo.str();
        P[7].changes = std::to_string(ir.size()) + " instructions  ->  " + std::to_string(optCode.size()) +
                       " instructions. The program does the same thing with fewer steps.";

        // ---------------- 9. target code
        asmLines = generateAssembly(optCode);
        haveAsm = true;
        P[8].status = "ok";
        P[8].output = asmToText(asmLines);
        P[8].changes = std::to_string(optCode.size()) + " TAC instructions  ->  " +
                       std::to_string(asmLines.size()) + " lines of pseudo-assembly (data declarations, comments and instructions).";

        // ---------------- 10. execution check
        RunResult before = runIR(ir, inputs);
        RunResult after = runIR(optCode, inputs);
        std::ostringstream ro;
        ro << "Input values given     : ";
        if (inputs.empty()) ro << "(none)";
        for (size_t i = 0; i < inputs.size(); i++) ro << (i ? " " : "") << inputs[i];
        ro << "\n";
        ro << "Original code output   : " << joinValues(before.output) << "\n";
        ro << "Optimised code output  : " << joinValues(after.output) << "\n";
        ro << "Steps executed         : original " << before.steps << ", optimised " << after.steps << "\n";
        if (!before.error.empty()) ro << "Original code stopped  : " << before.error << "\n";
        if (!after.error.empty()) ro << "Optimised code stopped : " << after.error << "\n";
        bool same = (before.output == after.output) && (before.error == after.error);
        ro << "\nComparison: " << (same ? "IDENTICAL - the optimisation preserved the meaning of the program."
                                        : "DIFFERENT - the optimiser has a bug!") << "\n";
        P[9].output = ro.str();
        P[9].status = !same ? "error" : (before.error.empty() ? "ok" : "warning");
        P[9].changes = "No new representation: the TAC is executed and the two results are compared.";
    } while (false);

    // ---------------- phases after a failure are skipped
    for (size_t i = 0; i < P.size(); i++) {
        if (P[i].status == "skipped")
            P[i].output = "Skipped: compilation stopped earlier because " + blocked +
                          " reported errors. Fix those errors first.\n";
    }

    // ---------------- source-line tracer
    for (int ln = 1; ln <= static_cast<int>(rep.sourceLines.size()); ln++) {
        std::ostringstream t;
        t << "Source line " << ln << ":  " << rep.sourceLines[ln - 1] << "\n\n";

        t << "1. Tokens on this line:\n    ";
        bool anyTok = false;
        for (size_t i = 0; i < lx.tokens.size(); i++)
            if (lx.tokens[i].line == ln && lx.tokens[i].type != TokenType::END_OF_FILE) {
                t << tokenLabel(lx.tokens[i]) << "  ";
                anyTok = true;
            }
        if (!anyTok) t << "(none - blank line or comment)";
        t << "\n\n";

        if (haveAst) {
            std::vector<NodePtr> stmts;
            collectStatements(ast, ln, stmts);
            t << "2. AST of the statement that starts on this line:\n";
            if (stmts.empty()) t << "    (no statement starts on this line)\n";
            for (size_t i = 0; i < stmts.size(); i++) t << indentLines(astToText(stmts[i]), "    ");
            t << "\n";
        }
        if (haveIr) t << "3. Original three-address code:\n" << codeForLine(ir, ln) << "\n";
        if (haveOpt) t << "4. Optimised three-address code:\n" << codeForLine(optCode, ln) << "\n";
        if (haveAsm) {
            t << "5. Pseudo-assembly:\n";
            bool any = false;
            for (size_t i = 0; i < asmLines.size(); i++)
                if (asmLines[i].line == ln) { t << "    " << asmLines[i].text << "\n"; any = true; }
            if (!any) t << "    (nothing generated for this line)\n";
            t << "\n";
        }

        bool anyErr = false;
        for (size_t p = 0; p < P.size(); p++) {
            for (size_t i = 0; i < P[p].errors.size(); i++)
                if (P[p].errors[i].line == ln) {
                    if (!anyErr) t << "Errors on this line:\n";
                    t << formatError(P[p].errors[i], rep.sourceLines);
                    anyErr = true;
                }
        }
        if (!blocked.empty())
            t << "(Later representations are not shown because " << blocked << " reported errors.)\n";
        rep.lineTraces[ln] = t.str();
    }

    rep.success = blocked.empty();
    return rep;
}

// ==================================================================
// Output as text (terminal)
// ==================================================================
std::string reportToText(const Report& r, const std::string& onlyPhase) {
    std::ostringstream out;
    int errors = 0, warnings = 0;
    for (size_t i = 0; i < r.phases.size(); i++) {
        const PhaseReport& p = r.phases[i];
        errors += static_cast<int>(p.errors.size());
        warnings += static_cast<int>(p.warnings.size());
        if (!onlyPhase.empty() && onlyPhase != p.id && onlyPhase != std::to_string(i + 1)) continue;

        std::string status = p.status;
        for (size_t k = 0; k < status.size(); k++) status[k] = static_cast<char>(toupper(static_cast<unsigned char>(status[k])));
        out << "================================================================\n";
        out << " " << p.title << "   [" << status << "]\n";
        out << "================================================================\n";
        out << "WHAT THIS PHASE DOES:\n  " << p.explain << "\n\n";
        out << "OUTPUT:\n" << indentLines(p.output, "  ") << "\n";
        if (!p.errors.empty()) {
            out << "ERRORS (" << p.errors.size() << "):\n";
            for (size_t k = 0; k < p.errors.size(); k++) out << formatError(p.errors[k], r.sourceLines);
            out << "\n";
        }
        if (!p.warnings.empty()) {
            out << "WARNINGS (" << p.warnings.size() << "):\n";
            for (size_t k = 0; k < p.warnings.size(); k++) out << formatError(p.warnings[k], r.sourceLines);
            out << "\n";
        }
        if (!p.changes.empty()) out << "HOW THE REPRESENTATION CHANGED:\n  " << p.changes << "\n\n";
        out << "NEXT STAGE:\n  " << p.next << "\n\n";
    }
    out << "RESULT: " << (r.success ? "compilation succeeded" : "compilation FAILED") << " ("
        << errors << " error(s), " << warnings << " warning(s))\n";
    return out.str();
}

// ==================================================================
// Output as JSON (web interface)
// ==================================================================
std::string reportToJson(const Report& r) {
    int errors = 0, warnings = 0;
    std::string phases = "[";
    for (size_t i = 0; i < r.phases.size(); i++) {
        const PhaseReport& p = r.phases[i];
        errors += static_cast<int>(p.errors.size());
        warnings += static_cast<int>(p.warnings.size());
        if (i) phases += ",";
        phases += "{\"id\":" + jstr(p.id) + ",\"title\":" + jstr(p.title) + ",\"status\":" + jstr(p.status) +
                  ",\"explain\":" + jstr(p.explain) + ",\"output\":" + jstr(p.output) +
                  ",\"changes\":" + jstr(p.changes) + ",\"next\":" + jstr(p.next) +
                  ",\"errors\":" + errorsToJson(p.errors, r.sourceLines) +
                  ",\"warnings\":" + errorsToJson(p.warnings, r.sourceLines) + "}";
    }
    phases += "]";

    std::string traces = "{";
    bool first = true;
    for (std::map<int, std::string>::const_iterator it = r.lineTraces.begin(); it != r.lineTraces.end(); ++it) {
        if (!first) traces += ",";
        first = false;
        traces += "\"" + std::to_string(it->first) + "\":" + jstr(it->second);
    }
    traces += "}";

    return std::string("{\"success\":") + (r.success ? "true" : "false") +
           ",\"errorCount\":" + std::to_string(errors) + ",\"warningCount\":" + std::to_string(warnings) +
           ",\"lineCount\":" + std::to_string(r.sourceLines.size()) +
           ",\"phases\":" + phases + ",\"traces\":" + traces + "}";
}

// ==================================================================
// Input values ("5 3 -2") for INPUT statements
// ==================================================================
bool parseInputValues(const std::string& text, std::vector<long long>& out, std::string& err) {
    std::istringstream in(text);
    std::string word;
    while (in >> word) {
        if (!isNumberText(word) || word.size() > 18) {
            err = "Input values must be whole numbers separated by spaces (problem: '" + word + "').";
            return false;
        }
        out.push_back(std::stoll(word));
    }
    return true;
}
