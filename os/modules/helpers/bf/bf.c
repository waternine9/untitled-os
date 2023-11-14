#include "bf.h"

void WMPrintStr(char* Str);
void WMPrintChar(char Char);

size_t BFCodeSize(uint8_t* Code)
{
    size_t Size = 0;
    while (*Code++) Size++;
    return Size;
}

bool BFCodeStep(bf_tokenizer* Tokenizer)
{
    if (Tokenizer->At >= Tokenizer->Size - 7) return false;
    return true;
}

size_t BFTellNext(bf_tokenizer* Tokenizer, uint8_t Char)
{
    int I = Tokenizer->At;
    while (I < Tokenizer->Size)
    {
        if (Tokenizer->Code[I] == Char) return I;

        I++;
    }
    return 0xFFFFFFFF;
}

size_t BFTellNextMatching(bf_tokenizer* Tokenizer, uint8_t CharDec, uint8_t CharInc)
{
    size_t Counter = 0;
    int I = Tokenizer->At;
    while (I < Tokenizer->Size)
    {
        if (Tokenizer->Code[I] == CharInc) Counter++;
        if (Tokenizer->Code[I] == CharDec)
        {
            if (Counter == 1) return I;
            Counter--;
        }

        I++;
    }
    return 0xFFFFFFFF;
}

size_t BFTellNextSemi(bf_tokenizer* Tokenizer)
{
    size_t Counter = 0;
    int I = Tokenizer->At;
    while (I < Tokenizer->Size)
    {
        if (Tokenizer->Code[I] == '{') Counter++;
        if (Tokenizer->Code[I] == '}') Counter--;
        if (Tokenizer->Code[I] == '(') Counter++;
        if (Tokenizer->Code[I] == ')') Counter--;
        if (Tokenizer->Code[I] == ';')
        {
            if (Counter == 0) return I;
        }

        I++;
    }
    return 0xFFFFFFFF;
}

bool BFCmpName(bf_name X, bf_name Y)
{
    if (X.Len != Y.Len) return false;
    for (int I = 0; I < X.Len; I++)
    {
        if (X.Name[I] == '.') return true;
        if (Y.Name[I] == '.') return true;
        if (X.Name[I] != Y.Name[I]) return false;
    }
    return true;
}

bool BFIsOpChar(uint8_t C)
{
    switch (C)
    {
        case '+':
            return true;
        case '<':
            return true;
        case '>':
            return true;
        case '-':
            return true;
        case '*':
            return true;
        case '/':
            return true;
        case '=':
            return true;
        case '%':
            return true;
        default:
            return false;
    }
}

bf_token_type BFTokenizeOpType(bf_tokenizer *Tokenizer)
{
    String S = NewStr();
    while (Tokenizer->At < Tokenizer->Size)
    {
        if (!BFIsOpChar(Tokenizer->Code[Tokenizer->At])) break;
        StrPushBack(&S, Tokenizer->Code[Tokenizer->At]);
        Tokenizer->At++;
    }
    if (S.size == 1)
    {
        uint8_t C = S.data[0];
        switch (C)
        {
            case '+':
                return BFADD;
            case '-':
                return BFSUB;
            case '*':
                return BFMUL;
            case '/':
                return BFDIV;
            case '=':
                return BFASSIGN;
            case '<':
                return BFLESSTHAN;
            case '>':
                return BFMORETHAN;
            case '%':
                return BFMODULO;
            default:
                break;
        }
        return BFUNKNOWN;
    }
    if (StrEqualsWith(S, StrFromCStr("=="))) return BFEQUALS;
    else if (StrEqualsWith(S, StrFromCStr(">>"))) return BFSHR;
    else if (StrEqualsWith(S, StrFromCStr("<<"))) return BFSHL;
    return BFUNKNOWN;
}

size_t BFFindNextOperator(bf_tokenizer* Tokenizer)
{
    size_t OldTokAt = Tokenizer->At;
    for (size_t I = Tokenizer->At; I < Tokenizer->Size; I++)
    {
        if (BFIsOpChar(Tokenizer->Code[I]))
        {
            return I;
        }
        /*Tokenizer->At = I;
        if (Tokenizer->Code[I] == ';') return 0xFFFFFFFF;
        if (BFTokenizeOpType(Tokenizer) != BFUNKNOWN)
        {
            Tokenizer->At = OldTokAt;
            return I;
        }*/
    }
    return 0xFFFFFFFF;
}

size_t BFTellNextCom(bf_tokenizer* Tokenizer)
{
    int Counter = 0;
    for (size_t I = Tokenizer->At; I < Tokenizer->Size; I++)
    {
        if (Tokenizer->Code[I] == '(') Counter++;
        if (Tokenizer->Code[I] == ')') 
        {
            Counter--;
            if (Counter == -1) return 0xFFFFFFFF;
        }
        if (Tokenizer->Code[I] == ',')
        {
            if (Counter == 0) return I;
        }
    }
    return 0xFFFFFFFF;
}

size_t BFTellNextArrEnd(bf_tokenizer* Tokenizer)
{
    int Counter = 0;
    for (size_t I = Tokenizer->At; I < Tokenizer->Size; I++)
    {
        if (Tokenizer->Code[I] == '[') Counter++;
        if (Tokenizer->Code[I] == ']') 
        {
            Counter--;
            if (Counter == 0) return I;
        }
    }
    return 0xFFFFFFFF;
}

bool BFFindVariable(bf_name SearchFor, bf_scope* SearchIn, bf_variable** OutPtr)
{

    for (int I = 0; I < SearchIn->VarsCount; I++)
    {
        if (BFCmpName(SearchFor, SearchIn->Vars[I]->Name))
        {
            *OutPtr = SearchIn->Vars[I];
            return true;
        }
    }
    if (SearchIn->Parent == 0) return false;
    return BFFindVariable(SearchFor, SearchIn->Parent, OutPtr);
}



bf_token* BFTokenizeExpr(bf_tokenizer* Tokenizer, bf_scope* CurrentScope);

bf_token* BFTokenizeFuncCall(bf_tokenizer* Tokenizer, bf_scope* CurrentScope)
{
    bf_token* Token = (bf_token*)malloc(sizeof(bf_token));
    memset(Token, 0, sizeof(bf_token));
    size_t NextSqrBr = BFTellNext(Tokenizer, '(');
    size_t NextMatchingSqrBr = BFTellNextMatching(Tokenizer, ')', '(');
    Token->Type = BFFUNCCALL;
    bf_name FuncName;
    
    FuncName.Len = NextSqrBr - Tokenizer->At;
    FuncName.Name = Tokenizer->Code + Tokenizer->At;

    for (int I = 0; I < Tokenizer->BFNumFunctions; I++)
    {
        if (BFCmpName(FuncName, Tokenizer->Functions[I]->Name))
        {
            Token->CallFunction = Tokenizer->Functions[I];
            break;
        }
    }
    if (!Token->CallFunction) return NULL;

    Token->Params = (bf_token**)malloc(sizeof(bf_token*));

    
    Tokenizer->At = NextSqrBr + 1;

    size_t NextCom = BFTellNextCom(Tokenizer);

    if (NextSqrBr + 1 == NextMatchingSqrBr) return Token;

    bool BreakOut = false;

    int Step = Token->CallFunction->Params.size;

    while (Step--)
    {
        bf_token* Param = BFTokenizeExpr(Tokenizer, CurrentScope);

        Token->Params[Token->NumParams++] = Param;
        bf_token** NewParams = (bf_token**)malloc((Token->NumParams + 1) * sizeof(bf_token*));
        memcpy(NewParams, Token->Params, Token->NumParams * sizeof(bf_token*));
        free(Token->Params);
        Token->Params = NewParams;

        
        if (NextCom == 0xFFFFFFFF)
        {
            return Token;
        }
        else
        {
            Tokenizer->At = NextCom + 1;
            NextCom = BFTellNextCom(Tokenizer);
        }
    }
    return Token;

}

