
#include "Lexer.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

static inline bool IsSpace(char c)
{
    return c == ' ' || c == '\n' || c == '\t';
}

static inline bool IsIdent(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c == '_');
}

static inline bool IsDigit(char c)
{
    return (c >= '0' && c <= '9');
}

TokNode *TokNode_New(TokType Type)
{
    TokNode *NewToken = malloc(sizeof(TokNode));
    NewToken->Type = Type;
    NewToken->Next = NULL;
    NewToken->String = NULL;
    return NewToken;
}

void TokNode_FreeAll(TokNode *List)
{
    TokNode *Node = List;
    while (Node)
    {
        TokNode *OldNode = Node;
        Node = OldNode->Next;
        free(OldNode->String);
        free(OldNode);
    }
}

void TokNode_Append(TokNode *List, TokNode *NewNode)
{
    TokNode *Node = List;
    while (Node->Next)
    {
        Node = Node->Next;
    }
    Node->Next = NewNode;
}

void Lexer_AppendToken(Lexer *Lex, TokNode *NewToken)
{
    if (Lex->Tokens == NULL)
    {
        Lex->Tokens = NewToken;
    }
    else
    {
        TokNode_Append(Lex->Tokens, NewToken);
    }
}

void Lexer_Tokenize(Lexer *Lex)
{
    size_t Length = strlen(Lex->Input);

    size_t *pPos = &Lex->Pos;
    for (*pPos = 0; *pPos < Length; ++(*pPos))
    {
        char c = Lex->Input[*pPos];

        if (IsIdent(c) || IsDigit(c))
        {
            TokNode *NewToken = TokNode_New(IsDigit(c) ? TOK_NUMBERLIT : TOK_IDENT);

            size_t Len = 1;
            // spaghetti loop >:)
            for (size_t c = Lex->Input[++(*pPos)];
                 IsIdent(c) || IsDigit(c);
                 c = Lex->Input[++(*pPos)])
            {
                Len++;
            }

            NewToken->String = malloc(Len + 1);
            strncpy(NewToken->String, &Lex->Input[*pPos - Len], Len);
            NewToken->String[Len] = 0;
            (*pPos)--;

            if (strcmp(NewToken->String, "return") == 0)
            {
                NewToken->Type = TOK_RETURN;
            }
            else if (strcmp(NewToken->String, "while") == 0)
            {
                NewToken->Type = TOK_WHILE;
            }
            else if (strcmp(NewToken->String, "void") == 0)
            {
                NewToken->Type = TOK_VOID;
            }
            else if (strcmp(NewToken->String, "int") == 0)
            {
                NewToken->Type = TOK_INT;
            }
            else if (strcmp(NewToken->String, "char") == 0)
            {
                NewToken->Type = TOK_CHAR;
            }

            Lexer_AppendToken(Lex, NewToken);
        }
        else if (c == '"' || c == '\'')
        {
            (*pPos)++;
            TokNode *NewToken = TokNode_New((c == '\'') ? TOK_CHARLIT : TOK_STRINGLIT);

            char Buffer[256] = {0};
            size_t BufPos = 0;

            while (Lex->Input[*pPos] != c)
            {
                char c = Lex->Input[*pPos];
                if (c == '\\')
                {
                    switch (Lex->Input[++(*pPos)])
                    {
                    case 'n':
                        c = '\n';
                        break;

                    default:
                        c = Lex->Input[*pPos];
                        break;
                    }
                }
                Buffer[BufPos++] = c;
                (*pPos)++;
            }

            NewToken->String = strdup(Buffer);

            Lexer_AppendToken(Lex, NewToken);
        }
        else if (IsSpace(c))
        {
            continue; // ignore whitespace
        }
        else
        {
            // big switch incoming
            switch (c)
            {
            case '(':
                Lexer_AppendToken(Lex, TokNode_New(TOK_OPAREN));
                break;
            case ')':
                Lexer_AppendToken(Lex, TokNode_New(TOK_CPAREN));
                break;
            case '{':
                Lexer_AppendToken(Lex, TokNode_New(TOK_OBRACE));
                break;
            case '}':
                Lexer_AppendToken(Lex, TokNode_New(TOK_CBRACE));
                break;
            case ',':
                Lexer_AppendToken(Lex, TokNode_New(TOK_COMMA));
                break;
            case ';':
                Lexer_AppendToken(Lex, TokNode_New(TOK_SEMICOLON));
                break;
            case '=':
                Lexer_AppendToken(Lex, TokNode_New(TOK_EQUAL));
                break;
            case '<':
                Lexer_AppendToken(Lex, TokNode_New(TOK_OANGLE));
                break;
            case '>':
                Lexer_AppendToken(Lex, TokNode_New(TOK_CANGLE));
                break;
            case '*':
                Lexer_AppendToken(Lex, TokNode_New(TOK_STAR));
                break;
            case '&':
                Lexer_AppendToken(Lex, TokNode_New(TOK_AMPERSAND));
                break;
            case '+':
                if (Lex->Input[*pPos + 1] == '+')
                {
                    Lexer_AppendToken(Lex, TokNode_New(TOK_PLUSPLUS));
                    (*pPos)++;
                    break;
                }
                Lexer_AppendToken(Lex, TokNode_New(TOK_PLUS));
                break;
            case '/':
                if (Lex->Input[*pPos + 1] == '/')
                {
                    // handle comments
                    while ((*pPos < Length) && Lex->Input[*pPos] != '\n')
                    {
                        (*pPos)++;
                    }
                    break;
                }
                Lexer_AppendToken(Lex, TokNode_New(TOK_SLASH));
                break;
            default:
                break;
            }
        }
    }
}

