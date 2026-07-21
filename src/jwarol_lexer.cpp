#include "jwarol_lexer.h"
#include <cctype>
#include <unordered_map>

static const std::unordered_map<std::string, TT>& keywords() {
    static const std::unordered_map<std::string, TT> kw = {
        {"if", TT::KW_IF}, {"elif", TT::KW_ELIF}, {"else", TT::KW_ELSE},
        {"while", TT::KW_WHILE}, {"for", TT::KW_FOR}, {"in", TT::KW_IN},
        {"def", TT::KW_DEF}, {"class", TT::KW_CLASS},
        {"return", TT::KW_RETURN}, {"yield", TT::KW_YIELD},
        {"import", TT::KW_IMPORT}, {"from", TT::KW_FROM}, {"as", TT::KW_AS},
        {"with", TT::KW_WITH}, {"try", TT::KW_TRY}, {"except", TT::KW_EXCEPT},
        {"finally", TT::KW_FINALLY}, {"raise", TT::KW_RAISE}, {"assert", TT::KW_ASSERT},
        {"and", TT::KW_AND}, {"or", TT::KW_OR}, {"not", TT::KW_NOT},
        {"is", TT::KW_IS}, {"break", TT::KW_BREAK}, {"continue", TT::KW_CONTINUE},
        {"pass", TT::KW_PASS}, {"del", TT::KW_DEL},
        {"global", TT::KW_GLOBAL}, {"nonlocal", TT::KW_NONLOCAL},
        {"lambda", TT::KW_LAMBDA}, {"async", TT::KW_ASYNC}, {"await", TT::KW_AWAIT},
        {"True", TT::KW_TRUE}, {"False", TT::KW_FALSE}, {"None", TT::KW_NONE},
        {"int", TT::KW_INT}, {"float", TT::KW_FLOAT}, {"str", TT::KW_STR},
    };
    return kw;
}

Lexer::Lexer(const std::string& source, const std::string& fn)
    : src(source), filename(fn), pos(0), line(1), col(0),
      indent_stack({0}), at_line_start(true), paren_depth(0) {}

char Lexer::peek() const { return pos < src.size() ? src[pos] : '\0'; }
char Lexer::peek2() const { return pos + 1 < src.size() ? src[pos + 1] : '\0'; }

char Lexer::advance() {
    char c = src[pos++];
    if (c == '\n') { line++; col = 0; } else { col++; }
    return c;
}

void Lexer::emit(TT type, const std::string& value) {
    tokens.push_back({type, value, line, col});
}
void Lexer::emit(const Token& t) {
    tokens.push_back(t);
}

void Lexer::error(const std::string& msg) {
    errs.push_back(filename + ":" + std::to_string(line) + ":" + std::to_string(col) + ": " + msg);
}

bool Lexer::has_errors() const { return !errs.empty(); }
const std::vector<std::string>& Lexer::errors() const { return errs; }

void Lexer::skip_spaces() {
    while (pos < src.size()) {
        char c = src[pos];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\x0c' || c == '\x0b') {
            advance();
        } else {
            break;
        }
    }
}

Token Lexer::read_number() {
    size_t start = pos;
    int start_col = col;
    bool is_float = false;

    if (src[pos] == '0' && pos + 1 < src.size()) {
        char c2 = src[pos + 1];
        if (c2 == 'x' || c2 == 'X') {
            advance(); advance();
            while (pos < src.size() && isxdigit((unsigned char)src[pos])) advance();
            return {TT::INT, std::string(src, start, pos - start), line, start_col};
        }
        if (c2 == 'o' || c2 == 'O') {
            advance(); advance();
            while (pos < src.size() && src[pos] >= '0' && src[pos] <= '7') advance();
            return {TT::INT, std::string(src, start, pos - start), line, start_col};
        }
        if (c2 == 'b' || c2 == 'B') {
            advance(); advance();
            while (pos < src.size() && (src[pos] == '0' || src[pos] == '1')) advance();
            return {TT::INT, std::string(src, start, pos - start), line, start_col};
        }
    }

    while (pos < src.size() && (isdigit((unsigned char)src[pos]) || src[pos] == '_')) advance();

    if (pos < src.size() && src[pos] == '.') {
        char nxt = peek2();
        if (isdigit((unsigned char)nxt)) {
            advance();
            while (pos < src.size() && (isdigit((unsigned char)src[pos]) || src[pos] == '_')) advance();
            is_float = true;
        } else if ((nxt == 'e' || nxt == 'E') && pos + 2 < src.size()) {
            advance();
            is_float = true;
        } else if (isalpha((unsigned char)nxt) || nxt == '_' || nxt == '(' || nxt == '[' || nxt == '.') {
            // dot is method call / attribute access, not part of number
        } else {
            advance();
            is_float = true;
        }
    }

    if (pos < src.size() && (src[pos] == 'e' || src[pos] == 'E')) {
        is_float = true;
        advance();
        if (pos < src.size() && (src[pos] == '+' || src[pos] == '-')) advance();
        while (pos < src.size() && (isdigit((unsigned char)src[pos]) || src[pos] == '_')) advance();
    }

    return {is_float ? TT::FLOAT : TT::INT, std::string(src, start, pos - start), line, start_col};
}