bool BFTokenizeType(bf_tokenizer *Tokenizer, bf_scope* Scope, bf_type* OutType)
{
    String S = NewStr();
    size_t OldTokAt = Tokenizer->At;
    bool IsFound = false;
    int ArrPos = -1;
    while (Tokenizer->At < Tokenizer->Size)
    {
        if (Tokenizer->Code[Tokenizer->At] == '[')
        {
            ArrPos = Tokenizer->At;
            IsFound = true;
        }
        if (Tokenizer->Code[Tokenizer->At] == ':')
        {
            Tokenizer->At++;
            IsFound = true;
            break;
        }
        else if (Tokenizer->Code[Tokenizer->At] == ';' || Tokenizer->Code[Tokenizer->At] == ')' || Tokenizer->Code[Tokenizer->At] == ',' || Tokenizer->Code[Tokenizer->At] == '(' || BFIsOpChar(Tokenizer->Code[Tokenizer->At]))
        {
            Tokenizer->At++;
            IsFound = false;
            break;
        }
        if (!IsFound) StrPushBack(&S, Tokenizer->Code[Tokenizer->At]);
        Tokenizer->At++;
    }
    if (IsFound)
    {  
        for (int I = 0;I < Tokenizer->Types.size;I++)
        {
            if (StrEqualsWith(((bf_type*)LinkedListGet(&Tokenizer->Types, I))->Name, S))
            {
                if (OutType) WMPrintStr("Tokenizing array");
                *OutType = *(bf_type*)LinkedListGet(&Tokenizer->Types, I);

                OutType->ArrData = 0;
                OutType->ArrSize = 0;

                if (Scope)
                {
                    size_t OldAt = Tokenizer->At;
                    if (ArrPos != -1)
                    {
                        Tokenizer->At = ArrPos + 1;
                        OutType->ArrSize = BFTokenizeExpr(Tokenizer, Scope);
                    }
                    Tokenizer->At = OldAt;
                    WMPrintStr("Success!");
                }

                return true;
            }
        }
    }
    else
    {
        Tokenizer->At = OldTokAt;
    }
    return false;
}

bf_token* BFTokenizeMemberAccess(bf_tokenizer* Tokenizer, bf_variable* CurVariable, bf_type CurType, size_t StopAt)
{
    while (Tokenizer->Code[Tokenizer->At] != '.')
    {
        if (Tokenizer->At == StopAt) return 0;
        Tokenizer->At++;
    }
    Tokenizer->At++;
    String Accum = NewStr();
    while (Tokenizer->Code[Tokenizer->At] != '.')
    {
        if (Tokenizer->At == StopAt) break;
        StrPushBack(&Accum, Tokenizer->Code[Tokenizer->At]);
        Tokenizer->At++;
    }
    if (Accum.size != 0)
    {
        bf_token* Token = (bf_token*)malloc(sizeof(bf_token));
        memset(Token, 0, sizeof(Token));
        Token->Type = BFMEMBERACCESS;
        for (int I = 0;I < CurType.StructMembers.size;I++)
        {
            if (StrEqualsWith(Accum, ((bf_type*)LinkedListGet(&CurType.StructMembers, I))->Name))
            {
                Token->Member = (bf_type*)LinkedListGet(&CurType.StructMembers, I);
                break;
            }
        }
        bf_token* Next = BFTokenizeMemberAccess(Tokenizer, CurVariable, Token->Member->Type, StopAt);
        if (Next == 0)
        {
            Token->IsFinalMember = true;
            return Token;
        }
        Token->First = Next;
        Token->IsFinalMember = false;
        return Token;
    }
    return 0;
}

bf_token* BFTokenizeIf(bf_tokenizer* Tokenizer, bf_scope* CurrentScope)
{
    size_t NextBr = BFTellNext(Tokenizer, '(');
    if (NextBr == 0xFFFFFFFF) 
    {
        WMPrintStr("ERR: if: Expected (\n");
        return 0;
    }
    size_t NextMatchingBr = BFTellNextMatching(Tokenizer, ')', '(');
    if (NextMatchingBr == 0xFFFFFFFF) 
    {
        WMPrintStr("ERR: if: Expected matching )\n");
        return 0;
    }
    Tokenizer->At = NextBr + 1;
    bf_token* Conditional = BFTokenizeExpr(Tokenizer, CurrentScope);
    if (Conditional == 0) 
    {
        WMPrintStr("ERR: if: Conditional failed to parse!\n");
        return 0;
    }
    Tokenizer->At = NextMatchingBr + 1;
    size_t NextMatchingSqrBr = BFTellNextMatching(Tokenizer, '}', '{');
    if (NextMatchingSqrBr == 0xFFFFFFFF) 
    {
        WMPrintStr("ERR: if: Expected matching }\n");
        return 0;
    }
    bf_token* Token = (bf_token*)malloc(sizeof(bf_token));
    memset(Token, 0, sizeof(bf_token));
    Token->Type = BFIF;
    Token->First = Conditional;
    bf_scope* IfScope = (bf_scope*)malloc(sizeof(bf_scope));
    IfScope->Lines = LinkedListCreate(sizeof(bf_token*));
    IfScope->Parent = CurrentScope;
    IfScope->Vars = (bf_variable**)malloc(sizeof(bf_variable*));
    IfScope->VarsCount = 0;
    IfScope->ArgsCount = 0;
    
    Token->NextScope = IfScope;

    Tokenizer->At++;
    
    while (Tokenizer->At < NextMatchingSqrBr)
    {
        size_t NextSemi = BFTellNextSemi(Tokenizer);
        bf_token* NewTok = BFTokenizeExpr(Tokenizer, IfScope);
        
        if (NewTok == 0) 
        {
            WMPrintStr("ERR: if: Line failed to parse!\n");
            return 0;
        }
        
        LinkedListPushBack(&IfScope->Lines, &NewTok);
        Tokenizer->At = NextSemi + 1;
    }
    return Token;
}
bf_token* BFTokenizeCast(bf_tokenizer* Tokenizer, bf_scope* CurrentScope)
{
    size_t NextBr = BFTellNext(Tokenizer, '(');
    if (NextBr == 0xFFFFFFFF) 
    {
        WMPrintStr("ERR: cast: Expected (\n");
        return 0;
    }
    size_t NextMatchingBr = BFTellNextMatching(Tokenizer, ')', '(');
    if (NextMatchingBr == 0xFFFFFFFF) 
    {
        WMPrintStr("ERR: cast: Expected matching )\n");
        return 0;
    }
    Tokenizer->At = NextBr + 1;
    bf_token* Token = (bf_token*)malloc(sizeof(bf_token));
    memset(Token, 0, sizeof(bf_token));
    Token->Type = BFCAST;
    bf_type OutType;
    if (BFTokenizeType(Tokenizer, CurrentScope, &OutType))
    {
        Token->CastTo = OutType;
    }
    else
    {

        Token->CastTo = *(bf_type*)LinkedListGet(&Tokenizer->Types, 0);
    }
    bf_token* Val = BFTokenizeExpr(Tokenizer, CurrentScope);
    if (Val == 0)
    {
        WMPrintStr("ERR: cast: Value to cast is not found\n");
    }
    Token->First = Val;
    Tokenizer->At = NextMatchingBr + 1;
    return Token;
}
bf_token* BFTokenizeReturn(bf_tokenizer* Tokenizer, bf_scope* CurrentScope)
{
    size_t NextBr = BFTellNext(Tokenizer, '(');
    if (NextBr == 0xFFFFFFFF) 
    {
        WMPrintStr("ERR: return: Expected (\n");
        return 0;
    }
    size_t NextMatchingBr = BFTellNextMatching(Tokenizer, ')', '(');
    if (NextMatchingBr == 0xFFFFFFFF) 
    {
        WMPrintStr("ERR: return: Expected matching )\n");
        return 0;
    }
    Tokenizer->At = NextBr + 1;
    bf_token* Token = (bf_token*)malloc(sizeof(bf_token));
    memset(Token, 0, sizeof(bf_token));
    Token->Type = BFRETURN;
    bf_token* Val = BFTokenizeExpr(Tokenizer, CurrentScope);
    if (Val == 0)
    {
        WMPrintStr("ERR: return: Value to return is not found\n");
    }
    Token->First = Val;
    Tokenizer->At = NextMatchingBr + 1;
    return Token;
}

