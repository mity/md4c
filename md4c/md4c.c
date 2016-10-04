/*
 * MD4C: Markdown parser for C
 * (http://github.com/mity/md4c)
 *
 * Copyright (c) 2016 Martin Mitas
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "md4c.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/*****************************
 ***  Miscellaneous Stuff  ***
 *****************************/

#ifdef _MSC_VER
    /* MSVC does not understand "inline" when building as pure C (not C++).
     * However it understands "__inline" */
    #ifndef __cplusplus
        #define inline __inline
    #endif
#endif

/* Magic to support UTF16-LE (i.e. what is called Unicode among Windows
 * developers) input/output on Windows. */
#ifdef _T
    #undef _T
#endif
#if defined _WIN32  &&  defined MD_WIN_UNICODE
    #define _T(x)           L##x
#else
    #define _T(x)           x
#endif

/* Misc. macros. */
#define SIZEOF_ARRAY(a)     (sizeof(a) / sizeof(a[0]))


/************************
 ***  Internal Types  ***
 ************************/

/* These are omnipresent so lets save some typing. */
typedef MD_CHAR CHAR;
typedef MD_SIZE SZ;
typedef MD_OFFSET OFF;

/* Context propagated through all the parsing. */
typedef struct MD_CTX_tag MD_CTX;
struct MD_CTX_tag {
    /* Immutables (parameters of md_parse()). */
    const CHAR* text;
    SZ size;
    MD_RENDERER r;
    void* userdata;

    /* For MD_BLOCK_QUOTE */
    unsigned quote_level;   /* Nesting level. */

    /* Minimal indentation to call the block "indented code". */
    unsigned code_indent_offset;

    /* For MD_BLOCK_HEADER. */
    unsigned header_level;

    /* For MD_BLOCK_CODE (fenced). */
    CHAR code_fence_char;   /* '~' or '`' */
    SZ code_fence_length;
    OFF code_fence_indent;
    OFF code_fence_info_beg;
    OFF code_fence_info_end;

    /* For MD_BLOCK_HTML. */
    unsigned html_block_type;
};

typedef enum MD_LINETYPE_tag MD_LINETYPE;
enum MD_LINETYPE_tag {
    MD_LINE_BLANK,
    MD_LINE_HR,
    MD_LINE_ATXHEADER,
    MD_LINE_SETEXTHEADER,
    MD_LINE_SETEXTUNDERLINE,
    MD_LINE_INDENTEDCODE,
    MD_LINE_CODEFENCE,
    MD_LINE_FENCEDCODE,
    MD_LINE_HTML,
    MD_LINE_TEXT
};

typedef struct MD_LINE_tag MD_LINE;
struct MD_LINE_tag {
    MD_LINETYPE type;
    OFF beg;
    OFF end;
    unsigned quote_level;   /* Level of nesting in <blockquote>. */
    unsigned indent;        /* Indentation level. */
};


/*******************
 ***  Debugging  ***
 *******************/

static void
md_log(MD_CTX* ctx, const char* fmt, ...)
{
    char buffer[256];
    va_list args;

    if(ctx->r.debug_log == NULL)
        return;

    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    buffer[sizeof(buffer) - 1] = '\0';
    ctx->r.debug_log(buffer, ctx->userdata);
}

#ifdef DEBUG
    #define MD_ASSERT(cond)                                             \
            do {                                                        \
                if(!(cond)) {                                           \
                    md_log(ctx, "%s:%d: Assertion '" #cond "' failed.", \
                            __FILE__, (int)__LINE__);                   \
                    ret = -2;                                           \
                    goto abort;                                         \
                }                                                       \
            } while(0)
#else
    #ifdef __gnuc__
        #define MD_ASSERT(cond)     do { __builtin_expect((condition) != 0, !0); } while(0)
    #elif defined _MSC_VER  &&  _MSC_VER > 120
        #define MD_ASSERT(cond)     do { __assume(cond); } while(0)
    #else
        #define MD_ASSERT(cond)     do {} while(0)
    #endif
#endif

#define MD_UNREACHABLE()            MD_ASSERT(1 == 0)


/*****************
 ***  Helpers  ***
 *****************/

/* Character accessors. */
#define CH(off)                 (ctx->text[(off)])
#define STR(off)                (ctx->text + (off))

/* Character classification.
 * Note we assume ASCII compatibility of code points < 128 here. */
