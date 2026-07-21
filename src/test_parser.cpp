#include <cstdio>
#include <string>
#include <fstream>
#include <sstream>
#include "jwarol_lexer.h"
#include "jwarol_parser.h"

static void print_ast(ASTNode* n, int depth = 0) {
    if (!n) return;
    for (int i = 0; i < depth; i++) printf("  ");
    switch (n->type) {
        case NodeType::INT_LIT:    printf("INT(%ld)\n", n->int_val); break;
        case NodeType::FLOAT_LIT:  printf("FLOAT(%g)\n", n->float_val); break;
        case NodeType::STRING_LIT: printf("STRING(\"%s\")\n", n->str_val.c_str()); break;
        case NodeType::BOOL_LIT:   printf("BOOL(%s)\n", n->bool_val ? "True" : "False"); break;
        case NodeType::NONE:       printf("NONE\n"); break;
        case NodeType::IDENT:      printf("IDENT(%s)\n", n->name.c_str()); break;
        case NodeType::BINOP:      printf("BINOP(%d)\n", (int)n->op); break;
        case NodeType::UNARYOP:    printf("UNARYOP(%d)\n", (int)n->op); break;
        case NodeType::COMPARE:    printf("COMPARE\n"); break;
        case NodeType::BOOLOP:     printf("BOOLOP(%d)\n", (int)n->op); break;
        case NodeType::CALL:       printf("CALL\n"); break;
        case NodeType::ATTRIBUTE:  printf("ATTR(%s)\n", n->name.c_str()); break;
        case NodeType::SUBSCRIPT:  printf("SUBSCRIPT\n"); break;
        case NodeType::LIST:       printf("LIST\n"); break;
        case NodeType::DICT:       printf("DICT\n"); break;
        case NodeType::TUPLE:      printf("TUPLE\n"); break;
        case NodeType::ASSIGN:     printf("ASSIGN\n"); break;
        case NodeType::AUG_ASSIGN: printf("AUG_ASSIGN(%d)\n", (int)n->op); break;
        case NodeType::EXPR_STMT:  printf("EXPR_STMT\n"); break;
        case NodeType::RETURN:     printf("RETURN\n"); break;
        case NodeType::IF:         printf("IF\n"); break;
        case NodeType::WHILE:      printf("WHILE\n"); break;
        case NodeType::FOR:        printf("FOR\n"); break;
        case NodeType::BREAK:      printf("BREAK\n"); break;
        case NodeType::CONTINUE:   printf("CONTINUE\n"); break;
        case NodeType::PASS:       printf("PASS\n"); break;
        case NodeType::DEF:        printf("DEF(%s) args=[", n->name.c_str()); for (size_t i=0;i<n->names.size();i++){if(i)printf(",");printf("%s",n->names[i].c_str());} printf("]\n"); break;
        case NodeType::CLASS:      printf("CLASS(%s)\n", n->name.c_str()); break;
        case NodeType::IMPORT:     printf("IMPORT\n"); break;
        case NodeType::FROM_IMPORT: printf("FROM_IMPORT(%s)\n", n->str_val.c_str()); break;
        default: printf("NODE(%d)\n", (int)n->type); break;
    }
    for (auto* c : n->children) print_ast(c, depth + 1);
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
y = 3.14 + 2.0 * 3
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

class Foo:
    def __init__(self, x):
        self.x = x
)";
    }

    Lexer lexer(code, argc > 1 ? argv[1] : "test.py");
    auto tokens = lexer.tokenize();

    if (lexer.has_errors()) {
        for (auto& e : lexer.errors()) fprintf(stderr, "Lexer: %s\n", e.c_str());
        return 1;
    }

    Parser parser(tokens);
    ASTNode* ast = parser.parse();

    if (parser.has_errors()) {
        for (auto& e : parser.errors()) fprintf(stderr, "Parser: %s\n", e.c_str());
        return 1;
    }

    print_ast(ast);
    delete ast;
    return 0;
}