bf_token* BFTokenizePrint(bf_tokenizer* Tokenizer, bf_scope* CurrentScope)
{
    size_t NextBr = BFTellNext(Tokenizer, '(');
    if (NextBr == 0xFFFFFFFF) 
    {
        WMPrintStr("ERR: print: Expected (\n");
        return 0;
    }
    size_t NextMatchingBr = BFTellNextMatching(Tokenizer, ')', '(');
    if (NextMatchingBr == 0xFFFFFFFF)
    {
        WMPrintStr("ERR: print: Expected matching )\n");
        return 0;
    }
    Tokenizer->At = NextBr + 1;
    bf_token* Character = BFTokenizeExpr(Tokenizer, CurrentScope);
    if (Character == 0) 
    {
        WMPrintStr("ERR: print: Character failed to parse!\n");
        return 0;
    }
    bf_token* Token = (bf_token*)malloc(sizeof(bf_token));
    memset(Token, 0, sizeof(bf_token));
    Token->Type = BFPRINT;
    Token->First = Character;
    Token->IsConstant = 0;
    Token->IsVar = 0;
    return Token;
}


bf_token* BFTokenizeWhile(bf_tokenizer* Tokenizer, bf_scope* CurrentScope)
{
    size_t NextBr = BFTellNext(Tokenizer, '(');
    if (NextBr == 0xFFFFFFFF) 
    {
        WMPrintStr("ERR: while: Expected (\n");
        return 0;
    }
    size_t NextMatchingBr = BFTellNextMatching(Tokenizer, ')', '(');
    if (NextMatchingBr == 0xFFFFFFFF) 
    {
        WMPrintStr("ERR: while: Expected matching )\n");
        return 0;
    }
    Tokenizer->At = NextBr + 1;
    bf_token* Conditional = BFTokenizeExpr(Tokenizer, CurrentScope);
    if (Conditional == 0) 
    {
        WMPrintStr("ERR: while: Conditional failed to parse!\n");
        return 0;
    }
    Tokenizer->At = NextMatchingBr + 1;
    size_t NextMatchingSqrBr = BFTellNextMatching(Tokenizer, '}', '{');
    if (NextMatchingSqrBr == 0xFFFFFFFF) 
    {
        WMPrintStr("ERR: while: Expected matching }\n");
        return 0;
    }
    bf_token* Token = (bf_token*)malloc(sizeof(bf_token));
    memset(Token, 0, sizeof(bf_token));
    Token->Type = BFWHILE;
    Token->First = Conditional;
    bf_scope* IfScope = (bf_scope*)malloc(sizeof(bf_scope));
    IfScope->Lines = LinkedListCreate(sizeof(bf_token));
    IfScope->Parent = CurrentScope;
    IfScope->Vars = (bf_variable**)malloc(sizeof(bf_variable*));
    IfScope->VarsCount = 0;
    IfScope->ArgsCount = 0;
    
    Token->NextScope = IfScope;

    Tokenizer->At++;
    
    while (Tokenizer->At < NextMatchingSqrBr)
    {
        size_t NextSemi = BFTellNextSemi(Tokenizer);
        bf_token* NewTok = BFTokenizeExpr(Tokenizer, IfScope);
        
        if (NewTok == 0) 
        {
            WMPrintStr("ERR: while: Line failed to parse!\n");
            return 0;
        }
        
        LinkedListPushBack(&IfScope->Lines, &NewTok);
        Tokenizer->At = NextSemi + 1;
    }
    return Token;
}

bf_token* BFTokenizeFor(bf_tokenizer* Tokenizer, bf_scope* CurrentScope)
{
    size_t NextBr = BFTellNext(Tokenizer, '(');
    if (NextBr == 0xFFFFFFFF) 
    {
        WMPrintStr("ERR: for: Expected (\n");
        return 0;
    }
    size_t NextMatchingBr = BFTellNextMatching(Tokenizer, ')', '(');
    if (NextMatchingBr == 0xFFFFFFFF) 
    {
        WMPrintStr("ERR: for: Expected matching )\n");
        return 0;
    }
    
    bf_token* Token = (bf_token*)malloc(sizeof(bf_token));
    memset(Token, 0, sizeof(bf_token));
    Token->Type = BFFOR;

    Tokenizer->At = NextBr + 1;
    bf_token* Initial = BFTokenizeExpr(Tokenizer, CurrentScope);
    if (Initial == 0) 
    {
        WMPrintStr("ERR: for: Initial failed to parse!\n");
        return 0;
    }

    Token->First = Initial;

    size_t NextSemi = BFTellNext(Tokenizer, ';');
    if (NextSemi == 0xFFFFFFFF || NextSemi > NextMatchingBr)
    {
        WMPrintStr("ERR: for: Expected first semicolon\n");
        return 0;
    }
    Tokenizer->At = NextSemi + 1;
    if (Tokenizer->Code[Tokenizer->At] != ';')
    {
        bf_token* Conditional = BFTokenizeExpr(Tokenizer, CurrentScope);
        if (Conditional == 0) 
        {
            WMPrintStr("ERR: for: Conditional failed to parse!\n");
            return 0;
        }
        Token->Second = Conditional;
    }
    NextSemi = BFTellNext(Tokenizer, ';');
    if (NextSemi == 0xFFFFFFFF || NextSemi > NextMatchingBr)
    {
        WMPrintStr("ERR: for: Expected second semicolon\n");
        return 0;
    }
    Tokenizer->At = NextSemi + 1;
    if (Tokenizer->Code[Tokenizer->At] != ')')
    {
        bf_token* Iterator = BFTokenizeExpr(Tokenizer, CurrentScope);
        if (Iterator == 0) 
        {
            WMPrintStr("ERR: for: Iterator failed to parse!\n");
            return 0;
        }
        Token->Third = Iterator;
    }
    Tokenizer->At = NextMatchingBr + 1;
    size_t NextMatchingSqrBr = BFTellNextMatching(Tokenizer, '}', '{');
    if (NextMatchingSqrBr == 0xFFFFFFFF) 
    {
        WMPrintStr("ERR: for: Expected matching }\n");
        return 0;
    }
    
    bf_scope* IfScope = (bf_scope*)malloc(sizeof(bf_scope));
    IfScope->Lines = LinkedListCreate(sizeof(bf_token*));
    IfScope->Parent = CurrentScope;
    IfScope->Vars = (bf_variable**)malloc(sizeof(bf_variable*));
    IfScope->VarsCount = 0;
    IfScope->ArgsCount = 0;
    
    Token->NextScope = IfScope;

    Tokenizer->At++;
    
    while (Tokenizer->At < NextMatchingSqrBr)
    {
        size_t NextSemi = BFTellNextSemi(Tokenizer);
        bf_token* NewTok = BFTokenizeExpr(Tokenizer, IfScope);
        
        if (NewTok == 0) 
        {
            WMPrintStr("ERR: for: Line failed to parse!\n");
            return 0;
        }
        
        LinkedListPushBack(&IfScope->Lines, &NewTok);
        Tokenizer->At = NextSemi + 1;
    }
    return Token;
}