#define ISASCII_(ch)            ((ch) <= 127)
#define ISBLANK_(ch)            ((ch) == _T(' ') || (ch) == _T('\t'))
#define ISNEWLINE_(ch)          ((ch) == _T('\r') || (ch) == _T('\n'))
#define ISWHITESPACE_(ch)       (ISBLANK_(ch) || ch == _T('\v') || ch == _T('\f'))
#define ISCNTRL_(ch)            ((ch) <= 31 || (ch) == 127)
#define ISPUNCT_(ch)            ((33 <= (ch) && (ch) <= 47) || (58 <= (ch) && (ch) <= 64) || (91 <= (ch) && (ch) <= 96) || (123 <= (ch) && (ch) <= 126))
#define ISUPPER_(ch)            (_T('A') <= (ch) && (ch) <= _T('Z'))
#define ISLOWER_(ch)            (_T('a') <= (ch) && (ch) <= _T('z'))
#define ISALPHA_(ch)            (ISUPPER_(ch) || ISLOWER_(ch))
#define ISDIGIT_(ch)            (_T('0') <= (ch) && (ch) <= _T('9'))
#define ISXDIGIT_(ch)           (ISDIGIT_(ch) || (_T('a') < (ch) && (ch) <= _T('f') || (_T('A') < (ch) && (ch) <= _T('F'))
#define ISALNUM_(ch)            (ISALPHA_(ch) || ISDIGIT_(ch))
#define ISANYOF_(ch, palette)   (md_strchr((palette), (ch)) != NULL)

#define ISASCII(off)            ISASCII_(CH(off))
#define ISBLANK(off)            ISBLANK_(CH(off))
#define ISNEWLINE(off)          ISNEWLINE_(CH(off))
#define ISWHITESPACE(off)       ISWHITESPACE_(CH(off))
#define ISCNTRL(off)            ISCNTRL_(CH(off))
#define ISPUNCT(off)            ISPUNCT_(CH(off))
#define ISUPPER(off)            ISUPPER_(CH(off))
#define ISLOWER(off)            ISLOWER_(CH(off))
#define ISALPHA(off)            ISALPHA_(CH(off))
#define ISDIGIT(off)            ISDIGIT_(CH(off))
#define ISXDIGIT(off)           ISXDIGIT_(CH(off))
#define ISALNUM(off)            ISALNUM_(CH(off))
#define ISANYOF(off, palette)   ISANYOF_(CH(off), (palette))


static inline const CHAR*
md_strchr(const CHAR* str, CHAR ch)
{
    OFF i;
    for(i = 0; str[i] != _T('\0'); i++) {
        if(ch == str[i])
            return (str + i);
    }
    return NULL;
}

/* Case insensitive check of string equality. */
static inline int
md_str_case_eq(const CHAR* s1, const CHAR* s2, SZ n)
{
    OFF i;
    for(i = 0; i < n; i++) {
        CHAR ch1 = s1[i];
        CHAR ch2 = s2[i];

        if(ISLOWER_(ch1))
            ch1 += ('A'-'a');
        if(ISLOWER_(ch2))
            ch2 += ('A'-'a');
        if(ch1 != ch2)
            return -1;
    }
    return 0;
}


#define MD_ENTER_BLOCK(type, arg)                                       \
    do {                                                                \
        ret = ctx->r.enter_block((type), (arg), ctx->userdata);         \
        if(ret != 0) {                                                  \
            md_log(ctx, "Aborted from enter_block() callback.");        \
            goto abort;                                                 \
        }                                                               \
    } while(0)

#define MD_LEAVE_BLOCK(type, arg)                                       \
    do {                                                                \
        ret = ctx->r.leave_block((type), (arg), ctx->userdata);         \
        if(ret != 0) {                                                  \
            md_log(ctx, "Aborted from leave_block() callback.");        \
            goto abort;                                                 \
        }                                                               \
    } while(0)

#define MD_ENTER_SPAN(type, arg)                                        \
    do {                                                                \
        ret = ctx->r.enter_span((type), (arg), ctx->userdata);          \
        if(ret != 0) {                                                  \
            md_log(ctx, "Aborted from enter_span() callback.");         \
            goto abort;                                                 \
        }                                                               \
    } while(0)

#define MD_LEAVE_SPAN(type, arg)                                        \
    do {                                                                \
        ret = ctx->r.leave_span((type), (arg), ctx->userdata);          \
        if(ret != 0) {                                                  \
            md_log(ctx, "Aborted from leave_span() callback.");         \
            goto abort;                                                 \
        }                                                               \
    } while(0)

#define MD_TEXT(type, str, size)                                        \
    do {                                                                \
        if(size > 0) {                                                  \
            ret = ctx->r.text((type), (str), (size), ctx->userdata);    \
            if(ret != 0) {                                              \
                md_log(ctx, "Aborted from text() callback.");           \
                goto abort;                                             \
            }                                                           \
        }                                                               \
    } while(0)


