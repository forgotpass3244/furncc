
#ifndef LEXER_H
#define LEXER_H

#include <stdlib.h>

typedef enum
{
    TOK_VOID,
    TOK_INT,
    TOK_CHAR,
    TOK_STAR,
    TOK_IDENT,
    TOK_OPAREN,
    TOK_CPAREN,
    TOK_OBRACE,
    TOK_CBRACE,
    TOK_COMMA,
    TOK_RETURN,
    TOK_NUMBERLIT,
    TOK_STRINGLIT,
    TOK_CHARLIT,
    TOK_SEMICOLON,
    TOK_EQUAL,
    TOK_WHILE,
    TOK_AMPERSAND,
    TOK_PLUS,
    TOK_PLUSPLUS,
    TOK_SLASH,
    TOK_OANGLE,
    TOK_CANGLE,
} TokType;

typedef struct TokNode TokNode;

struct TokNode
{
    TokType Type;
    char *String;
    TokNode *Next;
};

typedef struct
{
    char *Input;
    size_t Pos;
    TokNode *Tokens;
} Lexer;

void Lexer_Tokenize(Lexer *Lex);

void TokNode_FreeAll(TokNode *List);

#endif // LEXER_H
