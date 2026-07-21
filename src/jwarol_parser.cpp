#include "jwarol_parser.h"
#include <cstdio>
#include <algorithm>

Parser::Parser(const std::vector<Token>& tokens)
    : toks(tokens), pos(0) {}

const Token& Parser::peek() const {
    static Token eof{TT::ENDMARKER, "", 0, 0};
    return pos < toks.size() ? toks[pos] : eof;
}

const Token& Parser::advance() {
    if (pos < toks.size()) return toks[pos++];
    static Token eof{TT::ENDMARKER, "", 0, 0};
    return eof;
}

bool Parser::match(TT type) {
    if (peek().type == type) { advance(); return true; }
    return false;
}

bool Parser::check(TT type) const { return peek().type == type; }

const Token& Parser::expect(TT type, const std::string& msg) {
    if (peek().type == type) return advance();
    error(msg + " at " + std::to_string(peek().line) + ":" + std::to_string(peek().col) + ", got '" + peek().value + "'");
    return peek();
}

void Parser::error(const std::string& msg) {
    errs.push_back(msg);
}

void Parser::skip_to_newline() {
    while (pos < toks.size() && peek().type != TT::NEWLINE && peek().type != TT::ENDMARKER)
        advance();
    if (pos < toks.size() && peek().type == TT::NEWLINE) advance();
}

bool Parser::has_errors() const { return !errs.empty(); }
const std::vector<std::string>& Parser::errors() const { return errs; }

int Parser::precedence(TT t) {
    switch (t) {
        case TT::ASSIGN: case TT::PLUS_EQ: case TT::MINUS_EQ:
        case TT::STAR_EQ: case TT::SLASH_EQ: case TT::DSLASH_EQ:
        case TT::MOD_EQ: case TT::POW_EQ:
            return 1;
        case TT::KW_OR:  return 2;
        case TT::KW_AND: return 3;
        case TT::EQ: case TT::NEQ: case TT::LT: case TT::GT:
        case TT::LE: case TT::GE: case TT::KW_IN: case TT::KW_IS:
            return 5;
        case TT::PIPE:  return 6;
        case TT::CARET: return 7;
        case TT::AMP:   return 8;
        case TT::LSHIFT: case TT::RSHIFT: return 9;
        case TT::PLUS: case TT::MINUS:    return 10;
        case TT::STAR: case TT::SLASH: case TT::DSLASH: case TT::MOD: return 11;
        case TT::POW:   return 13;
        default: return -1;
    }
}

ASTNode* Parser::parse() {
    return parse_file();
}

ASTNode* Parser::parse_file() {
    auto* node = new ASTNode(NodeType::NONE, 1, 0);
    while (peek().type != TT::ENDMARKER) {
        if (peek().type == TT::NEWLINE) { advance(); continue; }
        auto* s = parse_stmt();
        if (s) node->add(s);
    }
    return node;
}

ASTNode* Parser::parse_stmt() {
    switch (peek().type) {
        case TT::KW_IF:     return parse_if();
        case TT::KW_WHILE:  return parse_while();
        case TT::KW_FOR:    return parse_for();
        case TT::KW_DEF:    return parse_def();
        case TT::KW_CLASS:  return parse_class();
        case TT::KW_RETURN: return parse_return();
        case TT::KW_IMPORT: return parse_import();
        case TT::KW_FROM:   return parse_from_import();
        case TT::KW_BREAK:  { auto& t = advance(); return new ASTNode(NodeType::BREAK, t.line, t.col); }
        case TT::KW_CONTINUE: { auto& t = advance(); return new ASTNode(NodeType::CONTINUE, t.line, t.col); }
        case TT::KW_PASS:   { auto& t = advance(); return new ASTNode(NodeType::PASS, t.line, t.col); }
        case TT::KW_INT:
        case TT::KW_FLOAT:
        case TT::KW_STR:    return parse_ann_assign();
        default: return parse_simple_stmt();
    }
}

