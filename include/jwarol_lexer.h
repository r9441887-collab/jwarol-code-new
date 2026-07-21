#pragma once

#include <string>
#include <vector>
#include <cstdint>

enum class TT {
    INT, FLOAT, STRING,
    IDENT,

    KW_IF, KW_ELIF, KW_ELSE, KW_WHILE, KW_FOR, KW_IN,
    KW_DEF, KW_CLASS, KW_RETURN, KW_YIELD,
    KW_IMPORT, KW_FROM, KW_AS, KW_WITH,
    KW_TRY, KW_EXCEPT, KW_FINALLY, KW_RAISE, KW_ASSERT,
    KW_AND, KW_OR, KW_NOT, KW_IS,
    KW_BREAK, KW_CONTINUE, KW_PASS, KW_DEL,
    KW_GLOBAL, KW_NONLOCAL, KW_LAMBDA,
    KW_ASYNC, KW_AWAIT,
    KW_TRUE, KW_FALSE, KW_NONE,
    KW_INT, KW_FLOAT, KW_STR,

    PLUS, MINUS, STAR, SLASH, DSLASH, MOD, POW,
    ASSIGN, EQ, NEQ, LT, GT, LE, GE,
    PLUS_EQ, MINUS_EQ, STAR_EQ, SLASH_EQ, DSLASH_EQ, MOD_EQ, POW_EQ,
    AMP, PIPE, CARET, TILDE, LSHIFT, RSHIFT,
    ARROW, AT, COLON, SEMI, COMMA, DOT, ELLIPSIS,

    LPAREN, RPAREN, LBRACKET, RBRACKET, LBRACE, RBRACE,

    NEWLINE, INDENT, DEDENT, ENDMARKER,
};

struct Token {
    TT type;
    std::string value;
    int line;
    int col;
};

class Lexer {
public:
    Lexer(const std::string& source, const std::string& filename = "<stdin>");
    std::vector<Token> tokenize();
    bool has_errors() const;
    const std::vector<std::string>& errors() const;

private:
    const std::string& src;
    std::string filename;
    size_t pos;
    int line, col;
    std::vector<int> indent_stack;
    std::vector<Token> tokens;
    std::vector<std::string> errs;
    bool at_line_start;
    int paren_depth;

    char peek() const;
    char peek2() const;
    char advance();
    void emit(TT type, const std::string& value = "");
    void emit(const Token& t);
    void error(const std::string& msg);

    void skip_spaces();
    Token read_number();
    Token read_string(bool raw = false);
    Token read_ident();
    void process_line_start();
};
