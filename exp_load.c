#include "a.h"



typedef enum EXP_TokenType
{
    EXP_TokenType_Text,
    EXP_TokenType_String,

    EXP_TokenType_ExpBegin,
    EXP_TokenType_ExpEnd,

    EXP_NumTokenTypes
} EXP_TokenType;



typedef struct EXP_Token
{
    EXP_TokenType type;
    u32 begin;
    u32 len;
} EXP_Token;



typedef struct EXP_LoadContext
{
    EXP_Space* space;
    u32 srcLen;
    const char* src;
    u32 cur;
    u32 curLine;
    EXP_NodeSrcInfoTable* srcInfoTable;
    vec_char tmpStrBuf;
} EXP_LoadContext;



static EXP_LoadContext EXP_newLoadContext
(
    EXP_Space* space, u32 strSize, const char* str, EXP_NodeSrcInfoTable* srcInfoTable
)
{
    assert(strSize == strlen(str));
    EXP_LoadContext ctx = { space, strSize, str, 0, 1, srcInfoTable };
    return ctx;
}

static void EXP_loadContextFree(EXP_LoadContext* ctx)
{
    vec_free(&ctx->tmpStrBuf);
}





static bool EXP_skipSapce(EXP_LoadContext* ctx)
{
    const char* src = ctx->src;
    for (;;)
    {
        if (ctx->cur >= ctx->srcLen)
        {
            return false;
        }
        else if (' ' >= src[ctx->cur])
        {
            if ('\n' == src[ctx->cur])
            {
                ++ctx->curLine;
            }
            ++ctx->cur;
            continue;
        }
        else if (ctx->cur + 1 < ctx->srcLen)
        {
            if (0 == strncmp("//", src + ctx->cur, 2))
            {
                ctx->cur += 2;
                for (;;)
                {
                    if (ctx->cur >= ctx->srcLen)
                    {
                        return false;
                    }
                    else if ('\n' == src[ctx->cur])
                    {
                        ++ctx->cur;
                        ++ctx->curLine;
                        break;
                    }
                    ++ctx->cur;
                }
                continue;
            }
            else if (0 == strncmp("/*", src + ctx->cur, 2))
            {
                ctx->cur += 2;
                u32 n = 1;
                for (;;)
                {
                    if (ctx->cur >= ctx->srcLen)
                    {
                        return false;
                    }
                    else if (ctx->cur + 1 < ctx->srcLen)
                    {
                        if (0 == strncmp("/*", src + ctx->cur, 2))
                        {
                            ++n;
                            ctx->cur += 2;
                            continue;
                        }
                        else if (0 == strncmp("*/", src + ctx->cur, 2))
                        {
                            --n;
                            if (0 == n)
                            {
                                ctx->cur += 2;
                                break;
                            }
                            ctx->cur += 2;
                            continue;
                        }
                    }
                    else if ('\n' == src[ctx->cur])
                    {
                        ++ctx->curLine;
                    }
                    ++ctx->cur;
                }
                continue;
            }
        }
        break;
    }
    return true;
}












static bool EXP_readToken_String(EXP_LoadContext* ctx, EXP_Token* out)
{
    const char* src = ctx->src;
    char endCh = src[ctx->cur];
    ++ctx->cur;
    EXP_Token tok = { EXP_TokenType_String, ctx->cur, 0 };
    for (;;)
    {
        if (ctx->cur >= ctx->srcLen)
        {
            return false;
        }
        else if (endCh == src[ctx->cur])
        {
            break;
        }
        else if ('\\' == src[ctx->cur])
        {
            ctx->cur += 2;
            continue;
        }
        else
        {
            if ('\n' == src[ctx->cur])
            {
                ++ctx->curLine;
            }
            ++ctx->cur;
        }
    }
    tok.len = ctx->cur - tok.begin;
    assert(tok.len > 0);
    *out = tok;
    ++ctx->cur;
    return true;
}



static bool EXP_readToken_Text(EXP_LoadContext* ctx, EXP_Token* out)
{
    EXP_Token tok = { EXP_TokenType_Text, ctx->cur, 0 };
    const char* src = ctx->src;
    for (;;)
    {
        if (ctx->cur >= ctx->srcLen)
        {
            break;
        }
        else if (strchr("[]\"' \t\n\r\b\f", src[ctx->cur]))
        {
            break;
        }
        else
        {
            ++ctx->cur;
        }
    }
    tok.len = ctx->cur - tok.begin;
    assert(tok.len > 0);
    *out = tok;
    return true;
}




static bool EXP_readToken(EXP_LoadContext* ctx, EXP_Token* out)
{
    const char* src = ctx->src;
    if (ctx->cur >= ctx->srcLen)
    {
        return false;
    }
    if (!EXP_skipSapce(ctx))
    {
        return false;
    }
    bool ok = false;
    if ('[' == src[ctx->cur])
    {
        EXP_Token tok = { EXP_TokenType_ExpBegin, ctx->cur, 1 };
        *out = tok;
        ++ctx->cur;
        ok = true;
    }
    else if (']' == src[ctx->cur])
    {
        EXP_Token tok = { EXP_TokenType_ExpEnd, ctx->cur, 1 };
        *out = tok;
        ++ctx->cur;
        ok = true;
    }
    else if (('"' == src[ctx->cur]) || ('\'' == src[ctx->cur]))
    {
        ok = EXP_readToken_String(ctx, out);
    }
    else
    {
        ok = EXP_readToken_Text(ctx, out);
    }
    return ok;
}