/******************************************
 ***  Processing Single Block Contents  ***
 ******************************************/

static int
md_process_normal_block(MD_CTX* ctx, const MD_LINE* lines, int n_lines)
{
    int i;
    int ret = 0;

    for(i = 0; i < n_lines; i++) {
        MD_TEXT(MD_TEXT_NORMAL, STR(lines[i].beg), lines[i].end - lines[i].beg);

        /* Output soft or hard line break. */
        if(i + 1 < n_lines) {
            MD_TEXTTYPE break_type;

            if(CH(lines[i].end) == _T(' ')  &&  CH(lines[i].end+1) == _T(' '))
                break_type = MD_TEXT_BR;
            else
                break_type = MD_TEXT_SOFTBR;

            MD_TEXT(break_type, _T("\n"), 1);
        }
    }

abort:
    return ret;
}

static int
md_process_verbatim_block(MD_CTX* ctx, MD_TEXTTYPE text_type, const MD_LINE* lines, int n_lines)
{
    static const CHAR indent_str[16] = _T("                ");
    int i;
    int ret = 0;

    for(i = 0; i < n_lines; i++) {
        const MD_LINE* line = &lines[i];
        int indent = line->indent;

        /* Output code indentation. */
        while(indent > SIZEOF_ARRAY(indent_str)) {
            MD_TEXT(text_type, indent_str, SIZEOF_ARRAY(indent_str));
            indent -= SIZEOF_ARRAY(indent_str);
        }
        if(indent > 0)
            MD_TEXT(text_type, indent_str, indent);

        /* Output the code line itself. */
        MD_TEXT(text_type, STR(line->beg), line->end - line->beg);

        /* Enforce end-of-line. */
        MD_TEXT(text_type, _T("\n"), 1);
    }

abort:
    return ret;
}


/***************************************
 ***  Breaking Document into Blocks  ***
 ***************************************/

static int
md_is_hr_line(MD_CTX* ctx, OFF beg, OFF* p_end)
{
    OFF off = beg + 1;
    int n = 1;

    while(off < ctx->size  &&  (CH(off) == CH(beg) || CH(off) == _T(' '))) {
        if(CH(off) == CH(beg))
            n++;
        off++;
    }

    if(n < 3)
        return -1;

    *p_end = off;
    return 0;
}

static int
md_is_atxheader_line(MD_CTX* ctx, OFF beg, OFF* p_beg, OFF* p_end)
{
    int n;
    OFF off = beg + 1;

    while(off < ctx->size  &&  CH(off) == _T('#')  &&  off - beg < 7)
        off++;
    n = off - beg;

    if(n > 6)
        return -1;
    ctx->header_level = n;

    if(!(ctx->r.flags & MD_FLAG_PERMISSIVEATXHEADERS)  &&  off < ctx->size  &&  CH(off) != _T(' ')  &&  !ISNEWLINE(off))
        return -1;

    while(off < ctx->size  &&  CH(off) == _T(' '))
        off++;
    *p_beg = off;
    return 0;
}

static int
md_is_setext_underline(MD_CTX* ctx, OFF beg, OFF* p_end)
{
    OFF off = beg + 1;

    while(off < ctx->size  &&  CH(off) == CH(beg))
        off++;

    while(off < ctx->size  && CH(off) == _T(' '))
        off++;

    /* Optionally, space(s) can follow. */
    while(off < ctx->size  &&  CH(off) == _T(' '))
        off++;

    /* But nothing more is allowed on the line. */
    if(off < ctx->size  &&  !ISNEWLINE(off))
        return -1;

    ctx->header_level = (CH(beg) == _T('=') ? 1 : 2);
    return 0;
}

static int
md_is_opening_code_fence(MD_CTX* ctx, OFF beg, OFF* p_end)
{
    OFF off = beg;

    MD_ASSERT(CH(beg) == _T('`') || CH(beg) == _T('~'));

    while(off < ctx->size && CH(off) == CH(beg))
        off++;

    /* Fence must have at least three characters. */
    if(off - beg < 3)
        return -1;

    ctx->code_fence_length = off - beg;

    /* Optionally, space(s) can follow. */
    while(off < ctx->size  &&  CH(off) == _T(' '))
        off++;

    /* Optionally, language info can follow. It must not contain '`'. */
    ctx->code_fence_info_beg = off;
    while(off < ctx->size  &&  CH(off) != _T('`')  &&  !ISNEWLINE(off))
        off++;
    if(off < ctx->size  &&  !ISNEWLINE(off))
        return -1;

    *p_end = off;

    /* Right trim of language info. */
    while(off > ctx->code_fence_info_beg  &&  CH(off-1) == _T(' '))
        off--;
    ctx->code_fence_info_end = off;

    ctx->code_fence_char = CH(beg);
    return 0;
}

