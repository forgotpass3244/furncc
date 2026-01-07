
#include "Compiler.h"
#include <stdarg.h>
#include <string.h>

void VarNode_FreeAll(VarNode *List)
{
    VarNode *Node = List;
    while (Node)
    {
        VarNode *OldNode = Node;
        Node = OldNode->Next;
        free(OldNode->Name);
        if (OldNode->Func)
        {
            free(OldNode->Func);
        }
        free(OldNode);
    }
}

void VarNode_Append(VarNode *List, VarNode *NewNode)
{
    VarNode *Node = List;
    while (Node->Next)
    {
        Node = Node->Next;
    }
    Node->Next = NewNode;
}

void Compiler_AppendVar(Compiler *Cmpl, VarNode *NewNode)
{
    if (Cmpl->Vars == NULL)
    {
        Cmpl->Vars = NewNode;
    }
    else
    {
        VarNode_Append(Cmpl->Vars, NewNode);
    }
}

static inline void Compiler_Error(Compiler *Cmpl, const char *Fmt, ...)
{
    va_list Args;
    va_start(Args, Fmt);
    vfprintf(stderr, Fmt, Args);
    va_end(Args);
    Cmpl->HasErrors = true;
}

VarNode *Compiler_VarLookup(Compiler *Cmpl, const char *Name)
{
    VarNode *Node = Cmpl->Vars;
    while (Node)
    {
        if (strcmp(Node->Name, Name) == 0)
        {
            break;
        }
        Node = Node->Next;
    }

    return Node;
}

CmplSymbol Compiler_ResolveSymbol(Compiler *Cmpl, Expr_t *Expr)
{
    if (Expr == NULL)
    {
        return (CmplSymbol) { { TYPE_NOT_A_TYPE }, NULL };
    }

    CmplSymbol Symbol = {0};
    Symbol.Type = (TypeDesc) { TYPE_VOID, 0 };

    switch (Expr->Type)
    {
    case EXPR_STRINGLIT:
    {
        Symbol.Type = (TypeDesc) { TYPE_CHAR, 1 };
        return Symbol;
    }
    break;

    case EXPR_NUMBERLIT:
    {
        Symbol.Type = (TypeDesc) { TYPE_INT, 0 };
        return Symbol;
    }
    break;

    case EXPR_IDENT:
    {
        VarNode *Var = Compiler_VarLookup(Cmpl, Expr->As.Ident);

        if (Var == NULL)
        {
            Symbol.Type.Type = TYPE_NOT_A_TYPE;
        }
        else
        {
            Symbol.Var = Var;
            Symbol.Type = Var->Type;
        }

        return Symbol;
    }
    break;

    case EXPR_CALL:
    {
        CmplSymbol FuncSymbol = Compiler_ResolveSymbol(Cmpl, Expr->As.Call.Callee);
        if (FuncSymbol.Var)
        {
            if (FuncSymbol.Var->Func)
            {
                Symbol.Type = FuncSymbol.Var->Func->ReturnType;
            }
        }
        return Symbol;
    }
    break;

    default:
    {
        Symbol.Type = (TypeDesc) { TYPE_VOID, 0 };
        return Symbol;
    }
    break;
    }
}