Token Lexer::read_string(bool raw) {
    int start_line = line;
    int start_col = col;
    char quote = advance();

    bool triple = false;
    if (pos < src.size() && src[pos] == quote) {
        if (pos + 1 < src.size() && src[pos + 1] == quote) {
            triple = true;
            advance(); advance();
        } else {
            advance();
            return {TT::STRING, "", start_line, start_col};
        }
    }

    std::string value;
    while (pos < src.size()) {
        if (!triple && src[pos] == quote) {
            advance();
            return {TT::STRING, value, start_line, start_col};
        }
        if (triple && src[pos] == quote && pos + 1 < src.size() && src[pos + 1] == quote
            && pos + 2 < src.size() && src[pos + 2] == quote) {
            advance(); advance(); advance();
            return {TT::STRING, value, start_line, start_col};
        }
        if (src[pos] == '\\' && pos + 1 < src.size() && !raw) {
            advance();
            char esc = advance();
            switch (esc) {
                case 'n': value += '\n'; break;
                case 't': value += '\t'; break;
                case 'r': value += '\r'; break;
                case '\\': value += '\\'; break;
                case '\'': value += '\''; break;
                case '"': value += '"'; break;
                case '0': value += '\0'; break;
                case 'a': value += '\a'; break;
                case 'b': value += '\b'; break;
                case 'f': value += '\f'; break;
                case 'v': value += '\v'; break;
                case '\n': break;
                default: value += esc; break;
            }
        } else {
            if (src[pos] == '\n' && !triple) {
                error("unterminated string");
                return {TT::STRING, value, start_line, start_col};
            }
            value += src[pos];
            advance();
        }
    }

    error("unterminated string");
    return {TT::STRING, value, start_line, start_col};
}

Token Lexer::read_ident() {
    size_t start = pos;
    int start_col = col;
    while (pos < src.size() && (isalnum((unsigned char)src[pos]) || src[pos] == '_'))
        advance();
    std::string word(src, start, pos - start);

    if ((word == "r" || word == "b" || word == "rb" || word == "br" ||
         word == "f" || word == "fr" || word == "rf") &&
        pos < src.size() && (src[pos] == '\'' || src[pos] == '"')) {
        bool raw = (word.find('r') != std::string::npos);
        Token str = read_string(raw);
        str.value = word + str.value;
        return str;
    }

    auto& kw = keywords();
    auto it = kw.find(word);
    if (it != kw.end()) return {it->second, word, line, start_col};
    return {TT::IDENT, word, line, start_col};
}

void Lexer::process_line_start() {
    while (true) {
        int indent = 0;
        while (pos < src.size() && (src[pos] == ' ' || src[pos] == '\t')) {
            indent += (src[pos] == '\t') ? 8 : 1;
            advance();
        }

        if (pos >= src.size()) break;
        if (src[pos] == '\n') { advance(); continue; }
        if (pos < src.size() && src[pos] == '#') {
            while (pos < src.size() && src[pos] != '\n') advance();
            if (pos < src.size()) { advance(); line++; col = 0; }
            continue;
        }

        if (paren_depth > 0) { at_line_start = false; return; }

        if (indent > indent_stack.back()) {
            indent_stack.push_back(indent);
            emit(TT::INDENT, "");
        } else if (indent < indent_stack.back()) {
            while (indent_stack.back() > indent) {
                indent_stack.pop_back();
                emit(TT::DEDENT, "");
            }
            if (indent_stack.back() != indent) {
                error("unindent does not match any outer indentation level");
            }
        }
        at_line_start = false;
        return;
    }
}