ASTNode* Parser::parse_simple_stmt() {
    auto* expr_node = parse_expr();

    if (match(TT::ASSIGN)) {
        auto* node = new ASTNode(NodeType::ASSIGN, expr_node->line, expr_node->col);
        node->add(expr_node);
        node->add(parse_expr());
        while (match(TT::ASSIGN)) node->add(parse_expr());
        match(TT::SEMI);
        return node;
    }

    TT aug_ops[] = {TT::PLUS_EQ, TT::MINUS_EQ, TT::STAR_EQ, TT::SLASH_EQ, TT::DSLASH_EQ, TT::MOD_EQ, TT::POW_EQ};
    for (TT op : aug_ops) {
        if (match(op)) {
            auto* node = new ASTNode(NodeType::AUG_ASSIGN, expr_node->line, expr_node->col);
            node->set_op(op);
            node->add(expr_node);
            node->add(parse_expr());
            match(TT::SEMI);
            return node;
        }
    }

    auto* node = new ASTNode(NodeType::EXPR_STMT, expr_node->line, expr_node->col);
    node->add(expr_node);
    match(TT::SEMI);
    return node;
}

ASTNode* Parser::parse_compound_stmt() {
    switch (peek().type) {
        case TT::KW_IF:    return parse_if();
        case TT::KW_WHILE: return parse_while();
        case TT::KW_FOR:   return parse_for();
        case TT::KW_DEF:   return parse_def();
        case TT::KW_CLASS: return parse_class();
        default: return parse_simple_stmt();
    }
}

ASTNode* Parser::parse_if() {
    auto& t = advance();
    auto* node = new ASTNode(NodeType::IF, t.line, t.col);
    node->add(parse_expr());
    expect(TT::COLON, "expected ':'");
    match(TT::NEWLINE);
    expect(TT::INDENT, "expected indented block");
    while (!check(TT::DEDENT) && peek().type != TT::ENDMARKER) {
        if (match(TT::NEWLINE)) continue;
        node->add(parse_stmt());
    }
    match(TT::DEDENT);

    while (match(TT::KW_ELIF)) {
        auto* elif_test = parse_expr();
        expect(TT::COLON, "expected ':'");
        match(TT::NEWLINE);
        expect(TT::INDENT, "expected indented block");
        auto* elif_block = new ASTNode(NodeType::IF, peek().line, peek().col);
        elif_block->add(elif_test);
        while (!check(TT::DEDENT) && peek().type != TT::ENDMARKER) {
            if (match(TT::NEWLINE)) continue;
            elif_block->add(parse_stmt());
        }
        match(TT::DEDENT);
        node->add(elif_block);
    }

    if (match(TT::KW_ELSE)) {
        expect(TT::COLON, "expected ':'");
        match(TT::NEWLINE);
        expect(TT::INDENT, "expected indented block");
        auto* else_block = new ASTNode(NodeType::ELSE, peek().line, peek().col);
        while (!check(TT::DEDENT) && peek().type != TT::ENDMARKER) {
            if (match(TT::NEWLINE)) continue;
            else_block->add(parse_stmt());
        }
        match(TT::DEDENT);
        node->add(else_block);
    }

    return node;
}

ASTNode* Parser::parse_while() {
    auto& t = advance();
    auto* node = new ASTNode(NodeType::WHILE, t.line, t.col);
    node->add(parse_expr());
    expect(TT::COLON, "expected ':'");
    match(TT::NEWLINE);
    expect(TT::INDENT, "expected indented block");
    while (!check(TT::DEDENT) && peek().type != TT::ENDMARKER) {
        if (match(TT::NEWLINE)) continue;
        node->add(parse_stmt());
    }
    match(TT::DEDENT);
    return node;
}