void Compiler_GenExpr(Compiler *Cmpl, Expr_t *Expr)
{
    if (Expr == NULL)
    {
        return;
    }

    switch (Expr->Type)
    {
    case EXPR_NUMBERLIT:
    {
        BCBuild_Put(&Cmpl->BCBuilder, LOAD_QWORD);
        BCBuild_PutAddress(&Cmpl->BCBuilder, REGISTER64_A);
        BCBuild_PutQWord(&Cmpl->BCBuilder, Expr->As.NumberLit);
    }
    break;

    case EXPR_CHARLIT:
    {
        BCBuild_Put(&Cmpl->BCBuilder, LOAD_QWORD);
        BCBuild_PutAddress(&Cmpl->BCBuilder, REGISTER64_A);
        BCBuild_PutQWord(&Cmpl->BCBuilder, Expr->As.CharLit);
    }
    break;

    case EXPR_STRINGLIT:
    {
        StringData StringData = Cmpl->StringDataList[0];
        size_t StringIndex = 0;
        size_t StringPointer = 0;
        while (StringData.String)
        {
            if (strcmp(StringData.String, Expr->As.StringLit) == 0)
            {
                break;
            }
            StringPointer += (strlen(StringData.String) + 1);
            StringData = Cmpl->StringDataList[++StringIndex];
        }
        if (StringData.String == NULL)
        {
            StringData.String = Expr->As.StringLit;
        }

        Cmpl->StringDataList[StringIndex] = StringData;

        BCBuild_Put(&Cmpl->BCBuilder, LOAD_QWORD);
        BCBuild_PutAddress(&Cmpl->BCBuilder, REGISTER64_A);
        BCBuild_PutAddress(&Cmpl->BCBuilder, 2000 + StringPointer);
    }
    break;

    case EXPR_IDENT:
    {
        VarNode *Var = Compiler_VarLookup(Cmpl, Expr->As.Ident);

        if (Var == NULL)
        {
            Compiler_Error(Cmpl, "undefined variable '%s'\n", Expr->As.Ident);
            return;
        }
        else if (Var->Func)
        {
            BCBuild_Put(&Cmpl->BCBuilder, LOAD_QWORD);
            BCBuild_PutAddress(&Cmpl->BCBuilder, REGISTER64_A);
            BCBuild_PutAddress(&Cmpl->BCBuilder, Var->Func->Label);
        }
        else
        {
            BCBuild_Put(&Cmpl->BCBuilder, STACK_READ_QWORD);
            BCBuild_PutQWord(&Cmpl->BCBuilder, Cmpl->StackLoc - Var->AddressOffset);
            BCBuild_PutAddress(&Cmpl->BCBuilder, REGISTER64_A);
        }
    }
    break;

    case EXPR_CALL:
    {
        size_t ArgCount = 0;
        for (int d = (sizeof(Expr->As.Call.Arguments) / sizeof(Expr->As.Call.Arguments[0])) - 1; d >= 0; d--)
        {
            Expr_t *ArgExpr = Expr->As.Call.Arguments[d];
            if (ArgExpr == NULL)
            {
                continue;
            }
            ArgCount++;
        }
        
        // reverse evaluation order
        for (int i = (ArgCount - 1); i >= 0; i--)
        {
            Expr_t *ArgExpr = Expr->As.Call.Arguments[i];
            Compiler_GenExpr(Cmpl, ArgExpr);

            BCBuild_Put(&Cmpl->BCBuilder, PUSH_QWORD);
            BCBuild_PutAddress(&Cmpl->BCBuilder, REGISTER64_A);
        }

        CmplSymbol FuncSymbol = Compiler_ResolveSymbol(Cmpl, Expr->As.Call.Callee);

        if (FuncSymbol.Var)
        {
            if (FuncSymbol.Var->Func)
            {
                BCBuild_Put(&Cmpl->BCBuilder, CALL);
                BCBuild_PutAddress(&Cmpl->BCBuilder, FuncSymbol.Var->Func->Label);
            }
        }
        else
        {
            Compiler_Error(Cmpl, "expected an lvalue to call\n");

            Compiler_GenExpr(Cmpl, Expr->As.Call.Callee);
            BCBuild_Put(&Cmpl->BCBuilder, MOVE_QWORD);
            BCBuild_PutAddress(&Cmpl->BCBuilder, REGISTER64_A);
            BCBuild_PutAddress(&Cmpl->BCBuilder, Cmpl->BCBuilder.Position + 8);

            BCBuild_Put(&Cmpl->BCBuilder, CALL);
            BCBuild_PutAddress(&Cmpl->BCBuilder, 0); // replaced at runtime
        }

        // callee pops args off the stack
    }
    break;

    case EXPR_ASSIGN:
    {
        if (Expr->As.Assign.Target->Type == EXPR_IDENT)
        {
            VarNode *Var = Compiler_VarLookup(Cmpl, Expr->As.Assign.Target->As.Ident);

            if (Var == NULL)
            {
                Compiler_Error(Cmpl, "variable '%s' must be declared before assigning\n", Expr->As.Assign.Target->As.Ident);
                return;
            }

            Compiler_GenExpr(Cmpl, Expr->As.Assign.Expr);

            BCBuild_Put(&Cmpl->BCBuilder, STACK_WRITE_QWORD);
            BCBuild_PutQWord(&Cmpl->BCBuilder, Cmpl->StackLoc - Var->AddressOffset);
            BCBuild_PutAddress(&Cmpl->BCBuilder, REGISTER64_A);
        }
    }
    break;

    case EXPR_ADDRESSOF:
    {
        CmplSymbol Symbol = Compiler_ResolveSymbol(Cmpl, Expr->As.AddressOf);
        if (Symbol.Var)
        {
            BCBuild_Put(&Cmpl->BCBuilder, STACK_POINTER_FROM_OFFSET);
            BCBuild_PutQWord(&Cmpl->BCBuilder, Cmpl->StackLoc - Symbol.Var->AddressOffset);
            BCBuild_PutAddress(&Cmpl->BCBuilder, REGISTER64_A);
        }
        else
        {
            Compiler_Error(Cmpl, "expected an lvalue to take the address of\n");
        }
    }
    break;

    case EXPR_INC:
    {
        CmplSymbol Symbol = Compiler_ResolveSymbol(Cmpl, Expr->As.Inc);
        if (Symbol.Var)
        {
            BCBuild_Put(&Cmpl->BCBuilder, STACK_READ_QWORD);
            BCBuild_PutAddress(&Cmpl->BCBuilder, Cmpl->StackLoc - Symbol.Var->AddressOffset);
            BCBuild_PutAddress(&Cmpl->BCBuilder, REGISTER64_A);

            BCBuild_Put(&Cmpl->BCBuilder, INC_QWORD);
            BCBuild_PutAddress(&Cmpl->BCBuilder, REGISTER64_A);

            BCBuild_Put(&Cmpl->BCBuilder, STACK_WRITE_QWORD);
            BCBuild_PutAddress(&Cmpl->BCBuilder, Cmpl->StackLoc - Symbol.Var->AddressOffset);
            BCBuild_PutAddress(&Cmpl->BCBuilder, REGISTER64_A);
        }
        else if (Expr->As.Inc->Type == EXPR_DEREF)
        {
            Compiler_GenExpr(Cmpl, Expr->As.Inc->As.Deref);

            BCBuild_Put(&Cmpl->BCBuilder, MOVE_QWORD);
            BCBuild_PutAddress(&Cmpl->BCBuilder, REGISTER64_A);
            BCBuild_PutAddress(&Cmpl->BCBuilder, Cmpl->BCBuilder.Position + 9);

            BCBuild_Put(&Cmpl->BCBuilder, INC_QWORD);
            BCBuild_PutAddress(&Cmpl->BCBuilder, 0);
        }
        else
        {
            Compiler_Error(Cmpl, "expected an lvalue to increment\n");
        }
    }
    break;

    case EXPR_DEREF:
    {
        Compiler_GenExpr(Cmpl, Expr->As.Deref);
        BCBuild_Put(&Cmpl->BCBuilder, DEREF_QWORD);
        BCBuild_PutAddress(&Cmpl->BCBuilder, REGISTER64_A);
        BCBuild_PutAddress(&Cmpl->BCBuilder, REGISTER64_A);
    }
    break;

    case EXPR_BINARYOP:
    {
        switch (Expr->As.BinaryOp.Op)
        {
        case OP_ADD:
        {
            Compiler_GenExpr(Cmpl, Expr->As.BinaryOp.A);

            BCBuild_Put(&Cmpl->BCBuilder, PUSH_QWORD);
            BCBuild_PutAddress(&Cmpl->BCBuilder, REGISTER64_A);
            Cmpl->StackLoc += sizeof(QWord);

            Compiler_GenExpr(Cmpl, Expr->As.BinaryOp.B);

            BCBuild_Put(&Cmpl->BCBuilder, POP_QWORD);
            BCBuild_PutAddress(&Cmpl->BCBuilder, REGISTER64_B);
            Cmpl->StackLoc -= sizeof(QWord);

            BCBuild_Put(&Cmpl->BCBuilder, ADD_QWORD);
            BCBuild_PutAddress(&Cmpl->BCBuilder, REGISTER64_A);
            BCBuild_PutAddress(&Cmpl->BCBuilder, REGISTER64_B);
            BCBuild_PutAddress(&Cmpl->BCBuilder, REGISTER64_A);
        }
        break;

        case OP_LESSTHAN:
        {
            Compiler_GenExpr(Cmpl, Expr->As.BinaryOp.A);

            BCBuild_Put(&Cmpl->BCBuilder, PUSH_QWORD);
            BCBuild_PutAddress(&Cmpl->BCBuilder, REGISTER64_A);
            Cmpl->StackLoc += sizeof(QWord);

            Compiler_GenExpr(Cmpl, Expr->As.BinaryOp.B);

            BCBuild_Put(&Cmpl->BCBuilder, POP_QWORD);
            BCBuild_PutAddress(&Cmpl->BCBuilder, REGISTER64_B);
            Cmpl->StackLoc -= sizeof(QWord);

            // op a = rb64, op b = ra64
            BCBuild_Put(&Cmpl->BCBuilder, COMPARE_QWORD);
            BCBuild_PutAddress(&Cmpl->BCBuilder, REGISTER64_A);
            BCBuild_PutAddress(&Cmpl->BCBuilder, REGISTER64_B);

            BCBuild_Put(&Cmpl->BCBuilder, LOAD_QWORD);
            BCBuild_PutAddress(&Cmpl->BCBuilder, REGISTER64_A);
            BCBuild_PutQWord(&Cmpl->BCBuilder, 0);
            
            BCBuild_Put(&Cmpl->BCBuilder, MAP_GREATER_BYTE);
            BCBuild_PutAddress(&Cmpl->BCBuilder, REGISTER64_A);
        }
        break;

        default:
        {
        }
        break;
        }
    }
    break;

    default:
        break;
    }
}

