#include "a.h"





PRIM_Space* PRIM_newSpace(void)
{
    PRIM_Space* space = zalloc(sizeof(*space));
    return space;
}

void PRIM_spaceFree(PRIM_Space* space)
{
    vec_free(&space->strPool);
    free(space);
}









void PRIM_nodeVecFree(PRIM_NodeBodyVec* vec)
{
    for (u32 i = 0; i < vec->length; ++i)
    {
        PRIM_nodeFree(vec->data[i]);
    }
    vec_free(vec);
}

PRIM_NodeBody* PRIM_nodeDup(const PRIM_NodeBody* node);

void PRIM_nodeVecDup(PRIM_NodeBodyVec* vec, const PRIM_NodeBodyVec* a)
{
    for (u32 i = 0; i < vec->length; ++i)
    {
        PRIM_nodeFree(vec->data[i]);
    }
    vec->length = 0;
    vec_reserve(vec, a->length);
    for (u32 i = 0; i < a->length; ++i)
    {
        PRIM_NodeBody* e = PRIM_nodeDup(a->data[i]);
        vec_push(vec, e);
    }
}

void PRIM_nodeVecConcat(PRIM_NodeBodyVec* vec, const PRIM_NodeBodyVec* a)
{
    vec_reserve(vec, vec->length + a->length);
    for (u32 i = 0; i < a->length; ++i)
    {
        PRIM_NodeBody* e = PRIM_nodeDup(a->data[i]);
        vec_push(vec, e);
    }
}









PRIM_NodeType PRIM_type(PRIM_Node node)
{
    return node->type;
}












void PRIM_nodeFree(PRIM_NodeBody* node)
{
    switch (node->type)
    {
    case PRIM_NodeType_Atom:
    {
        vec_free(&node->str);
        break;
    }
    case PRIM_NodeType_Expr:
    {
        PRIM_nodeVecFree(&node->vec);
        break;
    }
    default:
        assert(false);
        break;
    }
    free(node);
}

PRIM_NodeBody* PRIM_nodeDup(const PRIM_NodeBody* node)
{
    PRIM_NodeBody* node1 = zalloc(sizeof(*node1));
    node1->type = node->type;
    switch (node->type)
    {
    case PRIM_NodeType_Atom:
    {
        vec_dup(&node1->str, &node->str);
        break;
    }
    case PRIM_NodeType_Expr:
    {
        PRIM_nodeVecDup(&node1->vec, &node->vec);
        break;
    }
    default:
        assert(false);
        break;
    }
    node1->srcInfo = node->srcInfo;
    return node1;
}












PRIM_NodeBody* PRIM_atom(const char* str)
{
    PRIM_NodeBody* node = zalloc(sizeof(*node));
    node->type = PRIM_NodeType_Atom;
    vec_pusharr(&node->str, str, (u32)strlen(str) + 1);
    return node;
}

u32 PRIM_atomSize(PRIM_Node node)
{
    assert(PRIM_NodeType_Atom == node->type);
    return node->str.length > 0 ? node->str.length - 1 : 0;
}

const char* PRIM_atomCstr(PRIM_Node node)
{
    assert(PRIM_NodeType_Atom == node->type);
    return node->str.data;
}





PRIM_NodeBody* PRIM_expr(void)
{
    PRIM_NodeBody* node = zalloc(sizeof(*node));
    node->type = PRIM_NodeType_Expr;
    return node;
}

u32 PRIM_exprLen(PRIM_Node node)
{
    assert(PRIM_NodeType_Expr == node->type);
    return node->vec.length;
}

PRIM_NodeBody** PRIM_exprElm(PRIM_Node node)
{
    assert(PRIM_NodeType_Expr == node->type);
    return node->vec.data;
}






void PRIM_exprPush(PRIM_Node node, PRIM_Node c)
{
    assert(PRIM_NodeType_Expr == node->type);
    vec_push(&node->vec, c);
}

void PRIM_exprConcat(PRIM_Node node, PRIM_Node a)
{
    assert(PRIM_NodeType_Expr == node->type);
    assert(PRIM_NodeType_Expr == a->type);
    for (u32 i = 0; i < a->vec.length; ++i)
    {
        vec_push(&node->vec, PRIM_nodeDup(a->vec.data[i]));
    }
}











