
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "Parser.h"

void StmtNode_FreeAllRecursive(StmtNode *List)
{
    if (List == NULL)
    {
        return;
    }

    StmtNode *Node = List;
    while (Node)
    {
        StmtNode *OldNode = Node;

        switch (OldNode->Type)
        {
        case STMT_EXPR:
            Expr_FreeRecursive(OldNode->As.Expr);
            break;

        case STMT_WHILE:
            Expr_FreeRecursive(OldNode->As.While.Condition);
            StmtNode_FreeAllRecursive(OldNode->As.While.Body);
            break;

        case STMT_FUNC:
            free(OldNode->As.Func.Name);
            StmtNode_FreeAllRecursive(OldNode->As.Func.Body);
            break;

        case STMT_VARDECL:
            Expr_FreeRecursive(OldNode->As.VarDecl.Init);
            break;

        case STMT_RETURN:
            Expr_FreeRecursive(OldNode->As.Return);
            break;

        default:
            break;
        }

        Node = OldNode->Next;
        free(OldNode);
    }
}

void Expr_FreeRecursive(Expr_t *Expr)
{
    if (Expr == NULL)
    {
        return;
    }

    switch (Expr->Type)
    {
    case EXPR_CALL:
        Expr_FreeRecursive(Expr->As.Call.Callee);
        for (size_t i = 0; i < (sizeof(Expr->As.Call.Arguments) / sizeof(Expr->As.Call.Arguments[0])); i++)
        {
            Expr_t *Arg = Expr->As.Call.Arguments[i];
            if (Arg == NULL)
            {
                break;
            }
            Expr_FreeRecursive(Arg);
        }
        break;

    case EXPR_ASSIGN:
        Expr_FreeRecursive(Expr->As.Assign.Target);
        Expr_FreeRecursive(Expr->As.Assign.Expr);
        break;

    case EXPR_ADDRESSOF:
        Expr_FreeRecursive(Expr->As.AddressOf);
        break;

    case EXPR_INC:
        Expr_FreeRecursive(Expr->As.Inc);
        break;

    case EXPR_DEREF:
        Expr_FreeRecursive(Expr->As.Deref);
        break;

    case EXPR_BINARYOP:
        Expr_FreeRecursive(Expr->As.BinaryOp.A);
        Expr_FreeRecursive(Expr->As.BinaryOp.B);
        break;

    default:
        break;
    }

    free(Expr);
}

void StmtNode_Append(StmtNode *List, StmtNode *NewNode)
{
    if (List == NULL)
    {
        return;
    }
    StmtNode *Node = List;
    while (Node->Next)
    {
        Node = Node->Next;
    }
    Node->Next = NewNode;
}

void Parser_AppendStmt(Parser *Parse, StmtNode *NewNode)
{
    if (NewNode == NULL)
    {
        return;
    }

    if (Parse->Ast == NULL)
    {
        Parse->Ast = NewNode;
    }
    else
    {
        StmtNode_Append(Parse->Ast, NewNode);
    }
}

TokNode *Parser_ConsumeTok(Parser *Parse)
{
    if (Parse->Tok == NULL)
    {
        return NULL;
    }

    TokNode *OldNode = Parse->Tok;
    Parse->Tok = OldNode->Next;
    return OldNode;
}

TokNode *Parser_ExpectTok(Parser *Parse, TokType Type)
{
    if (Parse->Tok == NULL)
    {
        return NULL;
    }

    TokNode *OldNode = Parse->Tok;

    if (OldNode->Type != Type)
    {
        printf("expected %i, but got %i\n", Type, OldNode->Type);
    }

    Parse->Tok = OldNode->Next;
    return OldNode;
}

TokNode *Parser_PeekTok(Parser *Parse)
{
    static TokNode EofTok = {0};
    EofTok.Type = -1;
    if (Parse->Tok == NULL)
    {
        return &EofTok;
    }
    else
    {
        return Parse->Tok;
    }
}

void Parser_Parse(Parser *Parse)
{
    while (Parse->Tok)
    {
        StmtNode *Stmt = Parser_ParseStmt(Parse);
        Parser_AppendStmt(Parse, Stmt);
    }
}