bf_token* BFTokenizeExpr(bf_tokenizer* Tokenizer, bf_scope* CurrentScope)
{
    char s[10];
    for (int i = Tokenizer->At;i < Tokenizer->At + 9;i++)
    {
        s[i - Tokenizer->At] = Tokenizer->Code[i];
    }
    s[9] = 0;
    WMPrintStr(s);

    bf_token* Tok = (bf_token*)malloc(sizeof(bf_token));
    memset(Tok, 0, sizeof(bf_token));

    bf_type IsTyped_Type;
    bool IsTyped = BFTokenizeType(Tokenizer, CurrentScope, &IsTyped_Type);

    if (!IsTyped)
    {
        if (Tokenizer->Code[Tokenizer->At] == '(')
        {

            size_t NextMatching = BFTellNextMatching(Tokenizer, ')', '(');

            if (NextMatching == 0xFFFFFFFF)
            {
                free(Tok);
                return NULL;
            }
            Tokenizer->At++;
            bf_token* NextToken = BFTokenizeExpr(Tokenizer, CurrentScope);
            Tokenizer->At = NextMatching + 1;
            
            if (Tokenizer->Code[Tokenizer->At] == ';' || Tokenizer->Code[Tokenizer->At] == ')')
            {
                free(Tok);
                return NextToken;
            }
            Tok->First = NextToken;
            Tok->Type = BFTokenizeOpType(Tokenizer);
            Tok->Second = BFTokenizeExpr(Tokenizer, CurrentScope);
            return Tok;
        }
    }
    size_t NextOp = BFFindNextOperator(Tokenizer);
    size_t NextSemi = BFTellNext(Tokenizer, ';');
    size_t NextBr = BFTellNext(Tokenizer, ')');
    size_t NextRSqrBr = BFTellNext(Tokenizer, ')');
    size_t NextArrEnd = BFTellNextArrEnd(Tokenizer);
    size_t NextCom = BFTellNext(Tokenizer, ',');
    if (NextOp < NextBr) NextBr = NextOp;
    if (NextSemi < NextBr) NextBr = NextSemi;
    if (NextRSqrBr < NextBr) NextBr = NextRSqrBr;
    if (NextCom < NextBr) NextBr = NextCom;
    if (NextArrEnd < NextBr) NextBr = NextArrEnd;

    size_t NextSqrBr = BFTellNext(Tokenizer, '(');
    

    if (NextSqrBr != 0xFFFFFFFF)
    {
        String CheckStr = NewStr();
        for (int I = Tokenizer->At;I < NextSqrBr;I++)
        {
            StrPushBack(&CheckStr, Tokenizer->Code[I]);
        }
        if (StrEqualsWith(CheckStr, StrFromCStr("if")))
        {
            free(CheckStr.data);
            return BFTokenizeIf(Tokenizer, CurrentScope);
        }
        else if (StrEqualsWith(CheckStr, StrFromCStr("while")))
        {
            free(CheckStr.data);
            return BFTokenizeWhile(Tokenizer, CurrentScope);
        }
        else if (StrEqualsWith(CheckStr, StrFromCStr("for")))
        {
            free(CheckStr.data);
            return BFTokenizeFor(Tokenizer, CurrentScope);
        }
        else if (StrEqualsWith(CheckStr, StrFromCStr("print")))
        {
            free(CheckStr.data);
            return BFTokenizePrint(Tokenizer, CurrentScope);
        }
        else if (StrEqualsWith(CheckStr, StrFromCStr("cast")))
        {
            free(CheckStr.data);
            return BFTokenizeCast(Tokenizer, CurrentScope);
        }
        else if (StrEqualsWith(CheckStr, StrFromCStr("return")))
        {
            free(CheckStr.data);
            return BFTokenizeReturn(Tokenizer, CurrentScope);
        }
        free(CheckStr.data);
    }
    if (NextSqrBr < NextBr && NextRSqrBr != 0xFFFFFFFF)
    {
        free(Tok);
        Tok = BFTokenizeFuncCall(Tokenizer, CurrentScope);
        Tokenizer->At = NextRSqrBr + 1;
    }
    else
    {
        if (('0' <= Tokenizer->Code[Tokenizer->At] && Tokenizer->Code[Tokenizer->At] <= '9') || Tokenizer->Code[Tokenizer->At] == '~')
        {
            int Multiplier = 1;
            if (Tokenizer->Code[Tokenizer->At] == '~') 
            {
                Multiplier *= -1;
                Tokenizer->At++;
            }
            int Val = 0;
            for (int I = NextBr - 1;I >= Tokenizer->At;I--)
            {
                int C = Tokenizer->Code[I];
                if (C < '0' || C > '9') 
                {
                    return 0;
                }
                Val += Multiplier * (C - '0');
                Multiplier *= 10;
            }
            Tok->IsConstant = true;
            Tok->Const.type = *(bf_type*)LinkedListGet(&Tokenizer->Types, 0);
            Tok->Const.intVal = Val;
            Tok->Const.isReturn = false;
        }
        else
        {
            bf_variable* OutPtr;
            bf_name VarName;
            VarName.Name = Tokenizer->Code + Tokenizer->At;
            VarName.Len = NextBr - Tokenizer->At;
            size_t OldVarLen = VarName.Len;
            
            size_t IdxPos = 0;

            for (int I = 0;I < VarName.Len;I++)
            {
                if (VarName.Name[I] == '[')
                {
                    IdxPos = I + Tokenizer->At;
                    VarName.Len = I;
                    break;
                }

                if (VarName.Name[I] == '.')
                {
                    VarName.Len = I;
                    break;
                }
            }

            if (BFFindVariable(VarName, CurrentScope, &OutPtr))
            {
                Tok->IsVar = true;
                Tok->Var = OutPtr;
                size_t OldTokAt = Tokenizer->At;
                bf_token* IfMemberAccess = BFTokenizeMemberAccess(Tokenizer, OutPtr, OutPtr->Type, Tokenizer->At + OldVarLen);
                if (IfMemberAccess != 0)
                {
                    Tok->Type = BFMEMBERACCESS;
                    Tok->First = IfMemberAccess;
                    Tok->IsFinalMember = false;
                }
                Tokenizer->At = OldTokAt;
            }
            else
            {
                

                bf_variable* NewVar = (bf_variable*)malloc(sizeof(bf_variable));
                NewVar->Name = VarName;
                
                NewVar->Val = (bf_constant){ 0 };
                if (IsTyped) NewVar->Type = IsTyped_Type;
                else NewVar->Type = *(bf_type*)LinkedListGet(&Tokenizer->Types, 0);
                
                CurrentScope->Vars[CurrentScope->VarsCount++] = NewVar;
                bf_variable** NewVars = (bf_variable**)malloc(sizeof(bf_variable*) * (CurrentScope->VarsCount + 1));
                memcpy(NewVars, CurrentScope->Vars, sizeof(bf_variable*) * CurrentScope->VarsCount);
                free(CurrentScope->Vars);
                CurrentScope->Vars = NewVars;

                Tok->IsVar = true;
                Tok->Var = NewVar;
            }

            if (Tok->Var->Type.ArrSize && IdxPos > 0)
            {
                size_t OldTokAt = Tokenizer->At;
                Tokenizer->At = IdxPos + 1;
                Tok->First = BFTokenizeExpr(Tokenizer, CurrentScope);
                Tokenizer->At = OldTokAt;
            }
        }
    }
    
    if (NextBr < NextOp || NextOp == 0xFFFFFFFF)
    {
        return Tok;
    }

    bf_token* SurroundTok = (bf_token*)malloc(sizeof(bf_token));
    SurroundTok->First = Tok;

    NextOp = BFFindNextOperator(Tokenizer);
    Tokenizer->At = NextOp;
    SurroundTok->Type = BFTokenizeOpType(Tokenizer);
    
    
    
    SurroundTok->Second = BFTokenizeExpr(Tokenizer, CurrentScope);
    return SurroundTok;
}