static int
md_is_closing_code_fence(MD_CTX* ctx, OFF beg, OFF* p_end)
{
    OFF off = beg;
    int ret = -1;

    /* Closing fence must have at least the same length and use same char as
     * opening one. */
    while(off < ctx->size  &&  CH(off) == ctx->code_fence_char)
        off++;
    if(off - beg < ctx->code_fence_length)
        goto out;

    /* Optionally, space(s) can follow */
    while(off < ctx->size  &&  CH(off) == _T(' '))
        off++;

    /* But nothing more is allowed on the line. */
    if(off < ctx->size  &&  !ISNEWLINE(off))
        goto out;

    ret = 0;

out:
    /* Note we set *p_end even on failure: If we are not closing fence, caller
     * would eat the line anyway without any parsing. */
    *p_end = off;
    return ret;
}

/* Returns type of the raw HTML block, or -1 if it is not HTML block.
 * (Refer to CommonMark specification for details about the types.)
 */
static int
md_is_html_block_start_condition(MD_CTX* ctx, OFF beg)
{
    typedef struct TAG_tag TAG;
    struct TAG_tag {
        const CHAR* name;
        unsigned len    : 8;
    };

    /* Type 6 is started by a long list of allowed tags. We use two-level
     * tree to speed-up the search. */
#ifdef X
    #undef X
#endif
#define X(name)     { _T(name), sizeof(name)-1 }
#define Xend        { NULL, 0 }
    static const TAG t1[] = { X("script"), X("pre"), X("style"), Xend };

    static const TAG a6[] = { X("address"), X("article"), X("aside"), Xend };
    static const TAG b6[] = { X("base"), X("basefont"), X("blockquote"), X("body"), Xend };
    static const TAG c6[] = { X("caption"), X("center"), X("col"), X("colgroup"), Xend };
    static const TAG d6[] = { X("dd"), X("details"), X("dialog"), X("dir"),
                             X("div"), X("dl"), X("dt"), Xend };
    static const TAG f6[] = { X("fieldset"), X("figcaption"), X("figure"), X("footer"),
                             X("form"), X("frame"), X("frameset"), Xend };
    static const TAG h6[] = { X("h1"), X("head"), X("header"), X("hr"), X("html"), Xend };
    static const TAG i6[] = { X("iframe"), Xend };
    static const TAG l6[] = { X("legend"), X("li"), X("link"), Xend };
    static const TAG m6[] = { X("main"), X("menu"), X("menuitem"), X("meta"), Xend };
    static const TAG n6[] = { X("nav"), X("noframes"), Xend };
    static const TAG o6[] = { X("ol"), X("optgroup"), X("option"), Xend };
    static const TAG p6[] = { X("p"), X("param"), Xend };
    static const TAG s6[] = { X("section"), X("source"), X("summary"), Xend };
    static const TAG t6[] = { X("table"), X("tbody"), X("td"), X("tfoot"), X("th"),
                             X("thead"), X("title"), X("tr"), X("track"), Xend };
    static const TAG u6[] = { X("ul"), Xend };
    static const TAG xx[] = { Xend };
#undef X

    static const TAG* map6[26] = {
        a6, b6, c6, d6, xx, f6, xx, h6, i6, xx, xx, l6, m6,
        n6, o6, p6, xx, xx, s6, t6, u6, xx, xx, xx, xx, xx
    };
    OFF off = beg + 1;
    int i;

    MD_ASSERT(CH(beg) == _T('<'));

    /* Check for type 1: <script, <pre, or <style */
    for(i = 0; t1[i].name != NULL; i++) {
        if(off + t1[i].len < ctx->size) {
            if(md_str_case_eq(STR(off), t1[i].name, t1[i].len) == 0)
                return 1;
        }
    }

    /* Check for type 2: <!-- */
    if(off + 3 < ctx->size  &&  CH(off) == _T('!')  &&  CH(off+1) == _T('-')  &&  CH(off+2) == _T('-'))
        return 2;

    /* Check for type 3: <? */
    if(off < ctx->size  &&  CH(off) == _T('?'))
        return 3;

    /* Check for type 4 or 5: <! */
    if(off < ctx->size  &&  CH(off) == _T('!')) {
        /* Check for type 4: <! followed by uppercase letter. */
        if(off + 1 < ctx->size  &&  ISUPPER(off+1))
            return 4;

        /* Check for type 5: <![CDATA[ */
        if(off + 8 < ctx->size) {
            if(memcmp(STR(off), _T("![CDATA["), 8 * sizeof(CHAR)) == 0)
                return 5;
        }
    }

    /* Check for type 6: Many possible starting tags listed above. */
    if(off + 1 < ctx->size  &&  (ISALPHA(off) || (CH(off) == _T('/') && ISALPHA(off+1)))) {
        int slot;
        const TAG* tags;

        if(CH(off) == _T('/'))
            off++;

        slot = (ISUPPER(off) ? CH(off) - 'A' : CH(off) - 'a');
        tags = map6[slot];

        for(i = 0; tags[i].name != NULL; i++) {
            if(off + tags[i].len <= ctx->size) {
                if(md_str_case_eq(STR(off), tags[i].name, tags[i].len) == 0) {
                    OFF tmp = off + tags[i].len;
                    if(tmp >= ctx->size)
                        return 6;
                    if(ISBLANK(tmp) || ISNEWLINE(tmp) || CH(tmp) == _T('>'))
                        return 6;
                    if(tmp+1 < ctx->size && CH(tmp) == _T('/') && CH(tmp+1) == _T('>'))
                        return 6;
                    break;
                }
            }
        }
    }

    /* Check for type 7: any COMPLETE other opening or closing tag. */
    // TODO: Rework this: This should be shared with some part of
    // inline raw html (spec section 6.8).
    if(off + 1 < ctx->size) {
        if(ISALPHA(off)  ||  (CH(off) == _T('/') && ISALPHA(off+1))) {
            OFF tmp = off + 1;

            /* Eat tag name. */
            while(tmp < ctx->size  &&  (ISALNUM(tmp) || CH(tmp) == _T('-')))
                tmp++;

            /* If opening tag, eat any attributes. */
            if(tmp < ctx->size  &&  CH(tmp) != _T('/')) {
                // TODO
            }

            /* Eat any whitespace */
            while(tmp < ctx->size  &&  ISWHITESPACE(tmp))
                tmp++;

            if(tmp < ctx->size  &&  CH(tmp) == _T('/'))
                tmp++;

            if(tmp < ctx->size  &&  CH(tmp) == _T('>'))
                return 7;
        }
    }

    return -1;
}

