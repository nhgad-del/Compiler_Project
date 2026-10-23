#!/bin/bash
# run_tests.sh - runs every test case against ./explorer and records the REAL result.
# Usage (from the project folder):   make test      or      bash tests/run_tests.sh
# Results are printed and also saved (tab separated) in tests/results.tsv

cd "$(dirname "$0")/.." || exit 2
BIN=./explorer
[ -x "$BIN" ] || { echo "Build the program first with:  make"; exit 2; }

RESULTS=tests/results.tsv
: > "$RESULTS"
PASS=0
FAIL=0

record() {   # id description phase command expected actual status
    printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\n' "$1" "$2" "$3" "$4" "$5" "$6" "$7" >> "$RESULTS"
    if [ "$7" = "PASS" ]; then PASS=$((PASS + 1)); else FAIL=$((FAIL + 1)); fi
    printf '%-5s %-4s %s\n' "$1" "$7" "$2"
}

# t ID DESC PHASE EXPECTED CMD...        -> the output must CONTAIN EXPECTED
t() {
    local id=$1 desc=$2 phase=$3 expected=$4; shift 4
    local out line
    out=$("$@" 2>&1)
    line=$(printf '%s\n' "$out" | grep -F -m1 -- "$expected" | sed 's/^[[:space:]]*//')
    if [ -n "$line" ]; then record "$id" "$desc" "$phase" "$*" "$expected" "$line" PASS
    else record "$id" "$desc" "$phase" "$*" "$expected" "(expected text not found)" FAIL; fi
}

# tn ID DESC PHASE UNWANTED CMD...       -> the output must NOT contain UNWANTED
tn() {
    local id=$1 desc=$2 phase=$3 unwanted=$4; shift 4
    local out
    out=$("$@" 2>&1)
    if printf '%s\n' "$out" | grep -qF -- "$unwanted"; then
        record "$id" "$desc" "$phase" "$*" "does not contain: $unwanted" "(found it)" FAIL
    else
        record "$id" "$desc" "$phase" "$*" "does not contain: $unwanted" "(text absent, as expected)" PASS
    fi
}

# tp ID DESC PHASE EXPECTED 'source' [options]   -> source code is given on standard input
tp() {
    local id=$1 desc=$2 phase=$3 expected=$4 src=$5; shift 5
    local out line
    out=$(printf '%b' "$src" | "$BIN" - "$@" 2>&1)
    line=$(printf '%s\n' "$out" | grep -F -m1 -- "$expected" | sed 's/^[[:space:]]*//')
    local shown="printf '${src//\\n/\\n}' | ./explorer - $*"
    if [ -n "$line" ]; then record "$id" "$desc" "$phase" "$shown" "$expected" "$line" PASS
    else record "$id" "$desc" "$phase" "$shown" "$expected" "(expected text not found)" FAIL; fi
}

# tx ID DESC PHASE EXPECTED_EXIT_CODE CMD...  -> check the exit code
tx() {
    local id=$1 desc=$2 phase=$3 code=$4; shift 4
    "$@" > /dev/null 2>&1
    local got=$?
    if [ "$got" = "$code" ]; then record "$id" "$desc" "$phase" "$*" "exit code $code" "exit code $got" PASS
    else record "$id" "$desc" "$phase" "$*" "exit code $code" "exit code $got" FAIL; fi
}

E=examples
RUN_ORIG="Original code output   : "
RUN_OPT="Optimised code output  : "

# ---------------------------------------------------------------- valid programs
t  T01 "Valid arithmetic (x + y)"                     "Execution check"   "${RUN_OPT}30"           $BIN $E/01_arithmetic.txt
t  T02 "Operator precedence and parentheses"          "Execution check"   "${RUN_ORIG}14 20 3 10 2" $BIN $E/02_precedence.txt
t  T03 "Parse tree keeps the parentheses"             "Parse tree"        "'('"                     $BIN $E/02_precedence.txt --phase parsetree
tn T04 "AST does not keep the parentheses"            "AST"               "'('"                     $BIN $E/02_precedence.txt --phase ast
t  T05 "Variable declaration then assignment"         "Execution check"   "${RUN_ORIG}12"           $BIN $E/03_assignment.txt
t  T06 "IF/ELSE with a constant condition (true)"     "Execution check"   "${RUN_OPT}1"             $BIN $E/04_if_else.txt
t  T07 "Optimiser decides constant IF at compile time" "Optimisation"     "condition is always true" $BIN $E/04_if_else.txt --phase opt
t  T08 "IF/ELSE with INPUT, first branch"             "Execution check"   "${RUN_OPT}7"             $BIN $E/05_input_max.txt --input "7 3"
t  T09 "IF/ELSE with INPUT, else branch"              "Execution check"   "${RUN_OPT}9"             $BIN $E/05_input_max.txt --input "2 9"
t  T10 "WHILE loop (sum 1..5)"                        "Execution check"   "${RUN_ORIG}15"           $BIN $E/06_while_sum.txt
t  T11 "WHILE + INPUT (factorial of 5)"               "Execution check"   "${RUN_ORIG}120"          $BIN $E/07_factorial.txt --input "5"
t  T12 "Nested WHILE and IF"                          "Execution check"   "${RUN_ORIG}3"            $BIN $E/23_nested.txt
tp T13 "Integer division truncates toward zero"       "Execution check"   "${RUN_ORIG}3 -3"         'PRINT 7 / 2\nPRINT (0 - 7) / 2\n'
tp T14 "Unary minus"                                  "Execution check"   "${RUN_OPT}-5"            'PRINT -(2 + 3)\n'
tp T15 "Comments are ignored"                         "Lexical Analysis"  "${RUN_ORIG}1"            '# a comment\nPRINT 1 # another\n'
tp T16 "Empty program compiles"                       "All"               "compilation succeeded"   ''