std::vector<Token> Lexer::tokenize() {
    while (pos < src.size()) {
        if (at_line_start && paren_depth == 0) {
            process_line_start();
            if (pos >= src.size()) break;
        }

        char c = peek();

        if (c == '\\' && pos + 1 < src.size() && src[pos + 1] == '\n') {
            advance(); advance();
            continue;
        }

        if (c == '\n') {
            advance();
            if (paren_depth == 0) {
                emit(TT::NEWLINE, "\\n");
                at_line_start = true;
            }
            continue;
        }

        if (c == '\r') { advance(); continue; }

        skip_spaces();
        if (pos >= src.size()) break;
        c = peek();

        if (c == '#') {
            while (pos < src.size() && src[pos] != '\n') advance();
            continue;
        }

        if (c == '\'' || c == '"') {
            Token s = read_string();
            // check for string prefix (r, b, etc.)
            // already handled in read_ident if prefix precedes string
            emit(s.type, s.value);
            continue;
        }

        if (isdigit((unsigned char)c) || (c == '.' && isdigit((unsigned char)peek2()))) {
            emit(read_number());
            continue;
        }

        if (isalpha((unsigned char)c) || c == '_') {
            emit(read_ident());
            continue;
        }

        if (c == '(' || c == '[' || c == '{') paren_depth++;
        if (c == ')' || c == ']' || c == '}') {
            if (paren_depth > 0) paren_depth--;
        }

        advance();
        switch (c) {
            case '+':
                if (peek() == '=') { advance(); emit(TT::PLUS_EQ, "+="); }
                else emit(TT::PLUS, "+");
                break;
            case '-':
                if (peek() == '=') { advance(); emit(TT::MINUS_EQ, "-="); }
                else if (peek() == '>') { advance(); emit(TT::ARROW, "->"); }
                else emit(TT::MINUS, "-");
                break;
            case '*':
                if (peek() == '*') {
                    advance();
                    if (peek() == '=') { advance(); emit(TT::POW_EQ, "**="); }
                    else emit(TT::POW, "**");
                } else if (peek() == '=') { advance(); emit(TT::STAR_EQ, "*="); }
                else emit(TT::STAR, "*");
                break;
            case '/':
                if (peek() == '/') {
                    advance();
                    if (peek() == '=') { advance(); emit(TT::DSLASH_EQ, "//="); }
                    else emit(TT::DSLASH, "//");
                } else if (peek() == '=') { advance(); emit(TT::SLASH_EQ, "/="); }
                else emit(TT::SLASH, "/");
                break;
            case '%':
                if (peek() == '=') { advance(); emit(TT::MOD_EQ, "%="); }
                else emit(TT::MOD, "%");
                break;
            case '=':
                if (peek() == '=') { advance(); emit(TT::EQ, "=="); }
                else emit(TT::ASSIGN, "=");
                break;
            case '!':
                if (peek() == '=') { advance(); emit(TT::NEQ, "!="); }
                else error("unexpected character '!'");
                break;
            case '<':
                if (peek() == '=') { advance(); emit(TT::LE, "<="); }
                else if (peek() == '<') { advance();
                    if (peek() == '=') { advance(); emit(TT::LSHIFT, "<<="); }
                    else emit(TT::LSHIFT, "<<");
                }
                else emit(TT::LT, "<");
                break;
            case '>':
                if (peek() == '=') { advance(); emit(TT::GE, ">="); }
                else if (peek() == '>') { advance();
                    if (peek() == '=') { advance(); emit(TT::RSHIFT, ">>="); }
                    else emit(TT::RSHIFT, ">>");
                }
                else emit(TT::GT, ">");
                break;
            case '&': if (peek() == '=') { advance(); emit(TT::AMP, "&="); } else emit(TT::AMP, "&"); break;
            case '|': if (peek() == '=') { advance(); emit(TT::PIPE, "|="); } else emit(TT::PIPE, "|"); break;
            case '^': if (peek() == '=') { advance(); emit(TT::CARET, "^="); } else emit(TT::CARET, "^"); break;
            case '~': emit(TT::TILDE, "~"); break;
            case '@': emit(TT::AT, "@"); break;
            case ':': emit(TT::COLON, ":"); break;
            case ';': emit(TT::SEMI, ";"); break;
            case ',': emit(TT::COMMA, ","); break;
            case '.':
                if (peek() == '.' && peek2() == '.') {
                    advance(); advance();
                    emit(TT::ELLIPSIS, "...");
                } else {
                    emit(TT::DOT, ".");
                }
                break;
            case '(': emit(TT::LPAREN, "("); break;
            case ')': emit(TT::RPAREN, ")"); break;
            case '[': emit(TT::LBRACKET, "["); break;
            case ']': emit(TT::RBRACKET, "]"); break;
            case '{': emit(TT::LBRACE, "{"); break;
            case '}': emit(TT::RBRACE, "}"); break;
            default:
                error(std::string("unexpected character '") + c + "'");
                break;
        }
    }

    if (paren_depth > 0) {
        error("unexpected EOF, unmatched parenthesis");
    }

    while (indent_stack.back() > 0) {
        indent_stack.pop_back();
        emit(TT::DEDENT, "");
    }

    if (!tokens.empty() && tokens.back().type != TT::NEWLINE) {
        emit(TT::NEWLINE, "\\n");
    }
    emit(TT::ENDMARKER, "");

    return tokens;
}