/* Case insensitive check whether line starting at the offset contains 'what'. */
static int
md_line_case_contains(MD_CTX* ctx, OFF beg, const CHAR* what, SZ what_len)
{
    OFF i;
    for(i = beg; i + what_len < ctx->size; i++) {
        if(ISNEWLINE(i))
            break;
        if(md_str_case_eq(STR(i), what, what_len) == 0)
            return 0;
    }
    return -1;
}

/* Case sensitive check whether line starting at the offset contains 'what'. */
static int
md_line_contains(MD_CTX* ctx, OFF beg, const CHAR* what, SZ what_len)
{
    OFF i;
    for(i = beg; i + what_len < ctx->size; i++) {
        if(ISNEWLINE(i))
            break;
        if(memcmp(STR(i), what, what_len * sizeof(CHAR)) == 0)
            return 0;
    }
    return -1;
}

/* Returns type of HTML block end condition or -1 if not an end condition. */
static int
md_is_html_block_end_condition(MD_CTX* ctx, OFF beg)
{
    switch(ctx->html_block_type) {
    case 1:     return (md_line_case_contains(ctx, beg, _T("</script>"), 9) == 0
                    ||  md_line_case_contains(ctx, beg, _T("</pre>"), 6) == 0
                    ||  md_line_case_contains(ctx, beg, _T("</style>"), 8) == 0 ? 1 : -1);
    case 2:     return (md_line_contains(ctx, beg, _T("-->"), 3) == 0 ? 2 : -1);
    case 3:     return (md_line_contains(ctx, beg, _T("?>"), 2) == 0 ? 3 : -1);
    case 4:     return (md_line_contains(ctx, beg, _T(">"), 1) == 0 ? 4 : -1);
    case 5:     return (md_line_contains(ctx, beg, _T("]]>"), 3) == 0 ? 5 : -1);
    case 6:     /* Pass through */
    case 7:     return (ISNEWLINE(beg) ? ctx->html_block_type : -1);
    default:    return -1;
    }
}

/* Analyze type of the line and find some its properties. This serves as a
 * main input for determining type and boundaries of a block. */