# ---------------------------------------------------------------- optimisation
t  T17 "Constant folding (2 * 3)"                     "Optimisation"      "constant folding: 2 * 3 = 6" $BIN $E/08_constant_folding.txt --phase opt
t  T18 "Constant propagation + folding result"        "Execution check"   "${RUN_OPT}20 -5"         $BIN $E/08_constant_folding.txt
t  T19 "Optimised result identical to original"       "Execution check"   "IDENTICAL"               $BIN $E/08_constant_folding.txt
tn T20 "No propagation across a loop label"           "Optimisation"      "'i' replaced"            $BIN $E/06_while_sum.txt --phase opt
t  T21 "Temporaries merged (t2 into sum)"             "Optimisation"      "merged temporary t2 into 'sum'" $BIN $E/06_while_sum.txt --phase opt
t  T22 "Pseudo-assembly is generated"                 "Code generation"   "STORE total, R1"         $BIN $E/01_arithmetic.txt --phase codegen

# ---------------------------------------------------------------- errors
t  T23 "Invalid character"                            "Lexical"           "Invalid character '@'"   $BIN $E/11_err_invalid_char.txt
t  T24 "Malformed number (12abc)"                     "Lexical"           "Malformed number '12abc'" $BIN $E/18_err_bad_number.txt
tp T25 "Number too large"                             "Lexical"           "is too large"            'LET x = 1234567890123456789012\n'
t  T26 "Missing assignment value"                     "Syntax"            "Did you forget the value on the right side of '='?" $BIN $E/10_err_missing_value.txt
t  T27 "Missing closing parenthesis"                  "Syntax"            "Expected ')' to close the '('" $BIN $E/12_err_missing_paren.txt
t  T28 "Missing closing brace"                        "Syntax"            "Expected '}' to close the block" $BIN $E/15_err_missing_brace.txt
t  T29 "Lower-case keyword hint"                      "Syntax"            "Did you mean 'LET'?"     $BIN $E/16_err_lowercase.txt
t  T30 "Error recovery: 4 errors in one run"          "Syntax"            "ERRORS (4):"             $BIN $E/19_err_multiple.txt
t  T31 "Undefined variable"                           "Semantic"          "Variable 'y' is used before it is declared" $BIN $E/09_err_undefined.txt
t  T32 "Assignment to undeclared variable"            "Semantic"          "Assignment to undeclared variable 'x'" $BIN $E/17_err_assign_undeclared.txt
t  T33 "Variable declared twice"                      "Semantic"          "is already declared"     $BIN $E/14_err_redeclare.txt
t  T34 "Division by constant zero"                     "Semantic"          "Division by zero"        $BIN $E/13_err_div_zero.txt
tp T35 "Reserved temporary name (t1)"                 "Semantic"          "is reserved"             'LET t1 = 5\nPRINT t1\n'
t  T36 "Unused variable gives a warning only"         "Semantic"          "declared but its value is never used" $BIN $E/21_warning_unused.txt
tx T37 "Warning does not stop compilation (exit 0)"   "Semantic"          0                         $BIN $E/21_warning_unused.txt
t  T38 "Division by zero variable caught at run time" "Execution check"   "run-time error on line 3: division by zero" $BIN $E/20_runtime_div_zero.txt
t  T39 "Infinite loop stopped by step limit"          "Execution check"   "stopped after 200000 steps" $BIN $E/22_infinite_loop.txt
tp T40 "Arithmetic overflow detected"                 "Execution check"   "arithmetic overflow"     'LET x = 999999999999999999\nPRINT x * 999999999999999999\n'
t  T41 "INPUT without values gives a clear message"   "Execution check"   "INPUT needs a value"     $BIN $E/05_input_max.txt
t  T42 "Non-numeric --input is rejected"              "Command line"      "Input values must be whole numbers" $BIN $E/01_arithmetic.txt --input abc
tx T43 "Compile error gives exit code 1"              "All"               1                         $BIN $E/09_err_undefined.txt
tx T44 "Success gives exit code 0"                    "All"               0                         $BIN $E/01_arithmetic.txt