bf_token* BFTokenizeLine(bf_tokenizer* Tokenizer, bf_scope* CurrentScope)
{

    return BFTokenizeExpr(Tokenizer, CurrentScope);
}

bf_member BFTokenizeStructMember(bf_tokenizer* Tokenizer, bf_scope* CurrentScope)
{
    bf_member CurMember;
    bf_type IsTyped_Type;
    bool IsTyped = BFTokenizeType(Tokenizer, CurrentScope, &IsTyped_Type);
    if (!IsTyped) return CurMember;
    size_t NextSemi = BFTellNext(Tokenizer, ';');
    String CurMemberName = StrFromArray((char*)Tokenizer->Code + Tokenizer->At, NextSemi - Tokenizer->At); 
    CurMember.Name = CurMemberName;
    CurMember.Type = IsTyped_Type;
    Tokenizer->At = NextSemi + 1;
    return CurMember;
}

void BFTokenizeStruct(bf_tokenizer* Tokenizer, bf_scope* CurrentScope)
{
    size_t EndBr = BFTellNext(Tokenizer, '}');
    if (EndBr == 0xFFFFFFFF) return;
    size_t BeginBr = BFTellNext(Tokenizer, '{');
    bf_type CurType;
    CurType.IsStruct = true;
    CurType.StructMembers = LinkedListCreate(sizeof(bf_member));
    CurType.Name = StrFromArray((char*)Tokenizer->Code + Tokenizer->At, BeginBr - Tokenizer->At);
    Tokenizer->At = BeginBr + 1;
    while (Tokenizer->At < EndBr)
    {
        bf_member member = BFTokenizeStructMember(Tokenizer, CurrentScope);
        LinkedListPushBack(&CurType.StructMembers, &member);
    }
    Tokenizer->At = EndBr + 1;
    LinkedListPushBack(&Tokenizer->Types, &CurType);
}

void BFTokenizeFunction(bf_tokenizer* Tokenizer)
{
    
    size_t NextSqrBr = BFTellNext(Tokenizer, '(');
    size_t NextCurBr = BFTellNext(Tokenizer, '{');
    if (NextCurBr < NextSqrBr)
    {
        BFTokenizeStruct(Tokenizer, 0);
        return;
    }
    bf_type OutReturnType;
    bool isReturnTyped = BFTokenizeType(Tokenizer, 0, &OutReturnType);
    
    if (NextSqrBr == 0xFFFFFFFF) return;
    
    bf_function* CurrentFunction = (bf_function*)malloc(sizeof(bf_function));
    CurrentFunction->Name.Name = Tokenizer->Code + Tokenizer->At;
    CurrentFunction->Name.Len = NextSqrBr - Tokenizer->At;
    CurrentFunction->RootScope.Lines = LinkedListCreate(sizeof(bf_token*));
    CurrentFunction->RootScope.Parent = 0;
    if (isReturnTyped) CurrentFunction->Type = OutReturnType;
    else CurrentFunction->Type = *(bf_type*)LinkedListGet(&Tokenizer->Types, 0);
    Tokenizer->At = NextSqrBr + 1;
    size_t NextSqrBr2 = BFTellNext(Tokenizer, ')');

    if (NextSqrBr2 == 0xFFFFFFFF) return;


    // Parse Arguments

    CurrentFunction->RootScope.Vars = (bf_variable**)malloc(sizeof(bf_variable*));
    CurrentFunction->RootScope.ArgsCount = 0;
    CurrentFunction->Params = LinkedListCreate(sizeof(bf_type_name*));

    if (NextSqrBr + 1 != NextSqrBr2)
    {
        while (1)
        {
            size_t NextCom = BFTellNext(Tokenizer, ',');
            bool BreakOut = NextCom > NextSqrBr2 || NextCom == 0xFFFFFFFF;
            if (BreakOut)
            {
                NextCom = NextSqrBr2;
            }
            bf_variable *Var = (bf_variable*)malloc(sizeof(bf_variable));
            
            if (Tokenizer->Code[Tokenizer->At] == '&')
            {
                Tokenizer->At++;
                Var->Ref = true;
            }
            else
            {
                Var->Ref = false;
            }
            
            bf_type OutType;
            bool IsTyped = BFTokenizeType(Tokenizer, 0, &OutType);
            Var->Name.Name = Tokenizer->Code + Tokenizer->At;
            Var->Name.Len = NextCom - Tokenizer->At;
            Var->Val = (bf_constant){ 0 };
            if (IsTyped) Var->Type = OutType;
            else Var->Type = *(bf_type*)LinkedListGet(&Tokenizer->Types, 0);
            bf_type_name *_Param = (bf_type_name*)malloc(sizeof(bf_name));
            _Param->Name = NewStr();
            if (IsTyped) _Param->Type = OutType;
            else _Param->Type = *(bf_type*)LinkedListGet(&Tokenizer->Types, 0);

            for (int I = 0;I < Var->Name.Len;I++)
            {
                StrPushBack(&_Param->Name, &Var->Name.Name[I]);
            }

            LinkedListPushBack(&CurrentFunction->Params, &_Param);
            CurrentFunction->RootScope.Vars[CurrentFunction->RootScope.ArgsCount++] = Var;
            bf_variable** NewArgs = (bf_variable**)malloc(sizeof(bf_variable*) * (CurrentFunction->RootScope.ArgsCount + 1));
            memcpy(NewArgs, CurrentFunction->RootScope.Vars, sizeof(bf_variable*) * CurrentFunction->RootScope.ArgsCount);
            free(CurrentFunction->RootScope.Vars);
            CurrentFunction->RootScope.Vars = NewArgs;
            if (BreakOut) break;
            Tokenizer->At = NextCom + 1;
            

        }
    }

    CurrentFunction->RootScope.VarsCount = CurrentFunction->RootScope.ArgsCount;

    Tokenizer->At = NextSqrBr2 + 1;
    size_t NextMatchingBr = BFTellNextMatching(Tokenizer, '}', '{');
    
    if (NextMatchingBr == 0xFFFFFFFF) return;

    Tokenizer->At++;


    while (Tokenizer->At < NextMatchingBr)
    {
        size_t NextSemi = BFTellNextSemi(Tokenizer);
        if (NextSemi == 0xFFFFFFFF) return;
        bf_token* Line = BFTokenizeLine(Tokenizer, &CurrentFunction->RootScope);
        LinkedListPushBack(&CurrentFunction->RootScope.Lines, &Line);

        Tokenizer->At = NextSemi + 1;
    }
    Tokenizer->At = NextMatchingBr + 1;
    Tokenizer->Functions[Tokenizer->BFNumFunctions++] = CurrentFunction;
    bf_function** NewFuncs = (bf_function**)malloc(sizeof(bf_function*) * (Tokenizer->BFNumFunctions + 1));
    memset(NewFuncs, 0, sizeof(bf_function*) * (Tokenizer->BFNumFunctions + 1));
    memcpy(NewFuncs, Tokenizer->Functions, sizeof(bf_function*) * Tokenizer->BFNumFunctions);
    free(Tokenizer->Functions);
    Tokenizer->Functions = NewFuncs;
}

