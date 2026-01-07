
#define PROG_LEN 256

#include <stdio.h>
#include "Lexer.h"
#include "Parser.h"
#include "Compiler.h"

int main(int argc, const char **argv)
{
    if (argc < 2)
    {
        printf("no input file\n");
        return 1;
    }

    FILE *f = fopen(argv[1], "rb");
    if (f == NULL)
    {
        printf("failed to open\n");
        return 1;
    }

    char Buffer[PROG_LEN + 1];
    size_t BytesRead = fread(Buffer, 1, sizeof(Buffer), f);
    (void)BytesRead;
    fclose(f);

    Buffer[sizeof(Buffer) - 1] = 0; // make sure its null terminated

    Lexer Lex = { Buffer, 0, NULL };
    Lexer_Tokenize(&Lex);

    // {
    //     TokNode *Node = Lex.Tokens;
    //     while (Node)
    //     {
    //         printf("  TokType %i, String '%s'\n", Node->Type, Node->String);
    //         Node = Node->Next;
    //     }
    // }

    Parser Parse = { Lex.Tokens, NULL };
    Parser_Parse(&Parse);

    Compiler Cmpl = { Parse.Ast, NULL, false, NULL, { {0} }, {0}, 0 };
    Compiler_Compile(&Cmpl);

    VarNode_FreeAll(Cmpl.Vars);
    StmtNode_FreeAllRecursive(Parse.Ast);
    TokNode_FreeAll(Lex.Tokens);

    if (Cmpl.HasErrors)
    {
        printf("compilation has finished with errors\n");
        return 1;
    }

    return 0;
}