TypeDesc Parser_ParseType(Parser *Parse)
{
    TypeDesc Type = {0};
    TokNode *Node = Parser_PeekTok(Parse);

    switch (Node->Type)
    {
    case TOK_VOID:
    {
        Parser_ConsumeTok(Parse);
        Type.Type = TYPE_VOID;
    }
    break;

    case TOK_INT:
    {
        Parser_ConsumeTok(Parse);
        Type.Type = TYPE_INT;
    }
    break;

    case TOK_CHAR:
    {
        Parser_ConsumeTok(Parse);
        Type.Type = TYPE_CHAR;
    }
    break;

    default:
    {
        Type.Type = TYPE_NOT_A_TYPE;
    }
    break;
    }

    while (Parser_PeekTok(Parse)->Type == TOK_STAR)
    {
        Parser_ConsumeTok(Parse);
        Type.PointerDepth++;
    }

    return Type;
}

Expr_t *Parser_ParseExpr(Parser *Parse)
{
    Expr_t *Expr = Parser_ParsePrimary(Parse);

    if (Parser_PeekTok(Parse)->Type == TOK_PLUS)
    {
        Parser_ConsumeTok(Parse);
        Expr_t *AddExpr = malloc(sizeof(Expr_t));
        AddExpr->Type = EXPR_BINARYOP;
        AddExpr->As.BinaryOp.Op = OP_ADD;
        AddExpr->As.BinaryOp.A = Expr;
        AddExpr->As.BinaryOp.B = Parser_ParsePrimary(Parse);
        Expr = AddExpr;
    }
    else if (Parser_PeekTok(Parse)->Type == TOK_OANGLE)
    {
        Parser_ConsumeTok(Parse);
        Expr_t *LessThanExpr = malloc(sizeof(Expr_t));
        LessThanExpr->Type = EXPR_BINARYOP;
        LessThanExpr->As.BinaryOp.Op = OP_LESSTHAN;
        LessThanExpr->As.BinaryOp.A = Expr;
        LessThanExpr->As.BinaryOp.B = Parser_ParsePrimary(Parse);
        Expr = LessThanExpr;
    }

    return Expr;
}

Expr_t *Parser_ParsePrimary(Parser *Parse)
{
    Expr_t *Expr = Parser_ParseSecondary(Parse);

    if (Parser_PeekTok(Parse)->Type == TOK_OPAREN)
    {
        Parser_ConsumeTok(Parse);

        Expr_t *CallExpr = malloc(sizeof(Expr_t));
        CallExpr->Type = EXPR_CALL;
        CallExpr->As.Call.Callee = Expr;

        memset(&CallExpr->As.Call.Arguments, 0, sizeof(CallExpr->As.Call.Arguments));

        for (size_t i = 0; (Parser_PeekTok(Parse)->Type != TOK_CPAREN); i++)
        {
            CallExpr->As.Call.Arguments[i] = Parser_ParseExpr(Parse);

            if (Parser_PeekTok(Parse)->Type != TOK_CPAREN)
            {
                Parser_ExpectTok(Parse, TOK_COMMA);
            }
        }

        Parser_ConsumeTok(Parse);
        Expr = CallExpr;
    }
    else if (Parser_PeekTok(Parse)->Type == TOK_EQUAL)
    {
        Parser_ConsumeTok(Parse);
        Expr_t *AssignExpr = malloc(sizeof(Expr_t));
        AssignExpr->Type = EXPR_ASSIGN;
        AssignExpr->As.Assign.Target = Expr;
        AssignExpr->As.Assign.Expr = Parser_ParseExpr(Parse);
        
        Expr = AssignExpr;
    }

    return Expr;
}