void Compiler_GenStmt(Compiler *Cmpl, StmtNode *Stmt)
{
    switch (Stmt->Type)
    {
    case STMT_EXPR:
    {
        Compiler_GenExpr(Cmpl, Stmt->As.Expr);
    }
    break;

    case STMT_FUNC:
    {
        Function *Func = malloc(sizeof(Function));
        Func->Label = Cmpl->BCBuilder.Position;
        Func->Params[0].Name = NULL;

        Func->ReturnType = Stmt->As.Func.ReturnType;

        VarNode *FuncVar = malloc(sizeof(VarNode));
        FuncVar->Name = strdup(Stmt->As.Func.Name);
        FuncVar->Next = NULL;
        FuncVar->Func = Func;
        Compiler_AppendVar(Cmpl, FuncVar);

        VarNode *VarBeforeParams = Cmpl->Vars;
        memcpy(Func->Params, Stmt->As.Func.Params, sizeof(Func->Params)); // copy params
        for (size_t i = 0; i < sizeof(Func->Params); i++)
        {
            if (Func->Params[i].Name == NULL)
            {
                break;
            }

            VarNode *Param = malloc(sizeof(VarNode));
            Param->Name = strdup(Func->Params[i].Name);
            Param->Next = NULL;
            Param->Func = NULL;
            Param->Type = Func->Params[i].Type;
            Param->AddressOffset = Cmpl->StackLoc;
            Compiler_AppendVar(Cmpl, Param);

            Cmpl->StackLoc += sizeof(QWord);
        }

        Cmpl->ReturnType = &Func->ReturnType;
        StmtNode *Node = Stmt->As.Func.Body;
        while (Node)
        {
            Compiler_GenStmt(Cmpl, Node);
            Node = Node->Next;
        }
        Cmpl->ReturnType = NULL;

        for (size_t i = 0; i < sizeof(Func->Params); i++)
        {
            if (Func->Params[i].Name == NULL)
            {
                break;
            }

            BCBuild_Put(&Cmpl->BCBuilder, POP_QWORD);
            BCBuild_PutAddress(&Cmpl->BCBuilder, 0);

            Cmpl->StackLoc -= sizeof(QWord);
        }

        BCBuild_Put(&Cmpl->BCBuilder, RETURN); // implicit return at end of function

        // remove params from variable list
        (void)VarBeforeParams;
        // VarNode_FreeAll(VarBeforeParams->Next);
        // VarBeforeParams->Next = NULL;
    }
    break;

    case STMT_RETURN:
    {
        if (Cmpl->ReturnType == NULL)
        {
            Compiler_Error(Cmpl, "no function to return from\n");
        }
        else if (Stmt->As.Return && Cmpl->ReturnType->Type == TYPE_VOID && Cmpl->ReturnType->PointerDepth == 0)
        {
            Compiler_Error(Cmpl, "cannot return a value from a void function\n");
        }
        
        if (Stmt->As.Return)
        {
            Compiler_GenExpr(Cmpl, Stmt->As.Return);
        }

        BCBuild_Put(&Cmpl->BCBuilder, RETURN);
    }
    break;

    case STMT_VARDECL:
    {
        VarNode *Var = malloc(sizeof(VarNode));
        Var->Name = strdup(Stmt->As.VarDecl.Name);
        Var->Next = NULL;
        Var->Func = NULL;
        Var->Type = Stmt->As.VarDecl.Type;
        Var->AddressOffset = Cmpl->StackLoc;
        Compiler_AppendVar(Cmpl, Var);

        if (Stmt->As.VarDecl.Init)
        {
            Compiler_GenExpr(Cmpl, Stmt->As.VarDecl.Init);
            BCBuild_Put(&Cmpl->BCBuilder, PUSH_QWORD);
            BCBuild_PutAddress(&Cmpl->BCBuilder, REGISTER64_A);
        }
        else
        {
            BCBuild_Put(&Cmpl->BCBuilder, PUSH_QWORD);
            BCBuild_PutAddress(&Cmpl->BCBuilder, 0);
        }

        Cmpl->StackLoc += sizeof(QWord);
    }
    break;


    case STMT_WHILE:
    {
        QWord Label = Cmpl->BCBuilder.Position;
        Compiler_GenExpr(Cmpl, Stmt->As.While.Condition); // TODO: do the jumping and stuff ifnotzero

        BCBuild_Put(&Cmpl->BCBuilder, SET_FLAGS_BYTE); // add qword version later but for now byte is fine since its usually 1/0
        BCBuild_PutAddress(&Cmpl->BCBuilder, REGISTER64_A);

        BCBuild_Put(&Cmpl->BCBuilder, JUMP_IF_ZERO);

        QWord Placeholder = Cmpl->BCBuilder.Position;
        BCBuild_PutAddress(&Cmpl->BCBuilder, 0); // placeholder

        StmtNode *Node = Stmt->As.While.Body;
        while (Node)
        {
            Compiler_GenStmt(Cmpl, Node);
            Node = Node->Next;
        }

        BCBuild_Put(&Cmpl->BCBuilder, JUMP);
        BCBuild_PutAddress(&Cmpl->BCBuilder, Label);

        QWord BodyEnd = Cmpl->BCBuilder.Position;
        Memory_WriteQWord(Cmpl->BCBuilder.Mem, Placeholder, BodyEnd);
    }
    break;

    default:
        break;
    }
}