# ---------------------------------------------------------------- novelty: Phase-by-Phase Learning Trail
t  T45 "Single phase selectable (by name)"            "Learning Trail"    "WHAT THIS PHASE DOES"    $BIN $E/01_arithmetic.txt --phase lexer
t  T46 "Phase shows how representation changed"       "Learning Trail"    "HOW THE REPRESENTATION CHANGED" $BIN $E/01_arithmetic.txt --phase 4
t  T47 "Phase explains the next stage"                "Learning Trail"    "NEXT STAGE"              $BIN $E/01_arithmetic.txt --phase ast
t  T48 "Error Learning Mode names the phase + hint"   "Learning Trail"    "[Semantic Analysis] line 3" $BIN $E/09_err_undefined.txt
t  T49 "Error shows source line with caret"           "Learning Trail"    "|               ^"            $BIN $E/12_err_missing_paren.txt
t  T50 "Later phases are skipped after an error"      "Learning Trail"    "Skipped: compilation stopped earlier because Lexical Analysis" $BIN $E/11_err_invalid_char.txt
t  T51 "Source-line tracer: optimised code shown"     "Learning Trail"    "4. Optimised three-address code:" $BIN $E/06_while_sum.txt --trace 5
t  T52 "Source-line tracer: unoptimised vs optimised" "Learning Trail"    "6:  sum = sum + i"       $BIN $E/06_while_sum.txt --trace 5
tx T53 "Tracer rejects a line that does not exist"    "Learning Trail"    2                         $BIN $E/06_while_sum.txt --trace 99

# ---------------------------------------------------------------- web interface (HTTP)
if command -v curl > /dev/null 2>&1; then
    PORT=8799
    "$BIN" --serve $PORT > /dev/null 2>&1 &
    SERVER_PID=$!
    sleep 1
    page=$(curl -s --max-time 5 http://127.0.0.1:$PORT/)
    if printf '%s' "$page" | grep -qF "Interactive Compiler Explorer"; then
        record T54 "Web: server serves the HTML page" "Web interface" "curl http://127.0.0.1:$PORT/" "page contains the title" "page contains the title" PASS
    else
        record T54 "Web: server serves the HTML page" "Web interface" "curl http://127.0.0.1:$PORT/" "page contains the title" "(not found)" FAIL
    fi
    resp=$(printf '\nLET x = 4\nPRINT x * 2\n' | curl -s --max-time 5 -X POST --data-binary @- http://127.0.0.1:$PORT/compile)
    if printf '%s' "$resp" | grep -qF '"success":true'; then
        record T55 "Web: POST /compile returns JSON (success)" "Web interface" "curl -X POST --data-binary ... /compile" '"success":true' '"success":true' PASS
    else
        record T55 "Web: POST /compile returns JSON (success)" "Web interface" "curl -X POST --data-binary ... /compile" '"success":true' "(not found)" FAIL
    fi
    resp=$(printf '\nLET x = 5 @\n' | curl -s --max-time 5 -X POST --data-binary @- http://127.0.0.1:$PORT/compile)
    if printf '%s' "$resp" | grep -qF '"success":false' && printf '%s' "$resp" | grep -qF "Invalid character"; then
        record T56 "Web: POST /compile returns JSON (lexical error)" "Web interface" "curl -X POST --data-binary ... /compile" '"success":false + Invalid character' '"success":false + Invalid character' PASS
    else
        record T56 "Web: POST /compile returns JSON (lexical error)" "Web interface" "curl -X POST --data-binary ... /compile" '"success":false + Invalid character' "(not found)" FAIL
    fi
    code=$(curl -s -o /dev/null -w '%{http_code}' --max-time 5 http://127.0.0.1:$PORT/nothing)
    if [ "$code" = "404" ]; then record T57 "Web: unknown page gives HTTP 404" "Web interface" "curl http://127.0.0.1:$PORT/nothing" "HTTP 404" "HTTP $code" PASS
    else record T57 "Web: unknown page gives HTTP 404" "Web interface" "curl http://127.0.0.1:$PORT/nothing" "HTTP 404" "HTTP $code" FAIL; fi
    kill $SERVER_PID 2> /dev/null
    wait $SERVER_PID 2> /dev/null
else
    echo "curl not installed: web tests skipped (sudo apt install curl)"
fi

echo
echo "Passed: $PASS    Failed: $FAIL    (details in $RESULTS)"
[ "$FAIL" -eq 0 ]