static EXP_Node EXP_loadNode(EXP_LoadContext* ctx);

static bool EXP_loadEnd(EXP_LoadContext* ctx)
{
    assert(ctx->srcLen >= ctx->cur);
    if (ctx->srcLen == ctx->cur)
    {
        return true;
    }
    return false;
}

static bool EXP_loadExpEnd(EXP_LoadContext* ctx)
{
    if (EXP_loadEnd(ctx))
    {
        return true;
    }
    u32 cur0 = ctx->cur;
    u32 curLine0 = ctx->curLine;
    EXP_Token tok;
    if (!EXP_readToken(ctx, &tok))
    {
        return true;
    }
    switch (tok.type)
    {
    case EXP_TokenType_ExpEnd:
    {
        return true;
    }
    default:
        break;
    }
    ctx->cur = cur0;
    ctx->curLine = curLine0;
    return false;
}

static EXP_Node EXP_loadExp(EXP_LoadContext* ctx)
{
    EXP_Space* space = ctx->space;
    EXP_addExpEnter(space);
    while (!EXP_loadExpEnd(ctx))
    {
        EXP_Node e = EXP_loadNode(ctx);
        if (EXP_NodeInvalidId == e.id)
        {
            EXP_addExpCancel(space);
            EXP_Node node = { EXP_NodeInvalidId };
            return node;
        }
        EXP_addExpPush(ctx->space, e);
    }
    EXP_Node node = EXP_addExpDone(space);
    return node;
}



static void EXP_loadNodeSrcInfo(EXP_LoadContext* ctx, const EXP_Token* tok, EXP_NodeSrcInfo* info)
{
    info->offset = ctx->cur;
    info->line = ctx->curLine;
    info->column = 1;
    for (u32 i = 0; i < info->offset; ++i)
    {
        char c = ctx->src[info->offset - i];
        if (strchr("\n\r", c))
        {
            info->column = i;
            break;
        }
    }
    info->isStrTok = EXP_TokenType_String == tok->type;
}



static EXP_Node EXP_loadNode(EXP_LoadContext* ctx)
{
    EXP_Space* space = ctx->space;
    EXP_NodeSrcInfoTable* srcInfoTable = ctx->srcInfoTable;
    EXP_Node node = { EXP_NodeInvalidId };
    EXP_Token tok;
    if (!EXP_readToken(ctx, &tok))
    {
        return node;
    }
    EXP_NodeSrcInfo srcInfo = { true };
    EXP_loadNodeSrcInfo(ctx, &tok, &srcInfo);
    switch (tok.type)
    {
    case EXP_TokenType_Text:
    {
        const char* str = ctx->src + tok.begin;
        node = EXP_addLenStr(space, tok.len, str);
        break;
    }
    case EXP_TokenType_String:
    {
        char endCh = ctx->src[tok.begin - 1];
        const char* src = ctx->src + tok.begin;
        u32 n = 0;
        for (u32 i = 0; i < tok.len; ++i)
        {
            if ('\\' == src[i])
            {
                ++n;
                ++i;
            }
        }
        u32 len = tok.len - n;
        vec_resize(&ctx->tmpStrBuf, len + 1);
        u32 si = 0;
        for (u32 i = 0; i < tok.len; ++i)
        {
            if ('\\' == src[i])
            {
                ++i;
                ctx->tmpStrBuf.data[si++] = src[i];
                continue;
            }
            ctx->tmpStrBuf.data[si++] = src[i];
        }
        ctx->tmpStrBuf.data[len] = 0;
        assert(si == len);
        node = EXP_addLenStr(space, len, ctx->tmpStrBuf.data);
        break;
    }
    case EXP_TokenType_ExpBegin:
    {
        node = EXP_loadExp(ctx);
        if (EXP_NodeInvalidId == node.id)
        {
            return node;
        }
        break;
    }
    default:
        assert(false);
        return node;
    }
    if (srcInfoTable)
    {
        vec_push(srcInfoTable, srcInfo);
    }
    return node;
}













EXP_Node EXP_loadSrcAsCell(EXP_Space* space, const char* src, EXP_NodeSrcInfoTable* srcInfoTable)
{
    EXP_LoadContext ctx = EXP_newLoadContext(space, (u32)strlen(src), src, srcInfoTable);
    EXP_Node node = EXP_loadNode(&ctx);
    if (!EXP_loadEnd(&ctx))
    {
        EXP_loadContextFree(&ctx);
        EXP_Node node = { EXP_NodeInvalidId };
        return node;
    }
    EXP_loadContextFree(&ctx);
    return node;
}

EXP_Node EXP_loadSrcAsList(EXP_Space* space, const char* src, EXP_NodeSrcInfoTable* srcInfoTable)
{
    EXP_LoadContext ctx = EXP_newLoadContext(space, (u32)strlen(src), src, srcInfoTable);
    EXP_addExpEnter(space);
    for (;;)
    {
        EXP_Node e = EXP_loadNode(&ctx);
        if (EXP_NodeInvalidId == e.id) break;
        EXP_addExpPush(ctx.space, e);
    }
    if (!EXP_loadEnd(&ctx))
    {
        EXP_loadContextFree(&ctx);
        EXP_addExpCancel(space);
        EXP_Node node = { EXP_NodeInvalidId };
        return node;
    }
    EXP_loadContextFree(&ctx);
    EXP_Node node = EXP_addExpDone(space);
    return node;
}



























































