// Here we go!
bf_function** BFTokenize(uint8_t* Code, size_t Len)
{
    bf_tokenizer Tokenizer;
    Tokenizer.At = 0;
    Tokenizer.Code = Code;
    Tokenizer.Size = Len;
    Tokenizer.Functions = (bf_function**)malloc(sizeof(bf_function*));

    // First, lets initialize the default types.
    Tokenizer.Types = LinkedListCreate(sizeof(bf_type));
    bf_type IntType;
    IntType.IsFloat = false;
    IntType.IsStruct = false;
    IntType.Bits = BFBITS32;
    IntType.Name = StrFromCStr("int");
    LinkedListPushBack(&Tokenizer.Types, &IntType);
    bf_type shortType;
    shortType.IsFloat = false;
    shortType.IsStruct = false;
    shortType.Bits = BFBITS16;
    shortType.Name = StrFromCStr("short");
    LinkedListPushBack(&Tokenizer.Types, &shortType);
    bf_type byteType;
    byteType.IsFloat = false;
    byteType.IsStruct = false;
    byteType.Bits = BFBITS8;
    byteType.Name = StrFromCStr("byte");
    LinkedListPushBack(&Tokenizer.Types, &byteType);

    Tokenizer.BFNumFunctions = 0;
    while (BFCodeStep(&Tokenizer))
    {
        BFTokenizeFunction(&Tokenizer);
    }
    
    return Tokenizer.Functions;
}

bf_constant BFExecuteFunc(bf_function* Func);

bool BFTypeEquals(bf_type x, bf_type y)
{
    if (x.IsStruct != y.IsStruct) return false;
    if (x.IsStruct) 
    {
        if (!StrEqualsWith(x.Name, y.Name)) return false;
        return true;
    }
    if (x.IsFloat != y.IsFloat) return false;
    if (x.IsFloat) return true;
    if (x.Bits != y.Bits) return false;
    return true;
}

bf_constant BFAddVal(bf_constant x, bf_constant y)
{
    if (!BFTypeEquals(x.type, y.type)) 
    {
        WMPrintStr("bf: ADD Type mismatch");
        return (bf_constant){ 0 };
    }
    if (x.type.IsStruct)
    {
        WMPrintStr("bf: Cant add structs");
        return (bf_constant){ 0 };
    }
    x.floatVal += y.floatVal;
    x.byteVal += y.byteVal;
    x.intVal += y.intVal;
    x.shortVal += y.shortVal;
    return x;
}

bf_constant BFSubVal(bf_constant x, bf_constant y)
{
    if (!BFTypeEquals(x.type, y.type)) 
    {
        WMPrintStr("bf: SUB Type mismatch");
        return (bf_constant){ 0 };
    }
    if (x.type.IsStruct)
    {
        WMPrintStr("bf: Cant subtract structs");
        return (bf_constant){ 0 };
    }
    x.floatVal -= y.floatVal;
    x.byteVal -= y.byteVal;
    x.intVal -= y.intVal;
    x.shortVal -= y.shortVal;
    return x;
}

bf_constant BFMulVal(bf_constant x, bf_constant y)
{
    if (!BFTypeEquals(x.type, y.type)) 
    {
        WMPrintStr("bf: MUL Type mismatch");
        return (bf_constant){ 0 };
    }
    if (x.type.IsStruct)
    {
        WMPrintStr("bf: Cant mul structs");
        return (bf_constant){ 0 };
    }

    x.floatVal *= y.floatVal;
    x.byteVal *= y.byteVal;
    x.intVal *= y.intVal;
    x.shortVal *= y.shortVal;
    return x;
}

bf_constant BFDivVal(bf_constant x, bf_constant y)
{
    if (!BFTypeEquals(x.type, y.type)) 
    {
        WMPrintStr("bf: DIV Type mismatch");
        return (bf_constant){ 0 };
    }
    if (x.type.IsStruct)
    {
        WMPrintStr("bf: Cant div structs");
        return (bf_constant){ 0 };
    }

    x.floatVal /= y.floatVal;
    if (y.byteVal != 0) x.byteVal /= y.byteVal;
    if (y.intVal != 0) x.intVal /= y.intVal;
    if (y.shortVal != 0) x.shortVal /= y.shortVal;
    return x;
}

bf_constant BFModVal(bf_constant x, bf_constant y)
{
    if (!BFTypeEquals(x.type, y.type)) 
    {
        WMPrintStr("bf: MOD Type mismatch");
        return (bf_constant){ 0 };
    }
    if (x.type.IsStruct)
    {
        WMPrintStr("bf: Cant mod structs");
        return (bf_constant){ 0 };
    }

    if (y.byteVal != 0) x.byteVal %= y.byteVal;
    if (y.intVal != 0) x.intVal %= y.intVal;
    if (y.shortVal != 0) x.shortVal %= y.shortVal;
    return x;
}

bf_constant BFShlVal(bf_constant x, bf_constant y)
{
    if (!BFTypeEquals(x.type, y.type)) 
    {
        WMPrintStr("bf: SHL Type mismatch");
        return (bf_constant){ 0 };
    }
    if (x.type.IsStruct || x.type.IsFloat)
    {
        WMPrintStr("bf: Cant shift floats/structs");
        return (bf_constant){ 0 };
    }

    x.byteVal <<= y.byteVal;
    x.intVal <<= y.intVal;
    x.shortVal <<= y.shortVal;
    return x;
}

bf_constant BFShrVal(bf_constant x, bf_constant y)
{
    if (!BFTypeEquals(x.type, y.type)) 
    {
        WMPrintStr("bf: SHR Type mismatch");
        return (bf_constant){ 0 };
    }
    if (x.type.IsStruct || x.type.IsFloat)
    {
        WMPrintStr("bf: Cant shift floats/structs");
        return (bf_constant){ 0 };
    }

    x.byteVal >>= y.byteVal;
    x.intVal >>= y.intVal;
    x.shortVal >>= y.shortVal;
    return x;
}