static u32 PRIM_saveVecSL(const PRIM_Space* space, const PRIM_NodeBodyVec* vec, char* buf, u32 bufSize, bool withSrcInfo)
{
    u32 n = 0;
    u32 bufRemain = bufSize;
    char* bufPtr = buf;

    for (u32 i = 0; i < vec->length; ++i)
    {
        u32 en = PRIM_saveSL(space, vec->data[i], bufPtr, bufRemain, withSrcInfo);
        if (en < bufRemain)
        {
            bufRemain -= en;
            bufPtr += en;
        }
        else
        {
            bufRemain = 0;
            bufPtr = NULL;
        }
        n += en;

        if (i < vec->length - 1)
        {
            if (1 < bufRemain)
            {
                *bufPtr = ' ';
                bufRemain -= 1;
                bufPtr += 1;
            }
            else
            {
                bufRemain = 0;
                bufPtr = NULL;
            }
            n += 1;
        }
    }
    return n;
}


u32 PRIM_saveSL(const PRIM_Space* space, PRIM_Node node, char* buf, u32 bufSize, bool withSrcInfo)
{
    switch (node->type)
    {
    case PRIM_NodeType_Atom:
    {
        const char* str = node->str.data;
        u32 sreLen = (node->str.length > 0) ? node->str.length - 1 : 0;
        u32 n;
        bool isStrTok = false;
        if (withSrcInfo && node->srcInfo.hasSrcInfo)
        {
            isStrTok = node->srcInfo.isStrTok;
        }
        else
        {
            for (u32 i = 0; i < sreLen; ++i)
            {
                if (strchr("[]\"' \t\n\r\b\f", str[i]))
                {
                    isStrTok = true;
                    break;
                }
            }
        }
        if (isStrTok)
        {
            u32 l = 2;
            for (u32 i = 0; i < sreLen; ++i)
            {
                if (' ' >= str[i])
                {
                    ++l;
                }
                else if (strchr("[]\"'", str[i]))
                {
                    ++l;
                }
                ++l;
            }
            if (buf && (l < bufSize))
            {
                n = 0;
                buf[n++] = '"';
                for (u32 i = 0; i < sreLen; ++i)
                {
                    if (' ' >= str[i])
                    {
                        buf[n++] = '\\';
                    }
                    else if (strchr("[]\"'", str[i]))
                    {
                        buf[n++] = '\\';
                    }
                    buf[n++] = str[i];
                }
                buf[n++] = '"';
                assert(n == l);
            }
            else
            {
                n = l;
            }
        }
        else
        {
            n = snprintf(buf, bufSize, "%s", node->str.data);
        }
        return n;
    }
    case PRIM_NodeType_Expr:
    {
        u32 n = 0;
        const PRIM_NodeBodyVec* vec = &node->vec;

        u32 bufRemain = bufSize;
        char* bufPtr = buf;

        if (1 < bufRemain)
        {
            *bufPtr = '[';
            bufRemain -= 1;
            bufPtr += 1;
        }
        else
        {
            bufRemain = 0;
            bufPtr = NULL;
        }
        n += 1;

        u32 n1 = PRIM_saveVecSL(space, vec, bufPtr, bufRemain, withSrcInfo);
        if (n1 < bufRemain)
        {
            bufRemain -= n1;
            bufPtr += n1;
        }
        else
        {
            bufRemain = 0;
            bufPtr = NULL;
        }
        n += n1;

        if (1 < bufRemain)
        {
            *bufPtr = ']';
            *(bufPtr + 1) = 0;
            bufRemain -= 1;
            bufPtr += 1;
        }
        else
        {
            bufRemain = 0;
            bufPtr = NULL;
        }
        n += 1;
        return n;
    }
    default:
        assert(false);
        return 0;
    }
}















typedef struct PRIM_SaveMLctx
{
    const PRIM_Space* space;
    const PRIM_SaveMLopt* opt;
    const u32 bufSize;
    char* const buf;

    u32 n;
    u32 column;
    u32 depth;
} PRIM_SaveMLctx;



static bool PRIM_saveMlForward(PRIM_SaveMLctx* ctx, u32 a)
{
    ctx->n += a;
    ctx->column += a;
    return ctx->column <= ctx->opt->width;
}