ASTNode* Parser::parse_for() {
    auto& t = advance();
    auto* node = new ASTNode(NodeType::FOR, t.line, t.col);
    auto& tt = expect(TT::IDENT, "expected variable name");
    auto* target = new ASTNode(NodeType::IDENT, tt.line, tt.col);
    target->set_name(tt.value);
    expect(TT::KW_IN, "expected 'in'");
    auto* iter = parse_expr();
    node->add(target);
    node->add(iter);
    expect(TT::COLON, "expected ':'");
    match(TT::NEWLINE);
    expect(TT::INDENT, "expected indented block");
    while (!check(TT::DEDENT) && peek().type != TT::ENDMARKER) {
        if (match(TT::NEWLINE)) continue;
        node->add(parse_stmt());
    }
    match(TT::DEDENT);
    return node;
}

ASTNode* Parser::parse_def() {
    auto& t = advance();
    auto* node = new ASTNode(NodeType::DEF, t.line, t.col);
    node->set_name(expect(TT::IDENT, "expected function name").value);
    expect(TT::LPAREN, "expected '('");
    while (!check(TT::RPAREN) && peek().type != TT::ENDMARKER) {
        node->names.push_back(expect(TT::IDENT, "expected parameter name").value);
        if (!match(TT::COMMA)) break;
    }
    expect(TT::RPAREN, "expected ')'");
    expect(TT::COLON, "expected ':'");
    match(TT::NEWLINE);
    expect(TT::INDENT, "expected indented block");
    while (!check(TT::DEDENT) && peek().type != TT::ENDMARKER) {
        if (match(TT::NEWLINE)) continue;
        node->add(parse_stmt());
    }
    match(TT::DEDENT);
    return node;
}

ASTNode* Parser::parse_class() {
    auto& t = advance();
    auto* node = new ASTNode(NodeType::CLASS, t.line, t.col);
    node->set_name(expect(TT::IDENT, "expected class name").value);
    if (match(TT::LPAREN)) {
        while (!check(TT::RPAREN) && peek().type != TT::ENDMARKER) {
            node->add(parse_expr());
            if (!match(TT::COMMA)) break;
        }
        expect(TT::RPAREN, "expected ')'");
    }
    expect(TT::COLON, "expected ':'");
    match(TT::NEWLINE);
    expect(TT::INDENT, "expected indented block");
    while (!check(TT::DEDENT) && peek().type != TT::ENDMARKER) {
        if (match(TT::NEWLINE)) continue;
        node->add(parse_stmt());
    }
    match(TT::DEDENT);
    return node;
}

ASTNode* Parser::parse_return() {
    auto& t = advance();
    auto* node = new ASTNode(NodeType::RETURN, t.line, t.col);
    if (!check(TT::NEWLINE) && !check(TT::ENDMARKER) && !check(TT::DEDENT))
        node->add(parse_expr());
    return node;
}

ASTNode* Parser::parse_import() {
    auto& t = advance();
    auto* node = new ASTNode(NodeType::IMPORT, t.line, t.col);
    do {
        std::string name = expect(TT::IDENT, "expected module name").value;
        while (match(TT::DOT)) name += "." + expect(TT::IDENT, "expected name").value;
        std::string alias = name;
        if (match(TT::KW_AS)) alias = expect(TT::IDENT, "expected alias").value;
        node->names.push_back(name);
        node->names.push_back(alias);
    } while (match(TT::COMMA));
    return node;
}

ASTNode* Parser::parse_from_import() {
    auto& t = advance();
    auto* node = new ASTNode(NodeType::FROM_IMPORT, t.line, t.col);
    std::string module;
    if (match(TT::DOT)) module = ".";
    module += expect(TT::IDENT, "expected module name").value;
    while (match(TT::DOT)) module += "." + expect(TT::IDENT, "expected name").value;
    node->str_val = module;
    expect(TT::KW_IMPORT, "expected 'import'");
    do {
        std::string name = expect(TT::IDENT, "expected name").value;
        std::string alias = name;
        if (match(TT::KW_AS)) alias = expect(TT::IDENT, "expected alias").value;
        node->names.push_back(name);
        node->names.push_back(alias);
    } while (match(TT::COMMA));
    return node;
}

