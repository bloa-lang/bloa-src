#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"

typedef enum {
    EXPR_LITERAL,
    EXPR_UNARY,
    EXPR_BINARY,
    EXPR_GROUP,
    EXPR_VARIABLE,
    EXPR_ASSIGN
} ExprType;

typedef struct Expr {
    ExprType type;
    union {
        Value literal;
        struct {
            Token op;
            struct Expr* right;
        } unary;
        struct {
            struct Expr* left;
            Token op;
            struct Expr* right;
        } binary;
        struct Expr* group;
        Token variable;
        struct {
            Token name;
            struct Expr* value;
        } assign;
    } data;
} Expr;

typedef enum {
    STMT_EXPR,
    STMT_PRINT,
    STMT_VAR
} StmtType;

typedef struct Stmt {
    StmtType type;
    union {
        Expr* expression;
        Expr* print;
        struct {
            Token name;
            Expr* initializer;
        } var;
    } data;
} Stmt;

typedef struct {
    Token current;
    Token previous;
    bool had_error;
    bool panic_mode;
} Parser;

void init_parser(void);
Stmt* parse(void);

#endif