Expr_t *Parser_ParseSecondary(Parser *Parse)
{
    TokNode *Node = Parser_ConsumeTok(Parse);

    switch (Node->Type)
    {
    case TOK_OPAREN: // TODO: "(type)expr" casting
    {
        Expr_t *Expr = Parser_ParseExpr(Parse);
        Parser_ExpectTok(Parse, TOK_CPAREN);
        return Expr;
    }
    break;

    case TOK_NUMBERLIT:
    {
        Expr_t *Expr = malloc(sizeof(Expr_t));
        memset(Expr, 0, sizeof(Expr_t));
        Expr->Type = EXPR_NUMBERLIT;
        Expr->As.NumberLit = atoi(Node->String);
        return Expr;
    }
    break;

    case TOK_CHARLIT:
    {
        Expr_t *Expr = malloc(sizeof(Expr_t));
        memset(Expr, 0, sizeof(Expr_t));
        Expr->Type = EXPR_CHARLIT;
        Expr->As.CharLit = *(Node->String);
        return Expr;
    }
    break;

    case TOK_STRINGLIT:
    {
        Expr_t *Expr = malloc(sizeof(Expr_t));
        memset(Expr, 0, sizeof(Expr_t));
        Expr->Type = EXPR_STRINGLIT;
        Expr->As.StringLit = Node->String;
        return Expr;
    }
    break;
    
    case TOK_IDENT:
    {
        Expr_t *Expr = malloc(sizeof(Expr_t));
        memset(Expr, 0, sizeof(Expr_t));
        Expr->Type = EXPR_IDENT;
        Expr->As.Ident = Node->String;
        return Expr;
    }
    break;

    case TOK_AMPERSAND:
    {
        Expr_t *Expr = malloc(sizeof(Expr_t));
        memset(Expr, 0, sizeof(Expr_t));
        Expr->Type = EXPR_ADDRESSOF;
        Expr->As.AddressOf = Parser_ParseSecondary(Parse);
        return Expr;
    }
    break;

    case TOK_PLUSPLUS:
    {
        Expr_t *Expr = malloc(sizeof(Expr_t));
        memset(Expr, 0, sizeof(Expr_t));
        Expr->Type = EXPR_INC;
        Expr->As.Inc = Parser_ParseSecondary(Parse);
        return Expr;
    }
    break;

    case TOK_STAR:
    {
        Expr_t *Expr = malloc(sizeof(Expr_t));
        memset(Expr, 0, sizeof(Expr_t));
        Expr->Type = EXPR_DEREF;
        Expr->As.Deref = Parser_ParseSecondary(Parse);
        return Expr;
    }
    break;

    default:
        printf("expected an expression\n");
        return NULL;
    }
}

StmtNode *Parser_ParseStmt(Parser *Parse)
{
    if (Parse->Tok == NULL)
    {
        return NULL;
    }

    TypeDesc Type = Parser_ParseType(Parse);
    if (Type.Type != TYPE_NOT_A_TYPE)
    {
        if (Parser_PeekTok(Parse)->Next->Type == TOK_OPAREN)
        {
            return Parser_ParseFuncStmt(Parse, &Type);
        }
        else
        {
            return Parser_ParseVarDecl(Parse, &Type);
        }
    }

    TokNode *Node = Parser_PeekTok(Parse);

    switch (Node->Type)
    {

    case TOK_RETURN:
    {
        Parser_ConsumeTok(Parse);

        StmtNode *ReturnStmt = malloc(sizeof(StmtNode));
        memset(ReturnStmt, 0, sizeof(StmtNode));
        ReturnStmt->Type = STMT_RETURN;
        ReturnStmt->Next = NULL;

        if (Parser_PeekTok(Parse)->Type == TOK_SEMICOLON)
        {
            ReturnStmt->As.Return = NULL;
            Parser_ConsumeTok(Parse);
        }
        else
        {
            ReturnStmt->As.Return = Parser_ParseExpr(Parse);
            Parser_ExpectTok(Parse, TOK_SEMICOLON);
        }
        
        return ReturnStmt;
    }
    break;

    case TOK_WHILE:
    {
        Parser_ConsumeTok(Parse);
        
        Parser_ExpectTok(Parse, TOK_OPAREN);
        Expr_t *Condition = Parser_ParseExpr(Parse);
        Parser_ExpectTok(Parse, TOK_CPAREN);

        StmtNode *WhileNode = malloc(sizeof(StmtNode));
        memset(WhileNode, 0, sizeof(StmtNode));
        WhileNode->Type = STMT_WHILE;
        WhileNode->As.While.Condition = Condition;
        WhileNode->As.While.Body = NULL;

        Parser_ExpectTok(Parse, TOK_OBRACE);
        while (Parser_PeekTok(Parse)->Type != TOK_CBRACE)
        {
            StmtNode *Stmt = Parser_ParseStmt(Parse);
            if (WhileNode->As.Func.Body == NULL)
            {
                WhileNode->As.Func.Body = Stmt;
            }
            else
            {
                StmtNode_Append(WhileNode->As.Func.Body, Stmt);
            }
        }

        Parser_ConsumeTok(Parse);

        return WhileNode;
    }
    break;

    default:
        StmtNode *Stmt = malloc(sizeof(StmtNode));
        memset(Stmt, 0, sizeof(StmtNode));
        Stmt->Type = STMT_EXPR;
        Stmt->Next = NULL;
        Stmt->As.Expr = Parser_ParseExpr(Parse);
        Parser_ExpectTok(Parse, TOK_SEMICOLON);

        return Stmt;
    }
}