static void
md_analyze_line(MD_CTX* ctx, OFF beg, OFF* p_end, const MD_LINE* pivot_line, MD_LINE* line)
{
    OFF off = beg;

    line->type = MD_LINE_BLANK;
    line->quote_level = 0;
    line->indent = 0;

redo_indentation_after_blockquote_mark:
    /* Eat indentation. */
    while(off < ctx->size  &&  ISBLANK(off)) {
        if(CH(off) == _T('\t'))
            line->indent = (line->indent + 4) & ~3;
        else
            line->indent++;
        off++;
    }

    line->beg = off;

    /* Check whether we are fenced code continuation. */
    if(pivot_line->type == MD_LINE_FENCEDCODE || pivot_line->type == MD_LINE_CODEFENCE) {
        /* We are another MD_LINE_FENCEDCODE unless we are closing fence
         * which we transform into MD_LINE_BLANK. */
        if(line->indent < ctx->code_indent_offset) {
            if(md_is_closing_code_fence(ctx, off, &off) == 0) {
                line->type = MD_LINE_BLANK;
                goto done;
            }
        }

        line->type = MD_LINE_FENCEDCODE;
        goto done;
    }

    /* Check whether we are HTML block continuation. */
    if(pivot_line->type == MD_LINE_HTML  &&  ctx->html_block_type > 0) {
        if(md_is_html_block_end_condition(ctx, off) == ctx->html_block_type) {
            /* Make sure this is the last line of the block. */
            ctx->html_block_type = 0;
        }

        line->type = MD_LINE_HTML;
        goto done;
    }

    /* Check blockquote mark. */
    if(off < ctx->size  &&  CH(off) == _T('>')) {
        off++;
        if(off < ctx->size  &&  CH(off) == _T(' '))
            off++;
        line->quote_level++;
        line->indent = 0;
        goto redo_indentation_after_blockquote_mark;
    }

    /* Check whether we are blank line.
     * Note blank lines after indented code are treated as part of that block.
     * If they are at the end of the block, it is discarded by caller.
     */
    if(off >= ctx->size  ||  ISNEWLINE(off)) {
        line->indent = 0;
        if(pivot_line->type == MD_LINE_INDENTEDCODE)
            line->type = MD_LINE_INDENTEDCODE;
        else
            line->type = MD_LINE_BLANK;
        goto done;
    }

    /* Check whether we are indented code line.
     * Note indented code block cannot interrupt paragraph.
     * Keep this is as the first check after the blank line: The checks below
     * then do not need to verify that indentation < 4. */
    if((pivot_line->type == MD_LINE_BLANK || pivot_line->type == MD_LINE_INDENTEDCODE)
        && line->indent >= ctx->code_indent_offset) {
        line->type = MD_LINE_INDENTEDCODE;
        line->indent -= ctx->code_indent_offset;
        goto done;
    }

    /* Check whether we are indented code line.
     * Note indented code block cannot interrupt paragraph.
     * Keep this is as the first check after the blank line: The checks below
     * then do not need to verify that indentation < 4. */
    if((pivot_line->type == MD_LINE_BLANK || pivot_line->type == MD_LINE_INDENTEDCODE)
        && line->indent >= ctx->code_indent_offset) {
        line->type = MD_LINE_INDENTEDCODE;
        line->indent -= ctx->code_indent_offset;
        goto done;
    }

    /* Check whether we are ATX header. */
    if(CH(off) == _T('#')) {
        if(md_is_atxheader_line(ctx, off, &line->beg, &off) == 0) {
            line->type = MD_LINE_ATXHEADER;
            goto done;
        }
    }

    /* Check whether we are Setext underline. */
    if(pivot_line->type == MD_LINE_TEXT  &&  (CH(off) == _T('=') || CH(off) == _T('-'))) {
        if(md_is_setext_underline(ctx, off, &off) == 0) {
            line->type = MD_LINE_SETEXTUNDERLINE;
            goto done;
        }
    }

    /* Check whether we are thematic break line.
     * (Keep this after check for Setext underline as that one has higher priority). */
    if(ISANYOF(off, _T("-_*"))) {
        if(md_is_hr_line(ctx, off, &off) == 0) {
            line->type = MD_LINE_HR;
            goto done;
        }
    }

    /* Check whether we are starting code fence. */
    if(CH(off) == _T('`') || CH(off) == _T('~')) {
        if(md_is_opening_code_fence(ctx, off, &off) == 0) {
            ctx->code_fence_indent = line->indent;
            line->type = MD_LINE_CODEFENCE;
            goto done;
        }
    }

    /* Check whether we are start of raw HTML block. */
    if(CH(off) == _T('<')  &&  !(ctx->r.flags & MD_FLAG_NOHTMLBLOCKS)) {
        ctx->html_block_type = md_is_html_block_start_condition(ctx, off);
        if(ctx->html_block_type > 0) {
            line->type = MD_LINE_HTML;
            goto done;
        }
    }

    /* By default, we are normal text line. */
    line->type = MD_LINE_TEXT;

    /* Ordinary text line may need to upgrade block quote level because
     * of its lazy continuation. */
    if(pivot_line->type == MD_LINE_TEXT  &&  pivot_line->quote_level > line->quote_level)
        line->quote_level = pivot_line->quote_level;

done:
    /* Eat rest of the line contents */
    while(off < ctx->size  &&  !ISNEWLINE(off))
        off++;

    /* Set end of the line. */
    line->end = off;

    /* But for ATX header, we should not include the optional tailing mark. */
    if(line->type == MD_LINE_ATXHEADER) {
        OFF tmp = line->end;
        while(tmp > line->beg && CH(tmp-1) == _T(' '))
            tmp--;
        while(tmp > line->beg && CH(tmp-1) == _T('#'))
            tmp--;
        if(tmp == line->beg || CH(tmp-1) == _T(' ') || (ctx->r.flags & MD_FLAG_PERMISSIVEATXHEADERS))
            line->end = tmp;
    }

    /* Trim tailing spaces. */
    while(line->end > line->beg && CH(line->end-1) == _T(' '))
        line->end--;

    /* Eat also the new line. */
    if(off < ctx->size && CH(off) == _T('\r'))
        off++;
    if(off < ctx->size && CH(off) == _T('\n'))
        off++;

    *p_end = off;
}

