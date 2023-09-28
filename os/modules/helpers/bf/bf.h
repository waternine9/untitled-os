#ifndef TOS_BFLANG_H
#define TOS_BFLANG_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "string.h"
#include "linked_list.h"

typedef enum
{
    BFASSIGN,
    BFADD,
    BFSUB,
    BFMUL,
    BFDIV,
    BFSHR,
    BFSHL,
    BFMODULO,
    BFLESSTHAN,
    BFMORETHAN,
    BFEQUALS,
    BFFUNCCALL,
    BFRETURN,
    BFIF,
    BFWHILE,
    BFFOR,
    BFPRINT,
    BFMEMBERACCESS,
    BFCAST,
    BFUNKNOWN
} bf_token_type;

typedef struct {
    uint8_t* Name;
    size_t Len;
} bf_name;

typedef enum {
    BFBITS8 = 8,
    BFBITS16 = 16,
    BFBITS32 = 32
} bf_bits;

typedef struct _bf_type {
    bool IsStruct;
    bool IsFloat;
    LinkedList StructMembers;
    bf_bits Bits;
    String Name;
} bf_type;

typedef struct _bf_member {
    bf_type Type;
    String Name;
} bf_member;

typedef struct _bf_type_name {
    String Name;
    bf_type Type;
} bf_type_name;

typedef struct {
    bf_type type;
    float floatVal;
    char byteVal;
    int16_t shortVal;
    int32_t intVal;
} bf_constant;

typedef struct {
    bf_constant Val;
    bf_name Name;
    bf_type Type;
    bool Ref;
} bf_variable;


struct _bf_token;

typedef struct _bf_scope {
    LinkedList Lines;
    bf_variable** Vars; // First vars are the arguments...
    size_t ArgsCount;
    size_t VarsCount;
    struct _bf_scope* Parent;
} bf_scope;

typedef struct {
    bf_scope RootScope;
    bf_name Name;
    bf_type Type;
    LinkedList Params;
} bf_function;

typedef struct _bf_token {
    bf_token_type Type;
    bf_variable* Var;
    bool IsFinalMember;
    bf_member* Member;
    bf_constant Const;
    struct _bf_token** Params;
    size_t NumParams;
    struct _bf_token* First;
    struct _bf_token* Second;
    struct _bf_token* Third;
    bf_type CastTo;
    bf_scope* NextScope;
    bf_function* CallFunction;
    bool IsConstant;
    bool IsVar;
    int Val;
} bf_token;

typedef struct
{
    uint8_t* Code;
    size_t Size;
    size_t At;
    bf_function** Functions;
    size_t BFNumFunctions;
    LinkedList Types;
} bf_tokenizer;

typedef struct
{
    bf_type Type;
} bf_compiled_token;

bf_constant BFRunSource(String Code);

#endif