static void PRIM_saveMlBack(PRIM_SaveMLctx* ctx, u32 a)
{
    assert(ctx->n >= a);
    assert(ctx->column >= a);
    ctx->n -= a;
    ctx->column -= a;
}




static void PRIM_saveMlAdd(PRIM_SaveMLctx* ctx, const char* s)
{
    u32 a = (u32)strlen(s);
    u32 bufRemain = (ctx->bufSize > ctx->n) ? (ctx->bufSize - ctx->n) : 0;
    if (bufRemain > 0)
    {
        assert(ctx->buf);
        u32 wn = min(bufRemain - 1, a);
        if (wn > 0)
        {
            char* bufPtr = ctx->buf + ctx->n;
            stzncpy(bufPtr, s, wn + 1);
            bufPtr[wn] = 0;
        }
    }
    ctx->n += a;
    u32 ca = 0;
    for (u32 i = 0; i < a; ++i)
    {
        if ('\n' == s[a - 1 - i])
        {
            ctx->column = 0;
            break;
        }
        else
        {
            ++ca;
        }
    }
    ctx->column += ca;
}



static void PRIM_saveMlAddIdent(PRIM_SaveMLctx* ctx)
{
    u32 n = ctx->opt->indent * ctx->depth;
    for (u32 i = 0; i < n; ++i)
    {
        PRIM_saveMlAdd(ctx, " ");
    }
}







static void PRIM_saveMlAddNode(PRIM_SaveMLctx* ctx, PRIM_Node node, bool withSrcInfo);


static void PRIM_saveMlAddVec(PRIM_SaveMLctx* ctx, PRIM_NodeBodyVec* vec, bool withSrcInfo)
{
    for (u32 i = 0; i < vec->length; ++i)
    {
        PRIM_saveMlAddIdent(ctx);

        PRIM_saveMlAddNode(ctx, vec->data[i], withSrcInfo);

        PRIM_saveMlAdd(ctx, "\n");
    }
}




static void PRIM_saveMlAddNodeVec(PRIM_SaveMLctx* ctx, PRIM_Node node, bool withSrcInfo)
{
    u32 bufRemain = (ctx->bufSize > ctx->n) ? (ctx->bufSize - ctx->n) : 0;
    char* bufPtr = ctx->buf ? (ctx->buf + ctx->n) : NULL;
    u32 a = PRIM_saveSL(ctx->space, node, bufPtr, bufRemain, withSrcInfo);
    bool ok = PRIM_saveMlForward(ctx, a);

    if (!ok)
    {
        PRIM_saveMlBack(ctx, a);

        PRIM_saveMlAdd(ctx, "[\n");

        ++ctx->depth;
        PRIM_saveMlAddVec(ctx, &node->vec, withSrcInfo);
        --ctx->depth;

        PRIM_saveMlAddIdent(ctx);
        PRIM_saveMlAdd(ctx, "]");
    }
}



static void PRIM_saveMlAddNode(PRIM_SaveMLctx* ctx, PRIM_Node node, bool withSrcInfo)
{
    switch (node->type)
    {
    case PRIM_NodeType_Atom:
    {
        u32 bufRemain = (ctx->bufSize > ctx->n) ? (ctx->bufSize - ctx->n) : 0;
        char* bufPtr = ctx->buf ? (ctx->buf + ctx->n) : NULL;
        u32 a = PRIM_saveSL(ctx->space, node, bufPtr, bufRemain, withSrcInfo);
        PRIM_saveMlForward(ctx, a);
        return;
    }
    case PRIM_NodeType_Expr:
    {
        PRIM_saveMlAddNodeVec(ctx, node, withSrcInfo);
        return;
    }
    default:
        assert(false);
        return;
    }
}



















u32 PRIM_saveML(const PRIM_Space* space, PRIM_Node node, char* buf, u32 bufSize, const PRIM_SaveMLopt* opt)
{
    switch (node->type)
    {
    case PRIM_NodeType_Atom:
    {
        return PRIM_saveSL(space, node, buf, bufSize, opt->withSrcInfo);
    }
    case PRIM_NodeType_Expr:
    {
        PRIM_SaveMLctx ctx =
        {
            space, opt, bufSize, buf,
        };
        PRIM_saveMlAddNodeVec(&ctx, node, opt->withSrcInfo);
        return ctx.n;
    }
    default:
        assert(false);
        return 0;
    }
}




























































































