static int
md_process_blockquote_nesting(MD_CTX* ctx, unsigned desired_level)
{
    int ret = 0;

    /* Bring blockquote nesting to expected level. */
    if(ctx->quote_level != desired_level) {
        while(ctx->quote_level < desired_level) {
            MD_ENTER_BLOCK(MD_BLOCK_QUOTE, NULL);
            ctx->quote_level++;
        }
        while(ctx->quote_level > desired_level) {
            MD_LEAVE_BLOCK(MD_BLOCK_QUOTE, NULL);
            ctx->quote_level--;
        }
    }

abort:
    return ret;
}

/* Determine type of the block (from type of its 1st line and some context),
 * call block_enter() callback, then appropriate function to parse contents
 * of the block, and finally block_leave() callback.
 */
static int
md_process_block(MD_CTX* ctx, const MD_LINE* lines, int n_lines)
{
    MD_BLOCKTYPE block_type;
    union {
        MD_BLOCK_H_DETAIL header;
        MD_BLOCK_CODE_DETAIL code;
    } det;
    int ret = 0;

    if(n_lines == 0)
        return 0;

    /* Make sure the processed leaf block lives in the proper block quote
     * nesting level. */
    ret = md_process_blockquote_nesting(ctx, lines[0].quote_level);
    if(ret != 0)
        goto abort;

    /* Derive block type from type of the first line. */
    switch(lines[0].type) {
        case MD_LINE_BLANK:
            return 0;

        case MD_LINE_HR:
            block_type = MD_BLOCK_HR;
            break;

        case MD_LINE_ATXHEADER:
        case MD_LINE_SETEXTHEADER:
            block_type = MD_BLOCK_H;
            det.header.level = ctx->header_level;
            break;

        case MD_LINE_INDENTEDCODE:
            det.code.lang = NULL;
            det.code.lang_size = 0;
            block_type = MD_BLOCK_CODE;
            break;

        case MD_LINE_FENCEDCODE:
            block_type = MD_BLOCK_CODE;
            if(ctx->code_fence_info_beg < ctx->code_fence_info_end)
                det.code.lang = STR(ctx->code_fence_info_beg);
            else
                det.code.lang = NULL;
            det.code.lang_size = ctx->code_fence_info_end - ctx->code_fence_info_beg;
            break;

        case MD_LINE_TEXT:
            block_type = MD_BLOCK_P;
            break;

        case MD_LINE_HTML:
            block_type = MD_BLOCK_HTML;
            break;

        case MD_LINE_SETEXTUNDERLINE:
        case MD_LINE_CODEFENCE:
            /* Noop. */
            return 0;

        default:
            MD_UNREACHABLE();
            break;
    }

    MD_ENTER_BLOCK(block_type, (void*) &det);

    /* Process the block contents accordingly to is type. */
    switch(block_type) {
        case MD_BLOCK_HR:
            /* Noop. */
            break;

        case MD_BLOCK_CODE:
            ret = md_process_verbatim_block(ctx, MD_TEXT_CODE, lines, n_lines);
            break;

        case MD_BLOCK_HTML:
            ret = md_process_verbatim_block(ctx, MD_TEXT_HTML, lines, n_lines);
            break;

        default:
            ret = md_process_normal_block(ctx, lines, n_lines);
            break;
    }
    if(ret != 0)
        goto abort;

    MD_LEAVE_BLOCK(block_type, (void*) &det);

abort:
    return ret;
}

