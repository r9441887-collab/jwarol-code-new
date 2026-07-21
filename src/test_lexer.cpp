#include <cstdio>
#include <string>
#include <fstream>
#include <sstream>
#include "jwarol_lexer.h"

static const char* tt_name(TT t) {
    switch (t) {
        case TT::INT: return "INT";
        case TT::FLOAT: return "FLOAT";
        case TT::STRING: return "STRING";
        case TT::IDENT: return "IDENT";
        case TT::KW_IF: return "IF";
        case TT::KW_ELIF: return "ELIF";
        case TT::KW_ELSE: return "ELSE";
        case TT::KW_WHILE: return "WHILE";
        case TT::KW_FOR: return "FOR";
        case TT::KW_IN: return "IN";
        case TT::KW_DEF: return "DEF";
        case TT::KW_CLASS: return "CLASS";
        case TT::KW_RETURN: return "RETURN";
        case TT::KW_TRUE: return "TRUE";
        case TT::KW_FALSE: return "FALSE";
        case TT::KW_NONE: return "NONE";
        case TT::KW_AND: return "AND";
        case TT::KW_OR: return "OR";
        case TT::KW_NOT: return "NOT";
        case TT::KW_BREAK: return "BREAK";
        case TT::KW_CONTINUE: return "CONTINUE";
        case TT::KW_PASS: return "PASS";
        case TT::PLUS: return "PLUS";
        case TT::MINUS: return "MINUS";
        case TT::STAR: return "STAR";
        case TT::SLASH: return "SLASH";
        case TT::DSLASH: return "DSLASH";
        case TT::MOD: return "MOD";
        case TT::POW: return "POW";
        case TT::ASSIGN: return "ASSIGN";
        case TT::EQ: return "EQ";
        case TT::NEQ: return "NEQ";
        case TT::LT: return "LT";
        case TT::GT: return "GT";
        case TT::LE: return "LE";
        case TT::GE: return "GE";
        case TT::PLUS_EQ: return "PLUS_EQ";
        case TT::MINUS_EQ: return "MINUS_EQ";
        case TT::STAR_EQ: return "STAR_EQ";
        case TT::AMP: return "AMP";
        case TT::PIPE: return "PIPE";
        case TT::ARROW: return "ARROW";
        case TT::COLON: return "COLON";
        case TT::COMMA: return "COMMA";
        case TT::DOT: return "DOT";
        case TT::ELLIPSIS: return "ELLIPSIS";
        case TT::LPAREN: return "LPAREN";
        case TT::RPAREN: return "RPAREN";
        case TT::LBRACKET: return "LBRACKET";
        case TT::RBRACKET: return "RBRACKET";
        case TT::LBRACE: return "LBRACE";
        case TT::RBRACE: return "RBRACE";
        case TT::NEWLINE: return "NEWLINE";
        case TT::INDENT: return "INDENT";
        case TT::DEDENT: return "DEDENT";
        case TT::ENDMARKER: return "ENDMARKER";
        default: return "?";
    }
}

int main(int argc, char** argv) {
    std::string code;
    if (argc > 1) {
        std::ifstream ifs(argv[1]);
        if (!ifs) { fprintf(stderr, "Cannot open %s\n", argv[1]); return 1; }
        std::stringstream ss;
        ss << ifs.rdbuf();
        code = ss.str();
    } else {
        code = R"(
x = 100
y = 3.14
name = "hello"
arr = [1, 2, 3]

if x > 50:
    puts("big")
elif x > 10:
    puts("medium")
else:
    puts("small")

i = 0
while i < 10:
    i += 1
    if i == 5:
        break

for i in range(10):
    print(i)

def add(a, b):
    return a + b

True
False
None
1_000_000
0xFF
0o77
0b1010
1.5e-3
...ellipsis...
)";
    }

    Lexer lexer(code, argc > 1 ? argv[1] : "test.py");
    auto tokens = lexer.tokenize();

    for (auto& t : tokens) {
        printf("%3d:%-2d %-12s %s\n", t.line, t.col, tt_name(t.type), t.value.c_str());
    }

    if (lexer.has_errors()) {
        printf("\nErrors:\n");
        for (auto& e : lexer.errors()) {
            printf("  %s\n", e.c_str());
        }
    }

    return 0;
}