ASTNode* Parser::parse_expr(int min_prec) {
    auto* left = parse_unary();

    while (true) {
        TT op = peek().type;
        int prec = precedence(op);
        if (prec < min_prec) break;

        if (op == TT::ASSIGN || (op >= TT::PLUS_EQ && op <= TT::POW_EQ)) {
            break;
        }

        advance();

        if (op == TT::KW_AND || op == TT::KW_OR) {
            auto* right = parse_expr(prec + 1);
            auto* node = new ASTNode(NodeType::BOOLOP, left->line, left->col);
            node->set_op(op);
            node->add(left);
            node->add(right);
            left = node;
        } else if (op == TT::EQ || op == TT::NEQ || op == TT::LT || op == TT::GT ||
                   op == TT::LE || op == TT::GE || op == TT::KW_IN || op == TT::KW_IS) {
            auto* right = parse_expr(prec + 1);
            auto* node = new ASTNode(NodeType::COMPARE, left->line, left->col);
            node->set_op(op);
            node->add(left);
            node->add(right);
            left = node;
        } else {
            int right_prec = (op == TT::POW) ? prec - 1 : prec;
            auto* right = parse_expr(right_prec + 1);
            auto* node = new ASTNode(NodeType::BINOP, left->line, left->col);
            node->set_op(op);
            node->add(left);
            node->add(right);
            left = node;
        }
    }

    if (match(TT::KW_IF)) {
        auto* cond = parse_expr();
        expect(TT::KW_ELSE, "expected 'else' in conditional expression");
        auto* orelse = parse_expr();
        auto* node = new ASTNode(NodeType::IF, left->line, left->col);
        node->add(left);
        node->add(cond);
        node->add(orelse);
        return node;
    }

    return left;
}

ASTNode* Parser::parse_unary() {
    if (check(TT::MINUS) || check(TT::PLUS) || check(TT::TILDE)) {
        auto& t = advance();
        auto* operand = parse_unary();
        auto* node = new ASTNode(NodeType::UNARYOP, t.line, t.col);
        node->set_op(t.type);
        node->add(operand);
        return node;
    }
    if (match(TT::KW_NOT)) {
        auto* operand = parse_unary();
        auto* node = new ASTNode(NodeType::UNARYOP, operand->line, operand->col);
        node->set_op(TT::KW_NOT);
        node->add(operand);
        return node;
    }
    return parse_primary();
}