/* Go through the document, analyze each line, on the fly identify block
 * boundaries and call md_process_block() for sequence of MD_LINE composing
 * the block.
 */
static int
md_process_doc(MD_CTX *ctx)
{
    static const MD_LINE dummy_line = { MD_LINE_BLANK, 0 };
    const MD_LINE* pivot_line = &dummy_line;
    MD_LINE* line;
    MD_LINE* lines = NULL;
    int alloc_lines = 0;
    int n_lines = 0;
    OFF off = 0;
    int ret = 0;

    MD_ENTER_BLOCK(MD_BLOCK_DOC, NULL);

    while(off < ctx->size) {
        if(n_lines >= alloc_lines) {
            MD_LINE* new_lines;

            alloc_lines = (alloc_lines == 0 ? 32 : alloc_lines * 2);
            new_lines = (MD_LINE*) realloc(lines, alloc_lines * sizeof(MD_LINE));
            if(new_lines == NULL) {
                md_log(ctx, "realloc() failed.");
                ret = -1;
                goto abort;
            }

            lines = new_lines;
        }

        md_analyze_line(ctx, off, &off, pivot_line, &lines[n_lines]);
        line = &lines[n_lines];

        /* Some line types form block on their own. */
        if(line->type == MD_LINE_HR || line->type == MD_LINE_ATXHEADER) {
            /* Flush accumulated lines. */
            ret = md_process_block(ctx, lines, n_lines);
            if(ret != 0)
                goto abort;

            /* Flush ourself. */
            ret = md_process_block(ctx, line, 1);
            if(ret != 0)
                goto abort;

            pivot_line = &dummy_line;
            n_lines = 0;
            continue;
        }

        /* MD_LINE_SETEXTUNDERLINE changes meaning of the previous block. */
        if(line->type == MD_LINE_SETEXTUNDERLINE) {
            MD_ASSERT(n_lines > 0);
            lines[0].type = MD_LINE_SETEXTHEADER;
            line->type = MD_LINE_BLANK;
        }

        /* New block also starts if line type changes or if block quote nesting
         * level changes. */
        if(line->type != pivot_line->type  ||  line->quote_level != pivot_line->quote_level) {
            ret = md_process_block(ctx, lines, n_lines);
            if(ret != 0)
                goto abort;

            /* Keep the current line as the new pivot. */
            if(line != &lines[0])
                memcpy(&lines[0], line, sizeof(MD_LINE));
            pivot_line = &lines[0];
            n_lines = 1;
            continue;
        }

        /* Not much to do with multiple blank lines. */
        if(line->type == MD_LINE_BLANK)
            continue;

        /* Otherwise we just accumulate the line into ongoing block. */
        n_lines++;
    }

    /* Process also the last block. */
    if(pivot_line->type != MD_LINE_BLANK) {
        ret = md_process_block(ctx, lines, n_lines);
        if(ret != 0)
            goto abort;
    }

    /* Close any dangling parent blocks. */
    ret = md_process_blockquote_nesting(ctx, 0);
    if(ret != 0)
        goto abort;

    MD_LEAVE_BLOCK(MD_BLOCK_DOC, NULL);

abort:
    free(lines);
    return ret;
}


/********************
 ***  Public API  ***
 ********************/

int
md_parse(const MD_CHAR* text, MD_SIZE size, const MD_RENDERER* renderer, void* userdata)
{
    MD_CTX ctx;

    /* Setup context structure. */
    memset(&ctx, 0, sizeof(MD_CTX));
    ctx.text = text;
    ctx.size = size;
    memcpy(&ctx.r, renderer, sizeof(MD_RENDERER));
    ctx.userdata = userdata;

    /* Offset for indented code block. */
    ctx.code_indent_offset = (ctx.r.flags & MD_FLAG_NOINDENTEDCODEBLOCKS) ? (OFF)(-1) : 4;

    /* Do all the hard work. */
    return md_process_doc(&ctx);
}
