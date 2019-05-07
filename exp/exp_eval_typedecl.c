#include "exp_eval_a.h"
#include "exp_eval_typedecl.h"






typedef struct EXP_EvalCompileTypeDeclLevelFun
{
    u32 numIns;
    u32 numOuts;
} EXP_EvalCompileTypeDeclLevelFun;


typedef struct EXP_EvalCompileTypeDeclVar
{
    EXP_Node name;
    u32 type;
} EXP_EvalCompileTypeDeclVar;

typedef vec_t(EXP_EvalCompileTypeDeclVar) EXP_EvalCompileTypeDeclVarSpace;



typedef struct EXP_EvalCompileTypeDeclLevel
{
    EXP_Node src;
    bool hasRet;
    vec_u32 elms;
    union
    {
        EXP_EvalCompileTypeDeclLevelFun fun;
    };
    EXP_EvalCompileTypeDeclVarSpace varSpace;
} EXP_EvalCompileTypeDeclLevel;

static void EXP_evalCompileTypeDeclLevelFree(EXP_EvalCompileTypeDeclLevel* l)
{
    vec_free(&l->varSpace);
    vec_free(&l->elms);
}




EXP_evalCompileTypeDeclStackPop(EXP_EvalCompileTypeDeclStack* stack)
{
    EXP_EvalCompileTypeDeclLevel* l = &vec_last(stack);
    EXP_evalCompileTypeDeclLevelFree(l);
    vec_pop(stack);
}

void EXP_evalCompileTypeDeclStackResize(EXP_EvalCompileTypeDeclStack* stack, u32 n)
{
    assert(n < stack->length);
    for (u32 i = n; i < stack->length; ++i)
    {
        EXP_EvalCompileTypeDeclLevel* l = stack->data + i;
        EXP_evalCompileTypeDeclLevelFree(l);
    }
    vec_resize(stack, n);
}

void EXP_evalCompileTypeDeclStackFree(EXP_EvalCompileTypeDeclStack* stack)
{
    for (u32 i = 0; i < stack->length; ++i)
    {
        EXP_EvalCompileTypeDeclLevel* l = stack->data + i;
        EXP_evalCompileTypeDeclLevelFree(l);
    }
    vec_free(stack);
}
