ASTNode* Parser::parse_primary() {
    auto& t = peek();

    if (check(TT::INT)) {
        advance();
        auto* node = new ASTNode(NodeType::INT_LIT, t.line, t.col);
        std::string v = t.value;
        v.erase(std::remove_if(v.begin(), v.end(), [](char c){ return c == '_'; }), v.end());
        if (v.size() > 2 && (v[1] == 'x' || v[1] == 'X'))
            node->set_int(std::stoll(v, nullptr, 16));
        else if (v.size() > 2 && (v[1] == 'o' || v[1] == 'O'))
            node->set_int(std::stoll(v, nullptr, 8));
        else if (v.size() > 2 && (v[1] == 'b' || v[1] == 'B'))
            node->set_int(std::stoll(v, nullptr, 2));
        else
            node->set_int(std::stoll(v));
        return node;
    }

    if (check(TT::FLOAT)) {
        advance();
        auto* node = new ASTNode(NodeType::FLOAT_LIT, t.line, t.col);
        std::string v = t.value;
        v.erase(std::remove_if(v.begin(), v.end(), [](char c){ return c == '_'; }), v.end());
        node->set_float(std::stod(v));
        return node;
    }

    if (check(TT::STRING)) {
        advance();
        auto* node = new ASTNode(NodeType::STRING_LIT, t.line, t.col);
        node->set_str(t.value);
        return node;
    }

    if (check(TT::KW_TRUE)) { advance(); auto* n = new ASTNode(NodeType::BOOL_LIT, t.line, t.col); n->set_bool(true); return n; }
    if (check(TT::KW_FALSE)) { advance(); auto* n = new ASTNode(NodeType::BOOL_LIT, t.line, t.col); n->set_bool(false); return n; }
    if (check(TT::KW_NONE)) { advance(); return new ASTNode(NodeType::NONE, t.line, t.col); }

    if (check(TT::IDENT)) {
        advance();
        auto* node = new ASTNode(NodeType::IDENT, t.line, t.col);
        node->set_name(t.value);
        return parse_trailer(node);
    }

    if (check(TT::LPAREN)) {
        advance();
        if (match(TT::RPAREN)) {
            return new ASTNode(NodeType::TUPLE, t.line, t.col);
        }
        auto* first = parse_expr();
        if (match(TT::COMMA)) {
            auto* tuple = new ASTNode(NodeType::TUPLE, t.line, t.col);
            tuple->add(first);
            if (!check(TT::RPAREN)) {
                do { tuple->add(parse_expr()); } while (match(TT::COMMA));
            }
            expect(TT::RPAREN, "expected ')'");
            return tuple;
        }
        expect(TT::RPAREN, "expected ')'");
        return first;
    }

    if (check(TT::LBRACKET)) {
        advance();
        auto* node = new ASTNode(NodeType::LIST, t.line, t.col);
        if (!check(TT::RBRACKET)) {
            do { node->add(parse_expr()); } while (match(TT::COMMA));
        }
        expect(TT::RBRACKET, "expected ']'");
        return node;
    }

    if (check(TT::LBRACE)) {
        advance();
        auto* node = new ASTNode(NodeType::DICT, t.line, t.col);
        if (!check(TT::RBRACE)) {
            do {
                auto* key = parse_expr();
                expect(TT::COLON, "expected ':'");
                auto* val = parse_expr();
                node->add(key);
                node->add(val);
            } while (match(TT::COMMA));
        }
        expect(TT::RBRACE, "expected '}'");
        return node;
    }

    error("unexpected token '" + t.value + "' at " + std::to_string(t.line) + ":" + std::to_string(t.col));
    advance();
    return new ASTNode(NodeType::NONE, t.line, t.col);
}

ASTNode* Parser::parse_ann_assign() {
    auto& type_tok = advance();
    auto* node = new ASTNode(NodeType::ANN_ASSIGN, type_tok.line, type_tok.col);
    node->str_val = type_tok.value;
    node->name = expect(TT::IDENT, "expected variable name").value;
    if (match(TT::ASSIGN)) {
        node->add(parse_expr());
    }
    match(TT::SEMI);
    return node;
}

ASTNode* Parser::parse_trailer(ASTNode* base) {
    while (true) {
        if (match(TT::LPAREN)) {
            auto* call = new ASTNode(NodeType::CALL, base->line, base->col);
            call->add(base);
            if (!check(TT::RPAREN)) {
                do { call->add(parse_expr()); } while (match(TT::COMMA));
            }
            expect(TT::RPAREN, "expected ')'");
            base = call;
        } else if (match(TT::DOT)) {
            auto* attr = new ASTNode(NodeType::ATTRIBUTE, base->line, base->col);
            attr->add(base);
            attr->set_name(expect(TT::IDENT, "expected attribute name").value);
            base = attr;
        } else if (match(TT::LBRACKET)) {
            auto* sub = new ASTNode(NodeType::SUBSCRIPT, base->line, base->col);
            sub->add(base);
            if (check(TT::COLON)) {
                advance();
                auto* sl = new ASTNode(NodeType::SLICE, base->line, base->col);
                sl->add(new ASTNode(NodeType::NONE));
                if (!check(TT::RBRACKET)) sl->add(parse_expr());
                else sl->add(new ASTNode(NodeType::NONE));
                sub->add(sl);
            } else {
                auto* first = parse_expr();
                if (match(TT::COLON)) {
                    auto* sl = new ASTNode(NodeType::SLICE, base->line, base->col);
                    sl->add(first);
                    if (!check(TT::RBRACKET)) sl->add(parse_expr());
                    else sl->add(new ASTNode(NodeType::NONE));
                    sub->add(sl);
                } else {
                    sub->add(first);
                }
            }
            expect(TT::RBRACKET, "expected ']'");
            base = sub;
        } else {
            break;
        }
    }
    return base;
}