bf_constant BFLTVal(bf_constant x, bf_constant y)
{
    if (!BFTypeEquals(x.type, y.type)) 
    {
        WMPrintStr("bf: Type mismatch");
        return (bf_constant){ 0 };
    }
    if (x.type.IsStruct || x.type.IsFloat)
    {
        WMPrintStr("bf: Cant compare floats/structs");
        return (bf_constant){ 0 };
    }

    x.byteVal = x.byteVal < y.byteVal;
    x.intVal = x.intVal < y.intVal;
    x.shortVal = x.shortVal < y.shortVal;

    return x;
}

bf_constant BFGTVal(bf_constant x, bf_constant y)
{
    if (!BFTypeEquals(x.type, y.type)) 
    {
        WMPrintStr("bf: Type mismatch");
        return (bf_constant){ 0 };
    }
    if (x.type.IsStruct || x.type.IsFloat)
    {
        WMPrintStr("bf: Cant compare floats/structs");
        return (bf_constant){ 0 };
    }

    x.byteVal = x.byteVal > y.byteVal;
    x.intVal = x.intVal > y.intVal;
    x.shortVal = x.shortVal > y.shortVal;

    return x;
}

bf_constant BFEQVal(bf_constant x, bf_constant y)
{
    if (!BFTypeEquals(x.type, y.type)) 
    {
        WMPrintStr("bf: Type mismatch");
        return (bf_constant){ 0 };
    }
    if (x.type.IsStruct || x.type.IsFloat)
    {
        WMPrintStr("bf: Cant compare floats/structs");
        return (bf_constant){ 0 };
    }

    x.byteVal = x.byteVal == y.byteVal;
    x.intVal = x.intVal == y.intVal;
    x.shortVal = x.shortVal == y.shortVal;

    return x;
}

bf_constant BFCastVal(bf_constant x, bf_type castTo)
{
    if (BFTypeEquals(x.type, castTo)) return x;

    if (x.type.IsStruct)
    {
        WMPrintStr("bf: Cant cast structs");
        return (bf_constant){ 0 };
    }
    
    if (castTo.IsStruct)
    {
        WMPrintStr("bf: Cant cast to structs");
        return (bf_constant){ 0 };
    }

    if (!x.type.IsFloat)
    {
        int Cvt;
        if (x.type.Bits == BFBITS8) Cvt = x.byteVal;
        else if (x.type.Bits == BFBITS16) Cvt = x.shortVal;
        else if (x.type.Bits == BFBITS32) Cvt = x.intVal;

        if (castTo.IsFloat)
        {
            x.floatVal = Cvt;
        }
        else if (castTo.Bits == BFBITS8)
        {
            x.byteVal = Cvt;
        }
        else if (castTo.Bits == BFBITS16)
        {
            x.shortVal = Cvt;
        }
        else if (castTo.Bits == BFBITS32)
        {
            x.intVal = Cvt;
        }
    }
    else
    {
        float Cvt = x.floatVal;

        if (castTo.Bits == BFBITS8)
        {
            x.byteVal = Cvt;
        }
        else if (castTo.Bits == BFBITS16)
        {
            x.shortVal = Cvt;
        }
        else if (castTo.Bits == BFBITS32)
        {
            x.intVal = Cvt;
        }
    }

    x.type = castTo;

    return x;
}

