
#ifndef COMPILER_H
#define COMPILER_H

#include <stdio.h>
#include "Parser.h"
#include <stdbool.h>

#include "../furnvm/BytecodeBuilder.h"

typedef struct
{
    size_t Label;

    struct
    {
        const char *Name;
        TypeDesc Type;
    } Params[6];

    TypeDesc ReturnType;
} Function;

typedef struct VarNode VarNode;

struct VarNode
{
    char *Name;
    TypeDesc Type;
    QWord AddressOffset;
    Function *Func;
    VarNode *Next;
};

typedef struct
{
    TypeDesc Type;
    VarNode *Var;
} CmplSymbol;

typedef struct
{
    const char *String;
} StringData;


typedef struct
{
    StmtNode *Stmt;
    VarNode *Vars;
    bool HasErrors;
    TypeDesc *ReturnType;
    StringData StringDataList[10];
    BytecodeBuilder BCBuilder;
    size_t StackLoc;
} Compiler;

void Compiler_Compile(Compiler *Cmpl);

void VarNode_FreeAll(VarNode *List);

#endif // COMPILER_H

