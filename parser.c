#include "bloat.h"
#include "lexer.h"
#include "parser.h"

static Parser parser;

static void error_at(Token* token, const char* message) {
    if (parser.panic_mode) return;
    parser.panic_mode = true;
    fprintf(stderr, "[line %d] Error", token->line);

    if (token->type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    } else if (token->type == TOKEN_ERROR) {
        // Nothing
    } else {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }

    fprintf(stderr, ": %s\n", message);
    parser.had_error = true;
}

static void error(const char* message) {
    error_at(&parser.previous, message);
}

static void advance() {
    parser.previous = parser.current;

    for (;;) {
        parser.current = scan_token();
        if (parser.current.type != TOKEN_ERROR) break;

        error_at(&parser.current, parser.current.start);
    }
}

static void consume(TokenType type, const char* message) {
    if (parser.current.type == type) {
        advance();
        return;
    }

    error_at(&parser.current, message);
}

static Expr* expression();
static Stmt* statement();
static Stmt* declaration();

static Expr* binary() {
    Expr* expr = unary();

    while (true) {
        Token operator = parser.current;
        switch (operator.type) {
            case TOKEN_BANG_EQUAL:
            case TOKEN_EQUAL_EQUAL:
            case TOKEN_GREATER:
            case TOKEN_GREATER_EQUAL:
            case TOKEN_LESS:
            case TOKEN_LESS_EQUAL:
            case TOKEN_PLUS:
            case TOKEN_MINUS:
            case TOKEN_STAR:
            case TOKEN_SLASH:
                advance();
                Expr* right = unary();
                Expr* new_expr = malloc(sizeof(Expr));
                new_expr->type = EXPR_BINARY;
                new_expr->binary.left = expr;
                new_expr->binary.op = operator;
                new_expr->binary.right = right;
                expr = new_expr;
                break;
            default:
                return expr;
        }
    }
}

static Stmt* parse_statement() {
    if (match(TOKEN_PRINT)) return print_statement();
    return expression_statement();
}

void init_parser() {
    parser.had_error = false;
    parser.panic_mode = false;
}

Stmt* parse() {
    init_parser();
    advance();
    return declaration();
}
