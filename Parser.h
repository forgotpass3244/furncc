
#ifndef PARSER_H
#define PARSER_H

#include "Lexer.h"
#include <stdbool.h>

typedef enum
{
    TYPE_NOT_A_TYPE,
    TYPE_VOID,
    TYPE_INT,
    TYPE_CHAR,
    TYPE_STRUCT,
} TypeType; // lol

typedef struct
{
    TypeType Type;
    size_t PointerDepth;
} TypeDesc;

typedef enum
{
    STMT_EXPR,
    STMT_FUNC,
    STMT_RETURN,
    STMT_VARDECL,
    STMT_WHILE,
} StmtType;

typedef enum
{
    EXPR_NUMBERLIT,
    EXPR_CHARLIT,
    EXPR_STRINGLIT,
    EXPR_IDENT,
    EXPR_CALL,
    EXPR_ASSIGN, // add post_assign later
    EXPR_ADDRESSOF,
    EXPR_DEREF,
    EXPR_INC,
    EXPR_BINARYOP,
} ExprType;

typedef enum
{
    OP_ADD,
    OP_LESSTHAN,
} OpType;

typedef struct Expr_t Expr_t;

struct Expr_t
{
    ExprType Type;

    union
    {
        unsigned int NumberLit;
        char CharLit;
        const char *StringLit;
        const char *Ident;
        struct 
        {
            Expr_t *Callee;
            Expr_t *Arguments[6];
        } Call;
        struct
        {
            Expr_t *Target;
            Expr_t *Expr;
        } Assign;
        Expr_t *AddressOf;
        Expr_t *Deref;
        Expr_t *Inc;
        struct
        {
            OpType Op;
            Expr_t *A;
            Expr_t *B;
        } BinaryOp;
    } As;
};

typedef struct StmtNode StmtNode;

struct StmtNode
{
    StmtType Type;

    union
    {
        Expr_t *Expr;
        
        struct
        {
            char *Name;
            StmtNode *Body;

            struct
            {
                const char *Name;
                TypeDesc Type;
            } Params[6];

            TypeDesc ReturnType;
        } Func;

        Expr_t *Return;

        struct
        {
            const char *Name;
            TypeDesc Type;
            Expr_t *Init;
        } VarDecl;

        struct
        {
            Expr_t *Condition;
            StmtNode *Body;
        } While;
    } As;

    StmtNode *Next;
};

typedef struct
{
    TokNode *Tok; // current token
    StmtNode *Ast;
} Parser;

void StmtNode_FreeAllRecursive(StmtNode *List);

void Expr_FreeRecursive(Expr_t *Expr);

void Parser_Parse(Parser *Parse);

StmtNode *Parser_ParseStmt(Parser *Parse);

StmtNode *Parser_ParseFuncStmt(Parser *Parse, TypeDesc *ReturnType);

StmtNode *Parser_ParseVarDecl(Parser *Parse, TypeDesc *Type);

Expr_t *Parser_ParseExpr(Parser *Parse);

Expr_t *Parser_ParsePrimary(Parser *Parse);

Expr_t *Parser_ParseSecondary(Parser *Parse);

#endif // PARSER_H