void Compiler_Compile(Compiler *Cmpl)
{
    Memory Mem;
    Memory_Zero(&Mem);
    Cmpl->BCBuilder.Mem = &Mem;
    BCBuild_Header(&Cmpl->BCBuilder);

    // BCBuild_Put(&Cmpl->BCBuilder, LOAD_LIBRARY);
    // BCBuild_Put(&Cmpl->BCBuilder, 8); // path length
    // BCBuild_Put(&Cmpl->BCBuilder, 'b');
    // BCBuild_Put(&Cmpl->BCBuilder, 'i');
    // BCBuild_Put(&Cmpl->BCBuilder, 'n');
    // BCBuild_Put(&Cmpl->BCBuilder, '/');
    // BCBuild_Put(&Cmpl->BCBuilder, 'l');
    // BCBuild_Put(&Cmpl->BCBuilder, 'i');
    // BCBuild_Put(&Cmpl->BCBuilder, 'b');
    // BCBuild_Put(&Cmpl->BCBuilder, 'c');

    BCBuild_Put(&Cmpl->BCBuilder, CALL);

    // we dont know main()'s memory address yet
    size_t Placeholder = Cmpl->BCBuilder.Position;
    BCBuild_PutAddress(&Cmpl->BCBuilder, 0);

    BCBuild_Put(&Cmpl->BCBuilder, MOVE_DYNAMIC);
    BCBuild_PutAddress(&Cmpl->BCBuilder, REGISTER64_A); // source
    BCBuild_Put(&Cmpl->BCBuilder, 8);                   // source byte size
    BCBuild_PutAddress(&Cmpl->BCBuilder, REGISTER_A);   // destination
    BCBuild_Put(&Cmpl->BCBuilder, 1);                   // destination byte size

    BCBuild_Put(&Cmpl->BCBuilder, SYSCALL);
    BCBuild_Put(&Cmpl->BCBuilder, 0); // exit
    BCBuild_PutAddress(&Cmpl->BCBuilder, REGISTER64_A);


    
    
    // write
    {
        Function *Func = malloc(sizeof(Function));
        Func->Label = Cmpl->BCBuilder.Position;
        Func->Params[0].Name = NULL;

        VarNode *FuncVar = malloc(sizeof(VarNode));
        FuncVar->Name = strdup("write");
        FuncVar->Next = NULL;
        FuncVar->Func = Func;
        Compiler_AppendVar(Cmpl, FuncVar);

        BCBuild_Put(&Cmpl->BCBuilder, POP_QWORD);
        BCBuild_PutAddress(&Cmpl->BCBuilder, SYSCALL_ARG1);

        BCBuild_Put(&Cmpl->BCBuilder, POP_QWORD);
        BCBuild_PutAddress(&Cmpl->BCBuilder, SYSCALL_ARG2);

        BCBuild_Put(&Cmpl->BCBuilder, SYSCALL);
        BCBuild_Put(&Cmpl->BCBuilder, 1); // write stdout
        BCBuild_PutAddress(&Cmpl->BCBuilder, REGISTER64_A);

        // arguments already popped off the stack
        BCBuild_Put(&Cmpl->BCBuilder, RETURN);
    }

    // inc
    {
        Function *Func = malloc(sizeof(Function));
        Func->Label = Cmpl->BCBuilder.Position;
        Func->Params[0].Name = NULL;

        VarNode *FuncVar = malloc(sizeof(VarNode));
        FuncVar->Name = strdup("inc");
        FuncVar->Next = NULL;
        FuncVar->Func = Func;
        Compiler_AppendVar(Cmpl, FuncVar);

        BCBuild_Put(&Cmpl->BCBuilder, POP_QWORD);
        BCBuild_PutAddress(&Cmpl->BCBuilder, REGISTER64_A);

        BCBuild_Put(&Cmpl->BCBuilder, LOAD_QWORD);
        BCBuild_PutAddress(&Cmpl->BCBuilder, REGISTER64_B);
        BCBuild_PutQWord(&Cmpl->BCBuilder, 1);

        BCBuild_Put(&Cmpl->BCBuilder, MOVE_QWORD);
        BCBuild_PutAddress(&Cmpl->BCBuilder, REGISTER64_A);
        BCBuild_PutAddress(&Cmpl->BCBuilder, Cmpl->BCBuilder.Position + 9);

        BCBuild_Put(&Cmpl->BCBuilder, INC_QWORD);
        BCBuild_PutAddress(&Cmpl->BCBuilder, 0);

        // arguments already popped off the stack
        BCBuild_Put(&Cmpl->BCBuilder, RETURN);
    }

    // strlen
    {
        Function *Func = malloc(sizeof(Function));
        Func->Label = Cmpl->BCBuilder.Position;
        Func->Params[0].Name = NULL;

        VarNode *FuncVar = malloc(sizeof(VarNode));
        FuncVar->Name = strdup("strlen");
        FuncVar->Next = NULL;
        FuncVar->Func = Func;
        Compiler_AppendVar(Cmpl, FuncVar);

        BCBuild_Put(&Cmpl->BCBuilder, POP_QWORD);
        BCBuild_PutAddress(&Cmpl->BCBuilder, REGISTER64_A);

        // save original pointer to subtract later
        BCBuild_Put(&Cmpl->BCBuilder, MOVE_QWORD);
        BCBuild_PutAddress(&Cmpl->BCBuilder, REGISTER64_A);
        BCBuild_PutAddress(&Cmpl->BCBuilder, REGISTER64_B);

        QWord Label = Cmpl->BCBuilder.Position;

        BCBuild_Put(&Cmpl->BCBuilder, DEREF_BYTE);
        BCBuild_PutAddress(&Cmpl->BCBuilder, REGISTER64_A);
        BCBuild_PutAddress(&Cmpl->BCBuilder, REGISTER_A);

        BCBuild_Put(&Cmpl->BCBuilder, SET_FLAGS_BYTE);
        BCBuild_PutAddress(&Cmpl->BCBuilder, REGISTER_A);

        BCBuild_Put(&Cmpl->BCBuilder, JUMP_IF_ZERO);

        QWord Placeholder = Cmpl->BCBuilder.Position;
        BCBuild_PutAddress(&Cmpl->BCBuilder, 0); //placeholder

        BCBuild_Put(&Cmpl->BCBuilder, INC_QWORD);
        BCBuild_PutAddress(&Cmpl->BCBuilder, REGISTER64_A);

        BCBuild_Put(&Cmpl->BCBuilder, JUMP);
        BCBuild_PutAddress(&Cmpl->BCBuilder, Label);

        Memory_WriteQWord(Cmpl->BCBuilder.Mem, Placeholder, Cmpl->BCBuilder.Position);
        
        BCBuild_Put(&Cmpl->BCBuilder, SUB_QWORD);
        BCBuild_PutAddress(&Cmpl->BCBuilder, REGISTER64_A);
        BCBuild_PutAddress(&Cmpl->BCBuilder, REGISTER64_A);
        BCBuild_PutAddress(&Cmpl->BCBuilder, REGISTER64_B);

        // arguments already popped off the stack
        BCBuild_Put(&Cmpl->BCBuilder, RETURN);
    }

    // printf
    {
        Function *Func = malloc(sizeof(Function));
        Func->Label = Cmpl->BCBuilder.Position;
        Func->Params[0].Name = NULL;

        VarNode *FuncVar = malloc(sizeof(VarNode));
        FuncVar->Name = strdup("printf");
        FuncVar->Next = NULL;
        FuncVar->Func = Func;
        Compiler_AppendVar(Cmpl, FuncVar);

        // copy qword off the stack
        BCBuild_Put(&Cmpl->BCBuilder, POP_QWORD);
        BCBuild_PutAddress(&Cmpl->BCBuilder, REGISTER64_A);

        BCBuild_Put(&Cmpl->BCBuilder, LOAD_BYTE);
        BCBuild_PutAddress(&Cmpl->BCBuilder, REGISTER_B);
        BCBuild_Put(&Cmpl->BCBuilder, '%');

        QWord WhileLabel = Cmpl->BCBuilder.Position;

        // while condition

        BCBuild_Put(&Cmpl->BCBuilder, DEREF_BYTE);
        BCBuild_PutAddress(&Cmpl->BCBuilder, REGISTER64_A);
        BCBuild_PutAddress(&Cmpl->BCBuilder, REGISTER_A);

        BCBuild_Put(&Cmpl->BCBuilder, SET_FLAGS_BYTE);
        BCBuild_PutAddress(&Cmpl->BCBuilder, REGISTER_A);

        BCBuild_Put(&Cmpl->BCBuilder, JUMP_IF_ZERO);

        QWord WhilePlaceholder = Cmpl->BCBuilder.Position;
        BCBuild_PutAddress(&Cmpl->BCBuilder, 0); // placeholder

        // while body begin

        BCBuild_Put(&Cmpl->BCBuilder, MOVE_QWORD);
        BCBuild_PutAddress(&Cmpl->BCBuilder, REGISTER64_A);
        BCBuild_PutAddress(&Cmpl->BCBuilder, REGISTER64_D);

        BCBuild_Put(&Cmpl->BCBuilder, LOAD_QWORD);
        BCBuild_PutAddress(&Cmpl->BCBuilder, SYSCALL_ARG2);
        BCBuild_Put(&Cmpl->BCBuilder, 1);

        BCBuild_Put(&Cmpl->BCBuilder, SYSCALL);
        BCBuild_Put(&Cmpl->BCBuilder, 1);
        BCBuild_PutAddress(&Cmpl->BCBuilder, 0);
        
        BCBuild_Put(&Cmpl->BCBuilder, MOVE_QWORD);
        BCBuild_PutAddress(&Cmpl->BCBuilder, REGISTER64_D);
        BCBuild_PutAddress(&Cmpl->BCBuilder, REGISTER64_A);

        // if condition

        BCBuild_Put(&Cmpl->BCBuilder, COMPARE_BYTE);
        BCBuild_Put(&Cmpl->BCBuilder, REGISTER_A);
        BCBuild_Put(&Cmpl->BCBuilder, REGISTER_B);

        BCBuild_Put(&Cmpl->BCBuilder, TICK_FLAGS); // logical not
        BCBuild_Put(&Cmpl->BCBuilder, JUMP_IF_EQUAL);

        QWord IfPlaceholder = Cmpl->BCBuilder.Position;
        BCBuild_PutAddress(&Cmpl->BCBuilder, 0); // placeholder

        // if body begin

        BCBuild_Put(&Cmpl->BCBuilder, MOVE_QWORD);
        BCBuild_PutAddress(&Cmpl->BCBuilder, REGISTER64_A);
        BCBuild_PutAddress(&Cmpl->BCBuilder, REGISTER64_D);

        BCBuild_Put(&Cmpl->BCBuilder, LOAD_BYTE);
        BCBuild_PutAddress(&Cmpl->BCBuilder, REGISTER_C);
        BCBuild_Put(&Cmpl->BCBuilder, '_');

        BCBuild_Put(&Cmpl->BCBuilder, LOAD_QWORD);
        BCBuild_PutAddress(&Cmpl->BCBuilder, SYSCALL_ARG1);
        BCBuild_PutQWord(&Cmpl->BCBuilder, REGISTER_C);

        BCBuild_Put(&Cmpl->BCBuilder, LOAD_QWORD);
        BCBuild_PutAddress(&Cmpl->BCBuilder, SYSCALL_ARG2);
        BCBuild_PutQWord(&Cmpl->BCBuilder, 1);

        BCBuild_Put(&Cmpl->BCBuilder, SYSCALL);
        BCBuild_Put(&Cmpl->BCBuilder, 1);
        BCBuild_PutAddress(&Cmpl->BCBuilder, 0);

        BCBuild_Put(&Cmpl->BCBuilder, MOVE_QWORD);
        BCBuild_PutAddress(&Cmpl->BCBuilder, REGISTER64_D);
        BCBuild_PutAddress(&Cmpl->BCBuilder, REGISTER64_A);

        // if body end

        Memory_WriteQWord(Cmpl->BCBuilder.Mem, IfPlaceholder, Cmpl->BCBuilder.Position);

        // while body end

        BCBuild_Put(&Cmpl->BCBuilder, INC_QWORD);
        BCBuild_PutAddress(&Cmpl->BCBuilder, REGISTER64_A);

        BCBuild_Put(&Cmpl->BCBuilder, JUMP);
        BCBuild_PutAddress(&Cmpl->BCBuilder, WhileLabel);

        Memory_WriteQWord(Cmpl->BCBuilder.Mem, WhilePlaceholder, Cmpl->BCBuilder.Position);

        // arguments already popped off the stack
        BCBuild_Put(&Cmpl->BCBuilder, RETURN);
    }

    // puts
    {
        Function *Func = malloc(sizeof(Function));
        Func->Label = Cmpl->BCBuilder.Position;
        Func->Params[0].Name = NULL;

        VarNode *FuncVar = malloc(sizeof(VarNode));
        FuncVar->Name = strdup("puts");
        FuncVar->Next = NULL;
        FuncVar->Func = Func;
        Compiler_AppendVar(Cmpl, FuncVar);

        // copy qword off the stack
        BCBuild_Put(&Cmpl->BCBuilder, STACK_READ_QWORD);
        BCBuild_PutQWord(&Cmpl->BCBuilder, sizeof(QWord));
        BCBuild_PutAddress(&Cmpl->BCBuilder, REGISTER64_A);

        BCBuild_Put(&Cmpl->BCBuilder, PUSH_QWORD);
        BCBuild_PutAddress(&Cmpl->BCBuilder, REGISTER64_A);

        BCBuild_Put(&Cmpl->BCBuilder, CALL);
        BCBuild_PutAddress(&Cmpl->BCBuilder, Compiler_VarLookup(Cmpl, "strlen")->Func->Label);

        BCBuild_Put(&Cmpl->BCBuilder, PUSH_QWORD);
        BCBuild_PutAddress(&Cmpl->BCBuilder, REGISTER64_A);

        BCBuild_Put(&Cmpl->BCBuilder, STACK_READ_QWORD);
        BCBuild_PutQWord(&Cmpl->BCBuilder, sizeof(QWord) * 2);
        BCBuild_PutAddress(&Cmpl->BCBuilder, REGISTER64_A);

        BCBuild_Put(&Cmpl->BCBuilder, PUSH_QWORD);
        BCBuild_PutAddress(&Cmpl->BCBuilder, REGISTER64_A);

        BCBuild_Put(&Cmpl->BCBuilder, CALL);
        BCBuild_PutAddress(&Cmpl->BCBuilder, Compiler_VarLookup(Cmpl, "write")->Func->Label);

        // pop caller's arguments off the stack
        BCBuild_Put(&Cmpl->BCBuilder, POP_QWORD);
        BCBuild_PutAddress(&Cmpl->BCBuilder, 0);
        
        BCBuild_Put(&Cmpl->BCBuilder, RETURN);
    }

    // putchar
    {
        Function *Func = malloc(sizeof(Function));
        Func->Label = Cmpl->BCBuilder.Position;
        Func->Params[0].Name = NULL;

        VarNode *FuncVar = malloc(sizeof(VarNode));
        FuncVar->Name = strdup("putchar");
        FuncVar->Next = NULL;
        FuncVar->Func = Func;
        Compiler_AppendVar(Cmpl, FuncVar);

        BCBuild_Put(&Cmpl->BCBuilder, STACK_POINTER_FROM_OFFSET);
        BCBuild_PutAddress(&Cmpl->BCBuilder, 8);
        BCBuild_PutAddress(&Cmpl->BCBuilder, SYSCALL_ARG1);

        BCBuild_Put(&Cmpl->BCBuilder, LOAD_QWORD);
        BCBuild_PutAddress(&Cmpl->BCBuilder, SYSCALL_ARG2);
        BCBuild_PutAddress(&Cmpl->BCBuilder, 1);

        BCBuild_Put(&Cmpl->BCBuilder, SYSCALL);
        BCBuild_Put(&Cmpl->BCBuilder, SYSNUM_WRITE_OUT);
        BCBuild_PutAddress(&Cmpl->BCBuilder, REGISTER64_A);

        BCBuild_Put(&Cmpl->BCBuilder, POP_QWORD);
        BCBuild_PutAddress(&Cmpl->BCBuilder, 0);

        BCBuild_Put(&Cmpl->BCBuilder, RETURN);
    }

    // dumpstate
    {
        Function *Func = malloc(sizeof(Function));
        Func->Label = Cmpl->BCBuilder.Position;
        Func->Params[0].Name = NULL;

        VarNode *FuncVar = malloc(sizeof(VarNode));
        FuncVar->Name = strdup("dumpstate");
        FuncVar->Next = NULL;
        FuncVar->Func = Func;
        Compiler_AppendVar(Cmpl, FuncVar);

        BCBuild_Put(&Cmpl->BCBuilder, DUMP_STATE);
        BCBuild_Put(&Cmpl->BCBuilder, RETURN);
    }

    while (Cmpl->Stmt)
    {
        Compiler_GenStmt(Cmpl, Cmpl->Stmt);
        Cmpl->Stmt = Cmpl->Stmt->Next;
    }

    BCBuild_EndInstructions(&Cmpl->BCBuilder);

    VarNode *MainVar = Compiler_VarLookup(Cmpl, "main");
    if (!MainVar || !MainVar->Func)
    {
        Compiler_Error(Cmpl, "main function was not found\n");
        return;
    }
    Memory_WriteQWord(Cmpl->BCBuilder.Mem, Placeholder, MainVar->Func->Label);

    size_t StrCount = 0;
    for (size_t i = 0; i < (sizeof(Cmpl->StringDataList) / sizeof(Cmpl->StringDataList[0])); i++)
    {
        const char *String = Cmpl->StringDataList[i].String;
        if (String == NULL)
        {
            break;
        }

        for (size_t j = 0; j < strlen(String) + 1 /* null terminator */; j++)
        {
            Memory_WriteByte(Cmpl->BCBuilder.Mem, 2000 + StrCount++, String[j]);
        }
    }

    FILE *Out = fopen("out", "wb");
    Memory_FileWrite(Cmpl->BCBuilder.Mem, Out);
    fclose(Out);
}