static u32 EXP_evalCompileTypeDeclLoop
(
    EXP_Space* space,
    EXP_EvalAtypeInfoVec* atypeTable,
    EXP_EvalTypeContext* typeContext,
    EXP_EvalCompileTypeDeclStack* typeDeclStack,
    const EXP_SpaceSrcInfo* srcInfo,
    EXP_EvalError* outError
)
{
    EXP_EvalError err = { EXP_EvalErrCode_NONE };
    u32 r = -1;
next:
    if (!typeDeclStack->length)
    {
        assert(r != -1);
        return r;
    }
    EXP_EvalCompileTypeDeclLevel* top = &vec_last(typeDeclStack);
    EXP_Node node = top->src;

    if (EXP_isTok(space, node))
    {
        for (u32 i = 0; i + 1 < typeDeclStack->length; ++i)
        {
            EXP_EvalCompileTypeDeclLevel* fun = typeDeclStack->data + typeDeclStack->length - 2 - i;
            if (EXP_isSeqRound(space, fun->src))
            {
                for (u32 i = 0; i < fun->varSpace.length; ++i)
                {
                    EXP_EvalCompileTypeDeclVar* var = fun->varSpace.data + i;
                    EXP_Node varName = var->name;
                    if (EXP_nodeDataEq(space, node, varName))
                    {
                        r = var->type;
                        EXP_evalCompileTypeDeclStackPop(typeDeclStack);
                        goto next;
                    }
                }
            }
        }
        const char* cstr = EXP_tokCstr(space, node);
        for (u32 i = 0; i < atypeTable->length; ++i)
        {
            if (0 == strcmp(cstr, atypeTable->data[i].name))
            {
                r = EXP_evalTypeAtom(typeContext, i);
                EXP_evalCompileTypeDeclStackPop(typeDeclStack);
                goto next;
            }
        }
        EXP_evalErrorFound(outError, srcInfo, EXP_EvalErrCode_EvalSyntax, node);
        return -1;
    }
    else if (EXP_isSeqRound(space, node) || EXP_isSeqNaked(space, node))
    {
        const EXP_Node* elms = EXP_seqElm(space, node);
        u32 len = EXP_seqLen(space, node);
        if (0 == len)
        {
            EXP_evalErrorFound(outError, srcInfo, EXP_EvalErrCode_EvalSyntax, node);
            return -1;
        }
        u32 elmsOffset = EXP_isSeqCurly(space, elms[0]) ? 1 : 0;

        if (top->hasRet)
        {
            vec_push(&top->elms, r);
        }
        else
        {
            top->hasRet = true;

            if (EXP_isSeqCurly(space, elms[0]))
            {
                const EXP_Node* names = EXP_seqElm(space, elms[0]);
                u32 len = EXP_seqLen(space, elms[0]);
                for (u32 i = 0; i < len; ++i)
                {
                    EXP_Node name = names[i];
                    if (!EXP_isTok(space, name))
                    {
                        EXP_evalErrorFound(outError, srcInfo, EXP_EvalErrCode_EvalSyntax, node);
                        return -1;
                    }
                    for (u32 i = 0; i < top->varSpace.length; ++i)
                    {
                        EXP_Node name0 = top->varSpace.data[i].name;
                        if (EXP_nodeDataEq(space, name0, name))
                        {
                            EXP_evalErrorFound(outError, srcInfo, EXP_EvalErrCode_EvalSyntax, node);
                            return -1;
                        }
                    }
                    u32 t = EXP_evalTypeVar(typeContext, i);
                    EXP_EvalCompileTypeDeclVar v = { name, t };
                    vec_push(&top->varSpace, v);
                }
            }

            u32 arrowPos = -1;
            for (u32 i = elmsOffset; i < len; ++i)
            {
                if (EXP_isTok(space, elms[i]))
                {
                    const char* cstr = EXP_tokCstr(space, elms[i]);
                    if (0 == strcmp("->", cstr))
                    {
                        if (arrowPos != -1)
                        {
                            EXP_evalErrorFound(outError, srcInfo, EXP_EvalErrCode_EvalSyntax, node);
                            return -1;
                        }
                        arrowPos = i;
                        continue;
                    }
                }
                if (-1 == arrowPos)
                {
                    ++top->fun.numIns;
                }
                else
                {
                    ++top->fun.numOuts;
                }
            }
            if (-1 == arrowPos)
            {
                EXP_evalErrorFound(outError, srcInfo, EXP_EvalErrCode_EvalSyntax, node);
                return -1;
            }
            assert(elmsOffset + top->fun.numIns == arrowPos);
        }

        u32 p = elmsOffset + top->elms.length;
        u32 numIns = top->fun.numIns;
        u32 numOuts = top->fun.numOuts;
        u32 numAll = numIns + numOuts;
        bool inInput = true;
        if (p >= elmsOffset + numIns)
        {
            p += 1;
            inInput = false;
        }
        if (p < elmsOffset + 1 + numAll)
        {
            EXP_EvalCompileTypeDeclLevel l = { elms[p], inInput };
            vec_push(typeDeclStack, l);
        }
        else
        {
            r = EXP_evalTypeFun(typeContext, numIns, top->elms.data, numOuts, top->elms.data + numIns);
            EXP_evalCompileTypeDeclStackPop(typeDeclStack);
        }
        goto next;
    }
    else
    {
        EXP_evalErrorFound(outError, srcInfo, EXP_EvalErrCode_EvalSyntax, node);
        return -1;
    }
}








u32 EXP_evalCompileTypeDecl
(
    EXP_Space* space,
    EXP_EvalAtypeInfoVec* atypeTable,
    EXP_EvalTypeContext* typeContext,
    EXP_EvalCompileTypeDeclStack* typeDeclStack,
    EXP_Node node,
    const EXP_SpaceSrcInfo* srcInfo,
    EXP_EvalError* outError
)
{
    if (!EXP_isSeqRound(space, node) && !EXP_isSeqNaked(space, node))
    {
        EXP_evalErrorFound(outError, srcInfo, EXP_EvalErrCode_EvalSyntax, node);
        return -1;
    }
    const EXP_Node* elms = EXP_seqElm(space, node);
    u32 len = EXP_seqLen(space, node);

    assert(0 == typeDeclStack->length);
    EXP_EvalCompileTypeDeclLevel l = { 0 };
    if (1 == len)
    {
        l.src = elms[0];
    }
    else
    {
        l.src = node;
    }
    vec_push(typeDeclStack, l);
    u32 t = EXP_evalCompileTypeDeclLoop(space, atypeTable, typeContext, typeDeclStack, srcInfo, outError);
    return t;
}


















































































