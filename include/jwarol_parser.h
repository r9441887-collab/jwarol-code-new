#pragma once

#include "jwarol_lexer.h"
#include <vector>
#include <string>
#include <algorithm>

enum class NodeType {
    NONE,
    INT_LIT, FLOAT_LIT, STRING_LIT, BOOL_LIT,
    IDENT,
    BINOP, UNARYOP, COMPARE, BOOLOP,
    CALL, ATTRIBUTE, SUBSCRIPT,
    LIST, DICT, TUPLE,
    ASSIGN, AUG_ASSIGN,
    EXPR_STMT, RETURN, IF, WHILE, FOR,
    BREAK, CONTINUE, PASS,
    DEF, CLASS,
    IMPORT, FROM_IMPORT,
    ANN_ASSIGN,
    ELSE,
    SLICE,
};

struct ASTNode {
    NodeType type;
    int line, col;
    int64_t int_val;
    double float_val;
    std::string str_val;
    bool bool_val;
    TT op;
    std::vector<ASTNode*> children;
    std::vector<std::string> names;
    std::string name;

    ASTNode(NodeType t, int l = 0, int c = 0)
        : type(t), line(l), col(c), int_val(0), float_val(0),
          bool_val(false), op(TT::ENDMARKER) {}
    ~ASTNode() { for (auto* c : children) delete c; }

    ASTNode* add(ASTNode* c) { children.push_back(c); return this; }
    ASTNode* set_name(const std::string& n) { name = n; return this; }
    ASTNode* set_int(int64_t v) { int_val = v; return this; }
    ASTNode* set_float(double v) { float_val = v; return this; }
    ASTNode* set_str(const std::string& v) { str_val = v; return this; }
    ASTNode* set_bool(bool v) { bool_val = v; return this; }
    ASTNode* set_op(TT o) { op = o; return this; }
};

class Parser {
public:
    Parser(const std::vector<Token>& tokens);
    ASTNode* parse();
    bool has_errors() const;
    const std::vector<std::string>& errors() const;

private:
    const std::vector<Token>& toks;
    size_t pos;
    std::vector<std::string> errs;

    const Token& peek() const;
    const Token& advance();
    bool match(TT type);
    bool check(TT type) const;
    const Token& expect(TT type, const std::string& msg);
    void error(const std::string& msg);
    void skip_to_newline();

    ASTNode* parse_file();
    ASTNode* parse_stmt();
    ASTNode* parse_simple_stmt();
    ASTNode* parse_compound_stmt();

    ASTNode* parse_if();
    ASTNode* parse_while();
    ASTNode* parse_for();
    ASTNode* parse_def();
    ASTNode* parse_class();
    ASTNode* parse_return();
    ASTNode* parse_import();
    ASTNode* parse_from_import();
    ASTNode* parse_ann_assign();

    ASTNode* parse_expr(int min_prec = 0);
    ASTNode* parse_unary();
    ASTNode* parse_primary();
    ASTNode* parse_trailer(ASTNode* base);

    static int precedence(TT t);
};