bf_constant BFExecuteToken(bf_token* Tok)
{
    if (Tok->IsConstant) 
    {
        return Tok->Const;
    }
    if (Tok->IsVar)
    {
        if (Tok->Var->Val.type.ArrSize)
        {
            size_t ElemSize = 1;
            if (Tok->Var->Val.type.IsFloat) ElemSize = 4;
            else if (Tok->Var->Val.type.Bits == BFBITS32) ElemSize = 4;
            else if (Tok->Var->Val.type.Bits == BFBITS16) ElemSize = 2;
            
            if (!Tok->Var->Val.type.ArrData)
            {
                bf_constant ArrSize = BFExecuteToken(Tok->Var->Val.type.ArrSize);

                if (ArrSize.type.IsFloat || ArrSize.type.IsStruct)
                {
                    WMPrintStr("bf: Cant size arrays with floats/structs");
                    return (bf_constant){ 0 };
                }

                if (ArrSize.type.Bits == BFBITS32) Tok->Var->Val.type.ArrData = malloc(ElemSize * ArrSize.intVal);
                else if (ArrSize.type.Bits == BFBITS16) Tok->Var->Val.type.ArrData = malloc(ElemSize * ArrSize.shortVal);
                else if (ArrSize.type.Bits == BFBITS8) Tok->Var->Val.type.ArrData = malloc(ElemSize * ArrSize.byteVal);
            }
            
            bf_constant ArrIdx = BFExecuteToken(Tok->First);

            if (ArrIdx.type.IsFloat || ArrIdx.type.IsStruct)
            {
                WMPrintStr("bf: Cant index arrays with floats/structs");
                return (bf_constant){ 0 };
            }

            int Idx = 0;
            if (ArrIdx.type.Bits == BFBITS32) Idx = ArrIdx.intVal;
            else if (ArrIdx.type.Bits == BFBITS16) Idx = ArrIdx.shortVal;
            else if (ArrIdx.type.Bits == BFBITS8) Idx = ArrIdx.byteVal;
            
            if (Tok->Var->Val.type.IsFloat) 
            {
                Tok->Var->Val.floatVal = *(float*)((uint8_t*)Tok->Var->Val.type.ArrData + Idx * 4);
                Tok->Var->Val.assignDest = ((uint8_t*)Tok->Var->Val.type.ArrData + Idx * 4);
            }
            else if (Tok->Var->Val.type.Bits == BFBITS32)
            {
                Tok->Var->Val.intVal = *(int*)((uint8_t*)Tok->Var->Val.type.ArrData + Idx * 4);
                Tok->Var->Val.assignDest = ((uint8_t*)Tok->Var->Val.type.ArrData + Idx * 4);
            }
            else if (Tok->Var->Val.type.Bits == BFBITS16)
            {
                Tok->Var->Val.shortVal = *(int16_t*)((uint8_t*)Tok->Var->Val.type.ArrData + Idx * 2);
                Tok->Var->Val.assignDest = ((uint8_t*)Tok->Var->Val.type.ArrData + Idx * 2);
            }
            else if (Tok->Var->Val.type.Bits == BFBITS8)
            {
                Tok->Var->Val.byteVal = *(char*)((uint8_t*)Tok->Var->Val.type.ArrData + Idx * 1);
                Tok->Var->Val.assignDest = ((uint8_t*)Tok->Var->Val.type.ArrData + Idx * 1);
            }
        }
        return Tok->Var->Val;
    }
    if (Tok->Type == BFRETURN)
    {
        bf_constant Val = BFExecuteToken(Tok->First);
        Val.isReturn = true;
        return Val;
    }
    if (Tok->Type == BFCAST)
    {
        return BFCastVal(BFExecuteToken(Tok->First), Tok->CastTo);
    }
    if (Tok->Type == BFASSIGN)
    {
        if (Tok->First->Var) 
        { 
            bf_constant Val1 = BFExecuteToken(Tok->First);
            bf_constant Val2 = BFExecuteToken(Tok->Second);
            if (!BFTypeEquals(Tok->First->Var->Type, Val2.type))
            {
                WMPrintStr("bf: Assign type mismatch");
            }
            if (Val1.type.IsFloat) 
            {
                *(float*)Val1.assignDest = Val2.floatVal;
            }
            else if (Val1.type.Bits == BFBITS32)
            {
                *(int*)Val1.assignDest = Val2.intVal;
            }
            else if (Val1.type.Bits == BFBITS16)
            {
                *(int16_t*)Val1.assignDest = Val2.shortVal;
            }
            else if (Val1.type.Bits == BFBITS8)
            {
                *(char*)Val1.assignDest = Val2.byteVal;
            }
        }

        return Tok->First->Var->Val;
    }
    if (Tok->Type == BFADD)
    {
        return BFAddVal(BFExecuteToken(Tok->First), BFExecuteToken(Tok->Second));
    }
    if (Tok->Type == BFSUB)
    {
        return BFSubVal(BFExecuteToken(Tok->First), BFExecuteToken(Tok->Second));
    }
    if (Tok->Type == BFMUL)
    {
        return BFMulVal(BFExecuteToken(Tok->First), BFExecuteToken(Tok->Second));
    }
    if (Tok->Type == BFDIV)
    {
        return BFDivVal(BFExecuteToken(Tok->First), BFExecuteToken(Tok->Second));
    }
    if (Tok->Type == BFMODULO)
    {
        return BFModVal(BFExecuteToken(Tok->First), BFExecuteToken(Tok->Second));
    }
    if (Tok->Type == BFSHR)
    {
        return BFShrVal(BFExecuteToken(Tok->First), BFExecuteToken(Tok->Second));
    }
    if (Tok->Type == BFSHL)
    {
        return BFShlVal(BFExecuteToken(Tok->First), BFExecuteToken(Tok->Second));
    }
    if (Tok->Type == BFLESSTHAN)
    {
        return BFLTVal(BFExecuteToken(Tok->First), BFExecuteToken(Tok->Second));
    }
    if (Tok->Type == BFMORETHAN)
    {
        return BFGTVal(BFExecuteToken(Tok->First), BFExecuteToken(Tok->Second));
    }
    if (Tok->Type == BFEQUALS)
    {
        return BFEQVal(BFExecuteToken(Tok->First), BFExecuteToken(Tok->Second));
    }
    if (Tok->Type == BFPRINT)
    {
        bf_constant toPrint = BFExecuteToken(Tok->First);

        if (toPrint.type.IsFloat || toPrint.type.IsStruct) WMPrintStr("bf: print requires integer as parameter");

        if (toPrint.type.Bits == BFBITS8) WMPrintChar(toPrint.byteVal);
        else if (toPrint.type.Bits == BFBITS16) WMPrintChar(toPrint.shortVal);
        else if (toPrint.type.Bits == BFBITS32) WMPrintChar(toPrint.intVal);

        return (bf_constant){ 0 };
    }
    if (Tok->Type == BFIF)
    {
        bf_constant Bool = BFExecuteToken(Tok->First);

        if (Bool.type.IsFloat || Bool.type.IsStruct) WMPrintStr("bf: if requires interger as conditional");

        bool Run = false;
        if (Bool.type.Bits == BFBITS8) if (Bool.byteVal) Run = true;
        if (Bool.type.Bits == BFBITS16) if (Bool.shortVal) Run = true;
        if (Bool.type.Bits == BFBITS32) if (Bool.intVal) Run = true;

        if (Run)
        {
            for (int I = 0;I < Tok->NextScope->Lines.size;I++)
            {
                bf_constant Val = BFExecuteToken(*(bf_token**)LinkedListGet(&Tok->NextScope->Lines, I));
                if (Val.isReturn)
                {
                    return Val;
                }
            }
        }

        return (bf_constant){ 0 };
    }
    if (Tok->Type == BFWHILE)
    {
        while (1)
        {
            bf_constant Bool = BFExecuteToken(Tok->First);

            if (Bool.type.IsFloat || Bool.type.IsStruct) WMPrintStr("bf: while requires interger as conditional");

            if (Bool.type.Bits == BFBITS8) if (!Bool.byteVal) break;
            if (Bool.type.Bits == BFBITS16) if (!Bool.shortVal) break;
            if (Bool.type.Bits == BFBITS32) if (!Bool.intVal) break;

            for (int I = 0;I < Tok->NextScope->Lines.size;I++)
            {
                bf_constant Val = BFExecuteToken(*(bf_token**)LinkedListGet(&Tok->NextScope->Lines, I));
                if (Val.isReturn)
                {
                    return Val;
                }
            }
        }

        return (bf_constant){ 0 };
    }
    if (Tok->Type == BFFOR)
    {
        BFExecuteToken(Tok->First);
        while (1)
        {
            bf_constant Bool = BFExecuteToken(Tok->Second);
            if (Bool.type.IsFloat || Bool.type.IsStruct) WMPrintStr("bf: for requires interger as conditional");
            if (Bool.type.Bits == BFBITS8) if (!Bool.byteVal) break;
            if (Bool.type.Bits == BFBITS16) if (!Bool.shortVal) break;
            if (Bool.type.Bits == BFBITS32) if (!Bool.intVal) break;

            
            for (int I = 0;I < Tok->NextScope->Lines.size;I++)
            {
                bf_constant Val = BFExecuteToken(*(bf_token**)LinkedListGet(&Tok->NextScope->Lines, I));
                if (Val.isReturn)
                {
                    return Val;
                }
            }


            BFExecuteToken(Tok->Third);
        }

        return (bf_constant){ 0 };
    }
    if (Tok->Type == BFFUNCCALL)
    {
        for (int I = 0; I < Tok->NumParams; I++)
        {
            Tok->CallFunction->RootScope.Vars[I]->Val = BFExecuteToken(Tok->Params[I]);
        }
        bf_constant result = BFExecuteFunc(Tok->CallFunction);
        return result;
    }
    bf_constant Failed;
    Failed.isReturn = false;
    Failed.type.IsFloat = false;
    Failed.type.IsStruct = false;
    Failed.type.Bits = 8;
    Failed.intVal = 0;
    return Failed;
}

bf_constant BFExecuteFunc(bf_function* Func)
{
    for (int I = 0;; I++)
    {
        bf_constant R = BFExecuteToken(*(bf_token**)LinkedListGet(&Func->RootScope.Lines, I));

        if (I == Func->RootScope.Lines.size - 1 || R.isReturn)
        {
            R.isReturn = false;
            return R;
        }
    }
}

bf_constant BFRun(bf_function** Funcs)
{
    bf_name MainName;
    MainName.Len = 4;
    MainName.Name = (uint8_t*)malloc(MainName.Len);
    const char* SMainName = "Main";
    memcpy(MainName.Name, SMainName, 4);
    while (*Funcs)
    {
        if (BFCmpName((*Funcs)->Name, MainName))
        {
            WMPrintStr("Executing main");
            return BFExecuteFunc(*Funcs);
        }
        Funcs++;
    }
    return (bf_constant){ 0 };
}

bf_constant BFRunSource(String Code)
{
    char* Sanitized = (char*)malloc(Code.size + 1);
    memset(Sanitized, 0, Code.size);
    size_t SanitizedSize = 0;
    for (int I = 0;I < Code.size;I++)
    {
        if (Code.data[I] != ' ' && Code.data[I] != '\t' && Code.data[I] != '\n' && Code.data[I])
        {
            Sanitized[SanitizedSize++] = Code.data[I];
        }
    }
    Sanitized[SanitizedSize] = 0;

    //WMPrintStr(Sanitized);

    bf_function** Funcs = BFTokenize((uint8_t*)Sanitized, SanitizedSize);
    WMPrintStr("Executing");
    return BFRun(Funcs);
}