StmtNode *Parser_ParseFuncStmt(Parser *Parse, TypeDesc *ReturnType)
{
    const char *Name = Parser_ExpectTok(Parse, TOK_IDENT)->String;
    
    StmtNode *FuncNode = malloc(sizeof(StmtNode));
    memset(FuncNode, 0, sizeof(StmtNode));
    FuncNode->Type = STMT_FUNC;
    FuncNode->As.Func.Name = strdup(Name);
    FuncNode->As.Func.Body = NULL;
    if (ReturnType)
    {
        FuncNode->As.Func.ReturnType = *ReturnType;
    }

    Parser_ExpectTok(Parse, TOK_OPAREN);

    memset(&FuncNode->As.Func.Params, 0, sizeof(FuncNode->As.Func.Params));
    for (size_t i = 0; (Parser_PeekTok(Parse)->Type != TOK_CPAREN); i++)
    {
        TypeDesc ParamType = Parser_ParseType(Parse);
        FuncNode->As.Func.Params[i].Type = ParamType;

        if (ParamType.Type == TYPE_NOT_A_TYPE)
        {
            printf("parameters must have a type\n");
        }

        const char *ParamName = Parser_ExpectTok(Parse, TOK_IDENT)->String;
        FuncNode->As.Func.Params[i].Name = ParamName;

        if (Parser_PeekTok(Parse)->Type != TOK_CPAREN)
        {
            Parser_ExpectTok(Parse, TOK_COMMA);
        }
    }

    Parser_ConsumeTok(Parse);

    Parser_ExpectTok(Parse, TOK_OBRACE);
    while (Parser_PeekTok(Parse)->Type != TOK_CBRACE)
    {
        StmtNode *Stmt = Parser_ParseStmt(Parse);
        if (FuncNode->As.Func.Body == NULL)
        {
            FuncNode->As.Func.Body = Stmt;
        }
        else
        {
            StmtNode_Append(FuncNode->As.Func.Body, Stmt);
        }
    }

    Parser_ConsumeTok(Parse);

    return FuncNode;
}

StmtNode *Parser_ParseVarDecl(Parser *Parse, TypeDesc *Type)
{
    const char *Name = Parser_ExpectTok(Parse, TOK_IDENT)->String;

    StmtNode *VarDecl = malloc(sizeof(StmtNode));
    memset(VarDecl, 0, sizeof(StmtNode));
    VarDecl->Type = STMT_VARDECL;
    VarDecl->As.VarDecl.Type = *Type;
    VarDecl->As.VarDecl.Name = Name;
    VarDecl->As.VarDecl.Init = NULL;

    if (Parser_PeekTok(Parse)->Type == TOK_EQUAL)
    {
        Parser_ConsumeTok(Parse);
        VarDecl->As.VarDecl.Init = Parser_ParseExpr(Parse);
    }

    Parser_ExpectTok(Parse, TOK_SEMICOLON);

    return VarDecl;
}
