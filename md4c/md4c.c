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

#define STRINGIZE_(x)       #x
#define STRINGIZE(x)        STRINGIZE_(x)


/************************
 ***  Internal Types  ***
 ************************/

/* These are omnipresent so lets save some typing. */
typedef MD_CHAR CHAR;
typedef MD_SIZE SZ;
typedef MD_OFFSET OFF;

typedef struct MD_MARK_tag MD_MARK;


/* During analyzes of inline marks, we need to manage some "mark chains",
 * of (yet unresolved) openers. This structure holds start/end of the chain.
 * The chain internals are then realized through MD_MARK::prev and ::next.
 */
typedef struct MD_MARKCHAIN_tag MD_MARKCHAIN;
struct MD_MARKCHAIN_tag {
    int head;   /* Index of first mark in the chain, or -1 if empty. */
    int tail;   /* Index of last mark in the chain, or -1 if empty. */
};

/* Context propagated through all the parsing. */
typedef struct MD_CTX_tag MD_CTX;
struct MD_CTX_tag {
    /* Immutable stuff (parameters of md_parse()). */
    const CHAR* text;
    SZ size;
    MD_RENDERER r;
    void* userdata;

    /* Stack of inline/span markers.
     * This is only used for parsing a single block contents but by storing it
     * here we may reuse the stack for subsequent blocks; i.e. we have fewer
     * (re)allocations. */
    MD_MARK* marks;
    unsigned n_marks;
    unsigned alloc_marks;

    /* For resolving of inline spans. */
    MD_MARKCHAIN mark_chains[4];
#define BACKTICK_OPENERS        ctx->mark_chains[0]
#define LOWERTHEN_OPENERS       ctx->mark_chains[1]
#define ASTERISK_OPENERS        ctx->mark_chains[2]
#define UNDERSCORE_OPENERS      ctx->mark_chains[3]

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
    int html_block_type;
};

typedef enum MD_LINETYPE_tag MD_LINETYPE;
enum MD_LINETYPE_tag {
    MD_LINE_BLANK,
    MD_LINE_HR,
    MD_LINE_ATXHEADER,
    MD_LINE_SETEXTHEADER,
    MD_LINE_SETEXTUNDERLINE,
    MD_LINE_INDENTEDCODE,
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

#define MD_LOG(msg)                                                     \
    do {                                                                \
        if(ctx->r.debug_log != NULL)                                    \
            ctx->r.debug_log((msg), ctx->userdata);                     \
    } while(0)

#ifdef DEBUG
    #define MD_ASSERT(cond)                                             \
            do {                                                        \
                if(!(cond)) {                                           \
                    MD_LOG(__FILE__ ":" STRINGIZE(__LINE__) ": "        \
                           "Assertion '" STRINGIZE(cond) "' failed.");  \
                    exit(1);                                            \
                }                                                       \
            } while(0)

    #define MD_UNREACHABLE()        MD_ASSERT(1 == 0)
#else
    #ifdef __GNUC__
        #define MD_ASSERT(cond)     do { if(!(cond)) __builtin_unreachable(); } while(0)
        #define MD_UNREACHABLE()    do { __builtin_unreachable(); } while(0)
    #elif defined _MSC_VER  &&  _MSC_VER > 120
        #define MD_ASSERT(cond)     do { __assume(cond); } while(0)
        #define MD_UNREACHABLE()    do { __assume(0); } while(0)
    #else
        #define MD_ASSERT(cond)     do {} while(0)
        #define MD_UNREACHABLE()    do {} while(0)
    #endif
#endif


/*****************
 ***  Helpers  ***
 *****************/

/* Character accessors. */
#define CH(off)                 (ctx->text[(off)])
#define STR(off)                (ctx->text + (off))

/* Character classification.
 * Note we assume ASCII compatibility of code points < 128 here. */
#define ISASCII_(ch)            ((unsigned)(ch) <= 127)
#define ISBLANK_(ch)            ((unsigned)(ch) == _T(' ') || (unsigned)(ch) == _T('\t'))
#define ISNEWLINE_(ch)          ((unsigned)(ch) == _T('\r') || (unsigned)(ch) == _T('\n'))
#define ISWHITESPACE_(ch)       (ISBLANK_(ch) || ch == _T('\v') || ch == _T('\f'))
#define ISCNTRL_(ch)            ((unsigned)(ch) <= 31 || (unsigned)(ch) == 127)
#define ISPUNCT_(ch)            ((33 <= (unsigned)(ch) && (unsigned)(ch) <= 47) || (58 <= (unsigned)(ch) && (unsigned)(ch) <= 64) || (91 <= (unsigned)(ch) && (unsigned)(ch) <= 96) || (123 <= (unsigned)(ch) && (unsigned)(ch) <= 126))
#define ISUPPER_(ch)            (_T('A') <= (unsigned)(ch) && (unsigned)(ch) <= _T('Z'))
#define ISLOWER_(ch)            (_T('a') <= (unsigned)(ch) && (unsigned)(ch) <= _T('z'))
#define ISALPHA_(ch)            (ISUPPER_(ch) || ISLOWER_(ch))
#define ISDIGIT_(ch)            (_T('0') <= (unsigned)(ch) && (unsigned)(ch) <= _T('9'))
#define ISXDIGIT_(ch)           (ISDIGIT_(ch) || (_T('a') <= (unsigned)(ch) && (unsigned)(ch) <= _T('f')) || (_T('A') <= (unsigned)(ch) && (unsigned)(ch) <= _T('F')))
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

static int
md_text_with_null_replacement(MD_CTX* ctx, MD_TEXTTYPE type, const CHAR* str, SZ size)
{
    OFF off = 0;
    int ret = 0;

    while(1) {
        while(off < size  &&  str[off] != _T('\0'))
            off++;

        if(off > 0) {
            ret = ctx->r.text(type, str, off, ctx->userdata);
            if(ret != 0)
                return ret;

            str += off;
            size -= off;
            off = 0;
        }

        if(off >= size)
            return 0;

        ret = ctx->r.text(MD_TEXT_NULLCHAR, _T(""), 1, ctx->userdata);
        if(ret != 0)
            return ret;
    }
}


#define MD_CHECK(func)                                                  \
    do {                                                                \
        ret = (func);                                                   \
        if(ret != 0)                                                    \
            goto abort;                                                 \
    } while(0)

#define MD_ENTER_BLOCK(type, arg)                                       \
    do {                                                                \
        ret = ctx->r.enter_block((type), (arg), ctx->userdata);         \
        if(ret != 0) {                                                  \
            MD_LOG("Aborted from enter_block() callback.");             \
            goto abort;                                                 \
        }                                                               \
    } while(0)

#define MD_LEAVE_BLOCK(type, arg)                                       \
    do {                                                                \
        ret = ctx->r.leave_block((type), (arg), ctx->userdata);         \
        if(ret != 0) {                                                  \
            MD_LOG("Aborted from leave_block() callback.");             \
            goto abort;                                                 \
        }                                                               \
    } while(0)

#define MD_ENTER_SPAN(type, arg)                                        \
    do {                                                                \
        ret = ctx->r.enter_span((type), (arg), ctx->userdata);          \
        if(ret != 0) {                                                  \
            MD_LOG("Aborted from enter_span() callback.");              \
            goto abort;                                                 \
        }                                                               \
    } while(0)

#define MD_LEAVE_SPAN(type, arg)                                        \
    do {                                                                \
        ret = ctx->r.leave_span((type), (arg), ctx->userdata);          \
        if(ret != 0) {                                                  \
            MD_LOG("Aborted from leave_span() callback.");              \
            goto abort;                                                 \
        }                                                               \
    } while(0)

#define MD_TEXT(type, str, size)                                        \
    do {                                                                \
        if(size > 0) {                                                  \
            ret = ctx->r.text((type), (str), (size), ctx->userdata);    \
            if(ret != 0) {                                              \
                MD_LOG("Aborted from text() callback.");                \
                goto abort;                                             \
            }                                                           \
        }                                                               \
    } while(0)

#define MD_TEXT_INSECURE(type, str, size)                               \
    do {                                                                \
        if(size > 0) {                                                  \
            ret = md_text_with_null_replacement(ctx, type, str, size);  \
            if(ret != 0) {                                              \
                MD_LOG("Aborted from text() callback.");                \
                goto abort;                                             \
            }                                                           \
        }                                                               \
    } while(0)


/******************************
 ***  Recognizing raw HTML  ***
 ******************************/

/* md_is_html_tag() may be called when processing inlines (inline raw HTML)
 * or when breaking document to blocks (checking for start of HTML block type 7).
 *
 * When breaking document to blocks, we do not yet know line boundaries, but
 * in that case th whole tag has to live on a single line. We distinguish this
 * by n_lines == 0.
 */
static int
md_is_html_tag(MD_CTX* ctx, const MD_LINE* lines, int n_lines, OFF beg, OFF max_end, OFF* p_end)
{
    int attr_state;
    OFF off = beg;
    OFF line_end = (n_lines > 0) ? lines[0].end : ctx->size;
    int i = 0;

    MD_ASSERT(CH(beg) == _T('<'));

    if(off + 1 >= line_end)
        return -1;
    off++;

    /* For parsing attributes, we need a little state automaton below.
     * State -1: no attributes are allowed.
     * State 0: attribute could follow after some whitespace.
     * State 1: after a whitespace (attribute name may follow).
     * State 2: after attribute name ('=' MAY follow).
     * State 3: after '=' (value specification MUST follow).
     * State 41: in middle of unquoted attribute value.
     * State 42: in middle of single-quoted attribute value.
     * State 43: in middle of double-quoted attribute value.
     */
    attr_state = 0;

    if(CH(off) == _T('/')) {
        /* Closer tag "</ ... >". No attributes may be present. */
        attr_state = -1;
        off++;
    }

    /* Tag name */
    if(off >= line_end  ||  !ISALPHA(off))
        return -1;
    off++;
    while(off < line_end  &&  (ISALNUM(off)  ||  CH(off) == _T('-')))
        off++;

    /* (Optional) attributes (if not closer), (optional) '/' (if not closer)
     * and final '>'. */
    while(1) {
        while(off < line_end  &&  !ISNEWLINE(off)) {
            if(attr_state > 40) {
                if(attr_state == 41 && ISANYOF(off, _T("\"'=<>`"))) {
                    attr_state = 0;
                    off--;  /* Put the char back for re-inspection in the new state. */
                } else if(attr_state == 42 && CH(off) == _T('\'')) {
                    attr_state = 0;
                } else if(attr_state == 43 && CH(off) == _T('"')) {
                    attr_state = 0;
                }
                off++;
            } else if(ISWHITESPACE(off)) {
                if(attr_state == 0)
                    attr_state = 1;
                off++;
            } else if(attr_state <= 2 && CH(off) == _T('>')) {
                /* End. */
                goto done;
            } else if(attr_state <= 2 && CH(off) == _T('/') && off+1 < line_end && CH(off+1) == _T('>')) {
                /* End with digraph '/>' */
                off++;
                goto done;
            } else if((attr_state == 1 || attr_state == 2) && (ISALPHA(off) || CH(off) == _T('_') || CH(off) == _T(':'))) {
                off++;
                /* Attribute name */
                while(off < line_end && (ISALNUM(off) || ISANYOF(off, _T("_.:-"))))
                    off++;
                attr_state = 2;
            } else if(attr_state == 2 && CH(off) == _T('=')) {
                /* Attribute assignment sign */
                off++;
                attr_state = 3;
            } else if(attr_state == 3) {
                /* Expecting start of attribute value. */
                if(CH(off) == _T('"'))
                    attr_state = 43;
                else if(CH(off) == _T('\''))
                    attr_state = 42;
                else if(!ISANYOF(off, _T("\"'=<>`"))  &&  !ISNEWLINE(off))
                    attr_state = 41;
                else
                    return -1;
                off++;
            } else {
                /* Anything unexpected. */
                return -1;
            }
        }

        /* We have to be on a single line. See definition of start condition
         * of HTML block, type 7. */
        if(n_lines == 0)
            return -1;

        i++;
        if(i >= n_lines)
            return -1;

        off = lines[i].beg;
        line_end = lines[i].end;

        if(attr_state == 0)
            attr_state = 1;

        if(off >= max_end)
            return -1;
    }

done:
    if(off >= max_end)
        return -1;

    *p_end = off+1;
    return 0;
}

static int
md_is_html_comment(MD_CTX* ctx, const MD_LINE* lines, int n_lines, OFF beg, OFF max_end, OFF* p_end)
{
    OFF off = beg;
    int i = 0;

    MD_ASSERT(CH(beg) == _T('<'));

    if(off + 4 >= lines[0].end)
        return -1;
    if(CH(off+1) != _T('!')  ||  CH(off+2) != _T('-')  ||  CH(off+3) != _T('-'))
        return -1;
    off += 4;

    /* ">" and "->" must follow the opening. */
    if(off < lines[0].end  &&  CH(off) == _T('>'))
        return -1;
    if(off+1 < lines[0].end  &&  CH(off) == _T('-')  &&  CH(off+1) == _T('>'))
        return -1;

    while(1) {
        while(off + 2 < lines[i].end) {
            if(CH(off) == _T('-')  &&  CH(off+1) == _T('-')) {
                if(CH(off+2) == _T('>')) {
                    /* Success. */
                    off += 2;
                    goto done;
                } else {
                    /* "--" is prohibited inside the comment. */
                    return -1;
                }
            }

            off++;
        }

        i++;
        if(i >= n_lines)
            return -1;

        off = lines[i].beg;

        if(off >= max_end)
            return -1;
    }

done:
    if(off >= max_end)
        return -1;

    *p_end = off+1;
    return 0;
}

static int
md_is_html_processing_instruction(MD_CTX* ctx, const MD_LINE* lines, int n_lines, OFF beg, OFF max_end, OFF* p_end)
{
    OFF off = beg;
    int i = 0;

    MD_ASSERT(CH(beg) == _T('<'));

    if(off + 2 >= lines[0].end)
        return -1;
    if(CH(off+1) != _T('?'))
        return -1;
    off += 2;

    while(1) {
        while(off + 1 < lines[i].end) {
            if(CH(off) == _T('?')  &&  CH(off+1) == _T('>')) {
                /* Success. */
                off++;
                goto done;
            }

            off++;
        }

        i++;
        if(i >= n_lines)
            return -1;

        off = lines[i].beg;
        if(off >= max_end)
            return -1;
    }

done:
    if(off >= max_end)
        return -1;

    *p_end = off+1;
    return 0;
}

static int
md_is_html_declaration(MD_CTX* ctx, const MD_LINE* lines, int n_lines, OFF beg, OFF max_end, OFF* p_end)
{
    OFF off = beg;
    int i = 0;

    MD_ASSERT(CH(beg) == _T('<'));

    if(off + 2 >= lines[0].end)
        return -1;
    if(CH(off+1) != _T('!'))
        return -1;
    off += 2;

    /* Declaration name. */
    if(off >= lines[0].end  ||  !ISALPHA(off))
        return -1;
    off++;
    while(off < lines[0].end  &&  ISALPHA(off))
        off++;
    if(off < lines[0].end  &&  !ISWHITESPACE(off))
        return -1;

    while(1) {
        while(off < lines[i].end) {
            if(CH(off+1) == _T('>')) {
                /* Success. */
                off++;
                goto done;
            }

            off++;
        }

        i++;
        if(i >= n_lines)
            return -1;

        off = lines[i].beg;
        if(off >= max_end)
            return -1;
    }

done:
    if(off >= max_end)
        return -1;

    *p_end = off+1;
    return 0;
}

static int
md_is_html_cdata(MD_CTX* ctx, const MD_LINE* lines, int n_lines, OFF beg, OFF max_end, OFF* p_end)
{
    static const CHAR open_str[9] = _T("<![CDATA[");

    OFF off = beg;
    int i = 0;

    if(off + SIZEOF_ARRAY(open_str) >= lines[0].end)
        return -1;
    if(memcmp(STR(off), open_str, sizeof(open_str)) != 0)
        return -1;
    off += SIZEOF_ARRAY(open_str);

    while(1) {
        while(off + 2 < lines[i].end) {
            if(CH(off) == _T(']')  &&  CH(off+1) == _T(']')  &&  CH(off+2) == _T('>')) {
                /* Success. */
                off += 2;
                goto done;
            }

            off++;
        }

        i++;
        if(i >= n_lines)
            return -1;

        off = lines[i].beg;
        if(off >= max_end)
            return -1;
    }

done:
    if(off >= max_end)
        return -1;

    *p_end = off+1;
    return 0;
}

static int
md_is_html_any(MD_CTX* ctx, const MD_LINE* lines, int n_lines, OFF beg, OFF max_end, OFF* p_end)
{
    if(md_is_html_tag(ctx, lines, n_lines, beg, max_end, p_end) == 0)
        return 0;
    if(md_is_html_comment(ctx, lines, n_lines, beg, max_end, p_end) == 0)
        return 0;
    if(md_is_html_processing_instruction(ctx, lines, n_lines, beg, max_end, p_end) == 0)
        return 0;
    if(md_is_html_declaration(ctx, lines, n_lines, beg, max_end, p_end) == 0)
        return 0;
    if(md_is_html_cdata(ctx, lines, n_lines, beg, max_end, p_end) == 0)
        return 0;

    return -1;
}


/******************************************
 ***  Recognizing Some Complex Inlines  ***
 ******************************************/

static int
md_is_autolink(MD_CTX* ctx, OFF beg, OFF end)
{
    OFF off;

    MD_ASSERT(CH(beg) == _T('<'));
    MD_ASSERT(CH(end-1) == _T('>'));

    beg++;
    end--;

    /* Check for scheme. */
    off = beg;
    if(off >= end  ||  !ISASCII(off))
        return -1;
    off++;
    while(1) {
        if(off >= end)
            return -1;
        if(off - beg > 32)
            return -1;
        if(CH(off) == _T(':')  &&  off - beg >= 2)
            break;
        if(!ISALNUM(off) && CH(off) != _T('+') && CH(off) != _T('-') && CH(off) != _T('.'))
            return -1;
        off++;
    }

    /* Check the path after the scheme. */
    while(off < end) {
        if(ISWHITESPACE(off) || ISCNTRL(off) || CH(off) == _T('<') || CH(off) == _T('>'))
            return -1;
        off++;
    }

    return 0;
}


/******************************************************
 ***  Processing Sequence of Inlines (a.k.a Spans)  ***
 ******************************************************/

/* We process inlines in few phases:
 *
 * (1) We go through the block text and collect all significant characters
 *     which may start/end a span or some other significant position into
 *     ctx->marks[]. Core of this is what md_collect_marks() does.
 *
 *     We also do some very brief preliminary context-less analysis, whether
 *     it might be opener or closer (e.g. of an emphasis span).
 *
 *     This speeds the other steps as we do not need to re-iterate over all
 *     characters anymore.
 *
 * (2) We analyze each potential mark types, in order by their precedence.
 *
 *     In each md_analyze_XXX() function, we re-iterate list of the marks,
 *     skipping already resolved regions (in preceding precedences) and try to
 *     resolve them.
 *
 * (2.1) For trivial marks, which are single (e.g. HTML entity), we just mark
 *       them as resolved.
 *
 * (2.2) For range-type marks, we analyze whether the mark could be closer
 *       and, if yes, whether there is some preceding opener it could satisfy.
 *
 *       If not we check whether it could be really an opener and if yes, we
 *       remember it so subsequent closers may resolve it.
 *
 * (3) Finally, when all marks were analyzed, we render the block contents
 *     by calling MD_RENDERER::text() callback, interrupting by ::enter_span()
 *     or ::close_span() whenever we reach a resolved mark.
 */


/* The mark structure.
 *
 * '\\': Maybe escape sequence.
 *  '*': Maybe (strong) emphasis start/end.
 *  '`': Maybe code span start/end.
 *  '&': Maybe start of entity.
 *  ';': Maybe end of entity.
 *  '<': Maybe start of raw HTML or autolink.
 *  '>': Maybe end of raw HTML or autolink.
 *  '0': NULL char.
 *
 * Note that not all instances of these chars in the text imply creation of the
 * structure. Only those which have (or may have, after we see more context)
 * the special meaning.
 *
 * (Keep this struct as small as possible to fit as much of them into CPU
 * cache line.)
 */
struct MD_MARK_tag {
    OFF beg;
    OFF end;

    /* For unresolved openers, 'prev' and 'next' form the chain of open openers
     * of given type 'ch'.
     *
     * During resolving, we disconnect from the chain and point to the
     * corresponding counterpart so opener points to its closer and vice versa.
     */
    int prev    : 24;
    int ch      : 8;    /* Only ASCII chars can form a mark. */
    int next    : 24;
    int flags   : 8;
};

/* Mark flags (these apply to ALL mark types). */
#define MD_MARK_POTENTIAL_OPENER    0x01  /* Maybe opener. */
#define MD_MARK_POTENTIAL_CLOSER    0x02  /* Maybe closer. */
#define MD_MARK_OPENER              0x04  /* Definitely opener. */
#define MD_MARK_CLOSER              0x08  /* Definitely closer. */
#define MD_MARK_RESOLVED            0x10  /* Resolved in any definite way. */
#define MD_MARK_LEAF                0x20  /* Pair does not contain any nested spans. */

/* Mark flags specific for various mark types (so they can share bits). */
#define MD_MARK_INTRAWORD           0x40  /* Helper for emphasis '*', '_' ("the rule of 3"). */
#define MD_MARK_AUTOLINK            0x40  /* Distinguisher for '<', '>'. */


static MD_MARK*
md_push_mark(MD_CTX* ctx)
{
    MD_MARK* mark;

    if(ctx->n_marks >= ctx->alloc_marks) {
        MD_MARK* new_marks;

        ctx->alloc_marks = (ctx->alloc_marks > 0 ? ctx->alloc_marks * 2 : 64);
        new_marks = realloc(ctx->marks, ctx->alloc_marks * sizeof(MD_MARK));
        if(new_marks == NULL) {
            MD_LOG("realloc() failed.");
            return NULL;
        }

        ctx->marks = new_marks;
    }

    mark = &ctx->marks[ctx->n_marks];
    ctx->n_marks++;
    return mark;
}

#define PUSH_MARK_()                                                    \
        do {                                                            \
            mark = md_push_mark(ctx);                                   \
            if(mark == NULL) {                                          \
                ret = -1;                                               \
                goto abort;                                             \
            }                                                           \
        } while(0)

#define PUSH_MARK(ch_, beg_, end_, flags_)                              \
        do {                                                            \
            PUSH_MARK_();                                               \
            mark->beg = (beg_);                                         \
            mark->end = (end_);                                         \
            mark->prev = -1;                                            \
            mark->next = -1;                                            \
            mark->ch = (char)(ch_);                                     \
            mark->flags = (flags_);                                     \
        } while(0)


static void
md_mark_chain_append(MD_CTX* ctx, MD_MARKCHAIN* chain, int mark_index)
{
    if(chain->tail >= 0)
        ctx->marks[chain->tail].next = mark_index;
    else
        chain->head = mark_index;

    ctx->marks[mark_index].prev = chain->tail;
    chain->tail = mark_index;
}

static void
md_resolve_range(MD_CTX* ctx, MD_MARKCHAIN* chain, int opener_index, int closer_index)
{
    MD_MARK* opener = &ctx->marks[opener_index];
    MD_MARK* closer = &ctx->marks[closer_index];

    /* Remove opener from the list of openers. */
    if(chain != NULL) {
        if(opener->prev >= 0)
            ctx->marks[opener->prev].next = opener->next;
        else
            chain->head = opener->next;

        if(opener->next >= 0)
            ctx->marks[opener->next].prev = opener->prev;
        else
            chain->tail = opener->prev;
    }

    /* Interconnect opener and closer and mark both as resolved. */
    opener->next = closer_index;
    opener->flags |= MD_MARK_OPENER | MD_MARK_RESOLVED;
    closer->prev = opener_index;
    closer->flags |= MD_MARK_CLOSER | MD_MARK_RESOLVED;
}


#define MD_ROLLBACK_ALL         0
#define MD_ROLLBACK_CROSSING    1

/* In the range ctx->marks[opener_index] ... [closer_index], undo some or all
 * resolvings accordingly to these rules:
 *
 * (1) All openers BEFORE the range corresponding to any closer inside the
 *     range are un-resolved and they are re-added to their respective chains
 *     of unresolved openers. This ensures we can reuse the opener for closers
 *     AFTER the range.
 *
 * (2) If 'how' is MD_ROLLBACK_ALL, then ALL resolved marks inside the range
 *     are discarded.
 *
 * (3) If 'how' is MD_ROLLBACK_CROSSING, only closers with openers handled
 *     in (1) are discarded. I.e. pairs of openers and closers which are both
 *     inside the range are retained as well as any unpaired marks.
 */
static void
md_rollback(MD_CTX* ctx, int opener_index, int closer_index, int how)
{
    int i;
    int mark_index;

    /* Cut all unresolved openers at the mark index. */
    for(i = 0; i < SIZEOF_ARRAY(ctx->mark_chains); i++) {
        MD_MARKCHAIN* chain = &ctx->mark_chains[i];

        while(chain->tail >= opener_index)
            chain->tail = ctx->marks[chain->tail].prev;

        if(chain->tail >= 0)
            ctx->marks[chain->tail].next = -1;
        else
            chain->head = -1;
    }

    /* Go backwards so that un-resolved openers are re-added into their
     * respective chains, in the right order. */
    mark_index = closer_index - 1;
    while(mark_index > opener_index) {
        MD_MARK* mark = &ctx->marks[mark_index];
        int mark_flags = mark->flags;
        int discard_flag = (how == MD_ROLLBACK_ALL);

        if(mark->flags & MD_MARK_CLOSER) {
            int mark_opener_index = mark->prev;

            /* Undo opener BEFORE the range. */
            if(mark_opener_index < opener_index) {
                MD_MARK* mark_opener = &ctx->marks[mark_opener_index];
                MD_MARKCHAIN* chain;

                mark_opener->flags &= ~(MD_MARK_OPENER | MD_MARK_CLOSER | MD_MARK_RESOLVED | MD_MARK_LEAF);

                switch(mark_opener->ch) {
                    case '*':   chain = &ASTERISK_OPENERS; break;
                    case '_':   chain = &UNDERSCORE_OPENERS; break;
                    case '`':   chain = &BACKTICK_OPENERS; break;
                    case '<':   chain = &LOWERTHEN_OPENERS; break;
                    default:    MD_UNREACHABLE(); break;
                }
                md_mark_chain_append(ctx, chain, mark_opener_index);

                discard_flag = 1;
            }
        }

        /* And reset our flags. */
        if(discard_flag)
            mark->flags &= ~(MD_MARK_OPENER | MD_MARK_CLOSER | MD_MARK_RESOLVED | MD_MARK_LEAF);

        /* Jump as far as we can over unresolved or non-interesting marks. */
        switch(how) {
            case MD_ROLLBACK_CROSSING:
                if((mark_flags & MD_MARK_CLOSER)  &&  mark->prev > opener_index) {
                    /* If we are closer with opener INSIDE the range, there may
                     * not be any other crosser inside the subrange. */
                    mark_index = mark->prev;
                    break;
                }
                /* Pass through. */
            case MD_ROLLBACK_ALL:
                if((mark_flags & (MD_MARK_CLOSER | MD_MARK_LEAF)) == (MD_MARK_CLOSER | MD_MARK_LEAF)) {
                    /* If we are closer and now there is no nested resolved mark
                     * we can also jump right to our opener. */
                    mark_index = mark->prev;
                    break;
                }
                /* Pass through. */
            default:
                mark_index--;
                break;
        }
    }

    if(how == MD_ROLLBACK_ALL) {
        ctx->marks[opener_index].flags |= MD_MARK_LEAF;
        ctx->marks[closer_index].flags |= MD_MARK_LEAF;
    }
}

/* Split a longer mark into two. The new mark takes the given count of characters.
 * May only be called if a dummy 'D' mark follows.
 */
static int
md_split_mark(MD_CTX* ctx, int mark_index, SZ n)
{
    MD_MARK* mark = &ctx->marks[mark_index];
    MD_MARK* dummy = &ctx->marks[mark_index + 1];

    MD_ASSERT(mark->end - mark->beg > n);
    MD_ASSERT(dummy->ch == 'D');

    memcpy(dummy, mark, sizeof(MD_MARK));
    mark->end -= n;
    dummy->beg = mark->end;

    return mark_index + 1;
}

static int
md_collect_marks(MD_CTX* ctx, const MD_LINE* lines, int n_lines)
{
    int i;
    int ret = 0;
    MD_MARK* mark;

    for(i = 0; i < n_lines; i++) {
        const MD_LINE* line = &lines[i];
        OFF off = line->beg;
        OFF line_end = line->end;

        while(off < line_end) {
            CHAR ch = CH(off);
            /* A backslash escape.
             * It can go beyond line->end as it may involve escaped new
             * line to form a hard break. */
            if(ch == _T('\\')  &&  off+1 < ctx->size  &&  (ISPUNCT(off+1) || ISNEWLINE(off+1))) {
                /* Hard-break cannot be on the last line of the block. */
                if(!ISNEWLINE(off+1)  ||  i+1 < n_lines)
                    PUSH_MARK(ch, off, off+2, MD_MARK_RESOLVED);

                /* If '`' follows, we need both marks as the backslash may be
                 * inside a code span. */
                if(CH(off+1) == _T('`'))
                    off++;
                else
                    off += 2;
                continue;
            }

            /* Turn non-trivial whitespace into single space. */
            if((ctx->r.flags & MD_FLAG_COLLAPSEWHITESPACE)  &&  ISWHITESPACE_(ch)) {
                OFF tmp = off+1;

                while(tmp < line_end  &&  ISWHITESPACE(tmp))
                    tmp++;

                if(tmp - off > 1  ||  ch != _T(' ')) {
                    PUSH_MARK(ch, off, tmp, MD_MARK_RESOLVED);
                    off = tmp;
                    continue;
                }
            }

            /* A potential (string) emphasis start/end. */
            if(ch == _T('*')  ||  ch == _T('_')) {
                OFF tmp = off+1;
                int left_level;     /* What precedes: 0 = whitespace; 1 = punctuation; 2 = other char. */
                int right_level;    /* What follows: 0 = whitespace; 1 = punctuation; 2 = other char. */
                unsigned flags = 0;

                while(tmp < line_end  &&  CH(tmp) == ch)
                    tmp++;

                if(off == line->beg  ||  ISWHITESPACE(off-1))
                    left_level = 0;
                else if(ISPUNCT(off-1))
                    left_level = 1;
                else
                    left_level = 2;

                if(tmp == line_end  ||  ISWHITESPACE(tmp))
                    right_level = 0;
                else if(ISPUNCT(tmp))
                    right_level = 1;
                else
                    right_level = 2;

                /* Intra-word underscore doesn't have special meaning. */
                if(ch == _T('_')  &&  left_level == 2  &&  right_level == 2) {
                    left_level = 0;
                    right_level = 0;
                }

                if(left_level > 0  &&  left_level >= right_level)
                    flags |= MD_MARK_POTENTIAL_CLOSER;
                if(right_level > 0  &&  right_level >= left_level)
                    flags |= MD_MARK_POTENTIAL_OPENER;
                if(left_level == 2  &&  right_level == 2)
                    flags |= MD_MARK_INTRAWORD;

                if(flags != 0) {
                    PUSH_MARK(ch, off, tmp, flags);

                    /* During resolving, multiple asterisks may have to be
                     * split into independent span start/ends. Consider e.g.
                     * "**foo* bar*". Therefore we push also some empty dummy
                     * marks to have enough space for that. */
                    off++;
                    while(off < tmp) {
                        PUSH_MARK('D', off, off, 0);
                        off++;
                    }
                    continue;
                }

                off = tmp;
            }

            /* A potential code span start/end. */
            if(ch == _T('`')) {
                unsigned flags;
                OFF tmp = off+1;

                /* It may be opener only if it is not escaped. */
                if(ctx->n_marks > 0  &&  ctx->marks[ctx->n_marks-1].beg == off-1  &&  CH(off-1) == _T('\\'))
                    flags = MD_MARK_POTENTIAL_CLOSER;
                else
                    flags = MD_MARK_POTENTIAL_OPENER | MD_MARK_POTENTIAL_CLOSER;

                while(tmp < line_end  &&  CH(tmp) == _T('`'))
                    tmp++;
                PUSH_MARK(ch, off, tmp, flags);

                off = tmp;
                continue;
            }

            /* A potential entity start. */
            if(ch == _T('&')) {
                PUSH_MARK(ch, off, off+1, MD_MARK_POTENTIAL_OPENER);
                off++;
                continue;
            }

            /* A potential entity end. */
            if(ch == _T(';')) {
                /* We surely cannot be entity unless the previous mark is '&'. */
                if(ctx->n_marks > 0  &&  ctx->marks[ctx->n_marks-1].ch == _T('&')) {
                    PUSH_MARK(ch, off, off+1, MD_MARK_POTENTIAL_CLOSER);
                    off++;
                    continue;
                }
            }

            /* A potential autolink or raw HTML start/end. */
            if(ch == _T('<') || ch == _T('>')) {
                if(!(ctx->r.flags & MD_FLAG_NOHTMLSPANS)) {
                    PUSH_MARK(ch, off, off+1, (ch == _T('<') ? MD_MARK_POTENTIAL_OPENER : MD_MARK_POTENTIAL_CLOSER));
                    off++;
                    continue;
                }
            }

            /* NULL character. */
            if(ch == _T('\0')) {
                PUSH_MARK(ch, off, off+1, MD_MARK_RESOLVED);
                off++;
                continue;
            }

            off++;
        }
    }

    /* Add a dummy mark after the end of processed block to simplify
     * md_process_inlines(). */
    PUSH_MARK(127, ctx->size+1, ctx->size+1, MD_MARK_RESOLVED);

abort:
    return ret;
}


/* Analyze whether the back-tick is really start/end mark of a code span.
 * If yes, reset all marks inside of it and setup flags of both marks. */
static void
md_analyze_backtick(MD_CTX* ctx, int mark_index)
{
    MD_MARK* mark = &ctx->marks[mark_index];
    int opener_index = BACKTICK_OPENERS.tail;

    /* Try to find unresolved opener of the same length. If we find it,
     * we form a code span. */
    while(opener_index >= 0) {
        MD_MARK* opener = &ctx->marks[opener_index];

        if(opener->end - opener->beg == mark->end - mark->beg) {
            /* Rollback anything found inside it.
             * (e.g. the code span contains some back-ticks or other special
             * chars we misinterpreted.) */
            md_rollback(ctx, opener_index, mark_index, MD_ROLLBACK_ALL);

            /* Resolve the span. */
            md_resolve_range(ctx, &BACKTICK_OPENERS, opener_index, mark_index);

            /* Append any space or new line inside the span into the mark
             * itself to swallow it. */
            while(CH(opener->end) == _T(' ')  ||  ISNEWLINE(opener->end))
                opener->end++;
            while(CH(mark->beg-1) == _T(' ')  ||  ISNEWLINE(mark->beg-1))
                mark->beg--;

            /* Done. */
            return;
        }

        opener_index = ctx->marks[opener_index].prev;
    }

    /* We didn't find any matching opener, so we ourselves may be the opener
     * of some upcoming closer. */
    if(mark->flags & MD_MARK_POTENTIAL_OPENER)
        md_mark_chain_append(ctx, &BACKTICK_OPENERS, mark_index);
}

static void
md_analyze_lt_gt(MD_CTX* ctx, int mark_index, const MD_LINE* lines, int n_lines)
{
    MD_MARK* mark = &ctx->marks[mark_index];
    int opener_index;

    /* If it is an opener ('<'), remember it. */
    if(mark->flags & MD_MARK_POTENTIAL_OPENER) {
        md_mark_chain_append(ctx, &LOWERTHEN_OPENERS, mark_index);
        return;
    }

    /* Otherwise we are potential closer and we try to resolve with since all
     * the chained unresolved openers. */
    opener_index = LOWERTHEN_OPENERS.head;
    while(opener_index >= 0) {
        MD_MARK* opener = &ctx->marks[opener_index];
        OFF detected_end;
        int is_autolink = 0;
        int is_raw_html = 0;

        is_autolink = (md_is_autolink(ctx, opener->beg, mark->end) == 0);

        if(!is_autolink) {
            /* Identify the line where the opening mark lives. */
            int line_index = 0;
            while(1) {
                if(opener->beg < lines[line_index].end)
                    break;
                line_index++;
            }

            is_raw_html = (md_is_html_any(ctx, lines + line_index,
                    n_lines - line_index, opener->beg, mark->end, &detected_end) == 0);
        }

        /* Check whether the range forms a valid raw HTML. */
        if(is_autolink || is_raw_html) {
            /* If this fails, it means we have missed some earlier opportunity
             * to resolve the opener. */
            MD_ASSERT(detected_end == mark->end);

            md_rollback(ctx, opener_index, mark_index, MD_ROLLBACK_ALL);
            md_resolve_range(ctx, &LOWERTHEN_OPENERS, opener_index, mark_index);

            if(is_raw_html) {
                /* Make these marks zero width so the '<' and '>' are part of its
                 * contents. */
                opener->end = opener->beg;
                mark->beg = mark->end;

                opener->flags &= ~MD_MARK_AUTOLINK;
                mark->flags &= ~MD_MARK_AUTOLINK;
            } else {
                opener->flags |= MD_MARK_AUTOLINK;
                mark->flags |= MD_MARK_AUTOLINK;
            }

            /* And we are done. */
            return;
        }

        opener_index = opener->next;
    }
}

/* Analyze whether the mark '&' starts a HTML entity.
 * If so, update its flags as well as flags of corresponding closer ';'. */
static void
md_analyze_entity(MD_CTX* ctx, int mark_index)
{
    MD_MARK* opener = &ctx->marks[mark_index];
    MD_MARK* closer;
    OFF beg, end, off;

    /* Cannot be entity if there is no closer as the next mark.
     * (Any other mark between would mean strange character which cannot be
     * part of the entity.
     *
     * So we can do all the work on '&' and do not call this later for the
     * closing mark ';'.
     */
    if(mark_index + 1 >= ctx->n_marks)
        return;
    closer = &ctx->marks[mark_index+1];
    if(closer->ch != ';')
        return;

    if(CH(opener->end) == _T('#')) {
        if(CH(opener->end+1) == _T('x') || CH(opener->end+1) == _T('X')) {
            /* It can be only a hexadecimal entity. 
             * Check it has 1 - 8 hexadecimal digits. */
            beg = opener->end+2;
            end = closer->beg;
            if(!(1 <= end - beg  &&  end - beg <= 8))
                return;
            for(off = beg; off < end; off++) {
                if(!ISXDIGIT(off))
                    return;
            }
        } else {
            /* It can be only a decimal entity.
             * Check it has 1 - 8 decimal digits. */
            beg = opener->end+1;
            end = closer->beg;
            if(!(1 <= end - beg  &&  end - beg <= 8))
                return;
            for(off = beg; off < end; off++) {
                if(!ISDIGIT(off))
                    return;
            }
        }
    } else {
        /* It can be only a named entity. 
         * Check it starts with letter and 1-47 alnum chars follow. */
        beg = opener->end;
        end = closer->beg;
        if(!(2 <= end - beg  &&  end - beg <= 48))
            return;
        if(!ISALPHA(beg))
            return;
        for(off = beg + 1; off < end; off++) {
            if(!ISALNUM(off))
                return;
        }
    }

    /* Mark us as an entity.
     * As entity has no span, we may just turn the range into a single mark.
     * (This also causes we do not get called for ';'. */
    md_resolve_range(ctx, NULL, mark_index, mark_index+1);
    opener->end = closer->end;
}

static void
md_analyze_simple_pairing_mark(MD_CTX* ctx, MD_MARKCHAIN* chain, int mark_index,
                               int apply_rule_of_three)
{
    MD_MARK* mark = &ctx->marks[mark_index];

    /* If we can be a closer, try to resolve with the preceding opener. */
    if((mark->flags & MD_MARK_POTENTIAL_CLOSER)  &&  chain->tail >= 0) {
        int opener_index = chain->tail;
        MD_MARK* opener = &ctx->marks[opener_index];
        SZ opener_size = opener->end - opener->beg;
        SZ closer_size = mark->end - mark->beg;

        if(apply_rule_of_three  &&  (mark->flags & MD_MARK_INTRAWORD)) {
            while((opener_size + closer_size) % 3 == 0) {
                if(opener->prev < 0)
                    goto cannot_resolve;

                opener = &ctx->marks[opener->prev];
                opener_size = opener->end - opener->beg;
                closer_size = mark->end - mark->beg;
            }
        }

        if(opener_size > closer_size) {
            opener_index = md_split_mark(ctx, opener_index, closer_size);
            md_mark_chain_append(ctx, chain, opener_index);
        } else if(opener_size < closer_size) {
            md_split_mark(ctx, mark_index, closer_size - opener_size);
        }

        md_rollback(ctx, opener_index, mark_index, MD_ROLLBACK_CROSSING);
        md_resolve_range(ctx, chain, opener_index, mark_index);
        return;
    }

cannot_resolve:
    /* If not resolved, and we can be an opener, remember the mark for
     * the future. */
    if(mark->flags & MD_MARK_POTENTIAL_OPENER)
        md_mark_chain_append(ctx, chain, mark_index);
}

static inline void
md_analyze_asterisk(MD_CTX* ctx, int mark_index)
{
    md_analyze_simple_pairing_mark(ctx, &ASTERISK_OPENERS, mark_index, 1);
}

static inline void
md_analyze_underscore(MD_CTX* ctx, int mark_index)
{
    md_analyze_simple_pairing_mark(ctx, &UNDERSCORE_OPENERS, mark_index, 1);
}

/* Table of precedence of various span types. */
static const CHAR* md_precedence_table[] = {
    _T("&`<>"),     /* Code spans; autolinks; raw HTML. */
    _T("*_")        /* Emphasis and string emphasis. */
};

static void
md_analyze_marks(MD_CTX* ctx, const MD_LINE* lines, int n_lines, int precedence_level)
{
    const CHAR* mark_chars = md_precedence_table[precedence_level];
    int i = 0;

    while(i < ctx->n_marks) {
        MD_MARK* mark = &ctx->marks[i];

        /* Skip resolved spans. */
        if(mark->flags & MD_MARK_RESOLVED) {
            if(mark->flags & MD_MARK_OPENER) {
                MD_ASSERT(i < mark->next);
                i = mark->next + 1;
            } else {
                i++;
            }
            continue;
        }

        /* Skip marks we do not want to deal with. */
        if(!ISANYOF_(mark->ch, mark_chars)) {
            i++;
            continue;
        }

        /* Analyze the mark. */
        switch(mark->ch) {
            case '`':   md_analyze_backtick(ctx, i); break;
            case '<':   /* Pass through. */
            case '>':   md_analyze_lt_gt(ctx, i, lines, n_lines); break;
            case '&':   md_analyze_entity(ctx, i); break;
            case '*':   md_analyze_asterisk(ctx, i); break;
            case '_':   md_analyze_underscore(ctx, i); break;
        }

        i++;
    }
}

/* Analyze marks (build ctx->marks). */
static int
md_analyze_inlines(MD_CTX* ctx, const MD_LINE* lines, int n_lines)
{
    int i;

    /* Reset the previously collected stack of marks. */
    ctx->n_marks = 0;

    /* Reset all unresolved opener mark chains. */
    for(i = 0; i < SIZEOF_ARRAY(ctx->mark_chains); i++) {
        ctx->mark_chains[i].head = -1;
        ctx->mark_chains[i].tail = -1;
    }

    /* Collect all marks. */
    if(md_collect_marks(ctx, lines, n_lines) != 0)
        return -1;

    /* Analyze all marks. */
    for(i = 0; i < SIZEOF_ARRAY(md_precedence_table); i++)
        md_analyze_marks(ctx, lines, n_lines, i);

    return 0;
}

/* Render the output, accordingly to the analyzed ctx->marks. */
static int
md_process_inlines(MD_CTX* ctx, const MD_LINE* lines, int n_lines)
{
    union {
        MD_SPAN_A_DETAIL a;
    } det;
    MD_TEXTTYPE text_type;
    const MD_LINE* line = lines;
    const MD_MARK* prev_mark = NULL;
    const MD_MARK* mark;
    OFF off = lines[0].beg;
    OFF end = lines[n_lines-1].end;
    int enforce_hardbreak = 0;
    int ret = 0;

    /* Find first resolved mark. Note there is always at least one resolved
     * mark,  the dummy last one after the end of the latest line we actually
     * never really reach. This saves us of a lot of special checks and cases
     * in this function. */
    mark = ctx->marks;
    while(!(mark->flags & MD_MARK_RESOLVED))
        mark++;

    text_type = MD_TEXT_NORMAL;

    while(1) {
        /* Process the text up to the next mark or end-of-line. */
        OFF tmp = (line->end < mark->beg ? line->end : mark->beg);
        if(tmp > off) {
            MD_TEXT(text_type, STR(off), tmp - off);
            off = tmp;
        }

        /* If reached the mark, process it and move to next one. */
        if(off >= mark->beg) {
            switch(mark->ch) {
                case '\\':      /* Backslash escape. */
                    if(ISNEWLINE(mark->beg+1))
                        enforce_hardbreak = 1;
                    else
                        MD_TEXT(text_type, STR(mark->beg+1), 1);
                    break;

                case ' ':       /* Non-trivial space. */
                    MD_TEXT(text_type, _T(" "), 1);
                    break;

                case '`':       /* Code span. */
                    if(mark->flags & MD_MARK_OPENER) {
                        MD_ENTER_SPAN(MD_SPAN_CODE, NULL);
                        text_type = MD_TEXT_CODE;
                    } else {
                        MD_LEAVE_SPAN(MD_SPAN_CODE, NULL);
                        text_type = MD_TEXT_NORMAL;
                    }
                    break;

                case '_':
                case '*':       /* Emphasis, strong emphasis. */
                    if(mark->flags & MD_MARK_OPENER) {
                        while(off + 1 < mark->end) {
                            MD_ENTER_SPAN(MD_SPAN_STRONG, NULL);
                            off += 2;
                        }
                        if(off < mark->end)
                            MD_ENTER_SPAN(MD_SPAN_EM, NULL);
                    } else {
                        if((mark->end - off) & 0x01) {
                            MD_LEAVE_SPAN(MD_SPAN_EM, NULL);
                            off++;
                        }
                        while(off < mark->end) {
                            MD_LEAVE_SPAN(MD_SPAN_STRONG, NULL);
                            off += 2;
                        }
                    }
                    break;

                case '<':       /* Autolink or raw HTML. */
                    if(mark->flags & MD_MARK_AUTOLINK) {
                        det.a.href = STR(mark->end);
                        det.a.href_size = ctx->marks[mark->next].beg - mark->end;
                        MD_ENTER_SPAN(MD_SPAN_A, (void*) &det);
                    } else {
                        text_type = MD_TEXT_HTML;
                    }
                    break;

                case '>':
                    if(mark->flags & MD_MARK_AUTOLINK)
                        /* The detail already has to be initialized: There cannot
                         * be any resolved mark between the autlink opener and
                         * closer. */
                        MD_LEAVE_SPAN(MD_SPAN_A, (void*) &det);
                    else
                        text_type = MD_TEXT_NORMAL;
                    break;

                case '&':       /* Entity. */
                    MD_TEXT(MD_TEXT_ENTITY, STR(mark->beg), mark->end - mark->beg);
                    break;

                case '\0':
                    MD_TEXT(MD_TEXT_NULLCHAR, _T(""), 1);
                    break;
            }

            off = mark->end;

            /* Move to next resolved mark. */
            prev_mark = mark;
            mark++;
            while(!(mark->flags & MD_MARK_RESOLVED))
                mark++;
        }

        /* If reached end of line, move to next one. */
        if(off >= line->end) {
            /* If it is the last line, we are done. */
            if(off >= end)
                break;

            if(text_type == MD_TEXT_CODE) {
                /* Inside code spans, new lines are transformed into single
                 * spaces. */
                MD_ASSERT(prev_mark != NULL);
                MD_ASSERT(prev_mark->ch == '`'  &&  (prev_mark->flags & MD_MARK_OPENER));
                MD_ASSERT(mark->ch == '`'  &&  (mark->flags & MD_MARK_CLOSER));

                if(prev_mark->end < off  &&  off < mark->beg)
                    MD_TEXT(MD_TEXT_CODE, _T(" "), 1);
            } else {
                /* Output soft or hard line break. */
                MD_TEXTTYPE break_type = MD_TEXT_SOFTBR;

                if(text_type == MD_TEXT_NORMAL) {
                    if(enforce_hardbreak)
                        break_type = MD_TEXT_BR;
                    else if((CH(line->end) == _T(' ') && CH(line->end+1) == _T(' ')))
                        break_type = MD_TEXT_BR;
                }

                MD_TEXT(break_type, _T("\n"), 1);
            }

            /* Switch to the following line. */
            line++;
            off = line->beg;

            enforce_hardbreak = 0;
        }
    }

abort:
    return ret;
}


/******************************************
 ***  Processing Single Block Contents  ***
 ******************************************/

static int
md_process_normal_block(MD_CTX* ctx, const MD_LINE* lines, int n_lines)
{
    int ret;

    MD_CHECK(md_analyze_inlines(ctx, lines, n_lines));
    MD_CHECK(md_process_inlines(ctx, lines, n_lines));

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
        MD_TEXT_INSECURE(text_type, STR(line->beg), line->end - line->beg);

        /* Enforce end-of-line. */
        MD_TEXT(text_type, _T("\n"), 1);
    }

abort:
    return ret;
}

static int
md_process_code_block(MD_CTX* ctx, const MD_LINE* lines, int n_lines)
{
    /* Ignore blank lines at start/end of indented code block. */
    if(lines[0].type == MD_LINE_INDENTEDCODE) {
        while(n_lines > 0  &&  lines[0].beg == lines[0].end) {
            lines++;
            n_lines--;
        }
        while(n_lines > 0  &&  lines[n_lines-1].beg == lines[n_lines-1].end) {
            n_lines--;
        }
    }

    /* Skip the first line in case of fenced code: It is the fence.
     * (Only the starting fence is present due to logic in md_analyze_line().) */
    if(lines[0].type == MD_LINE_FENCEDCODE) {
        lines++;
        n_lines--;
    }

    if(n_lines == 0)
        return 0;

    return md_process_verbatim_block(ctx, MD_TEXT_CODE, lines, n_lines);
}


/***************************************
 ***  Breaking Document into Blocks  ***
 ***************************************/

static int
md_is_hr_line(MD_CTX* ctx, OFF beg, OFF* p_end)
{
    OFF off = beg + 1;
    int n = 1;

    while(off < ctx->size  &&  (CH(off) == CH(beg) || CH(off) == _T(' ') || CH(off) == _T('\t'))) {
        if(CH(off) == CH(beg))
            n++;
        off++;
    }

    if(n < 3)
        return -1;

    /* Nothing else can be present on the line. */
    if(off < ctx->size  &&  !ISNEWLINE(off))
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

    if(!(ctx->r.flags & MD_FLAG_PERMISSIVEATXHEADERS)  &&  off < ctx->size  &&
       CH(off) != _T(' ')  &&  CH(off) != _T('\t')  &&  !ISNEWLINE(off))
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
    if(off + 1 < ctx->size) {
        OFF end;

        if(md_is_html_tag(ctx, NULL, 0, beg, ctx->size, &end) == 0) {
            /* Only optional whitespace and new line may follow. */
            while(end < ctx->size  &&  ISWHITESPACE(end))
                end++;
            if(end >= ctx->size  ||  ISNEWLINE(end))
                return 7;
        }
    }

    return -1;
}

/* Case sensitive check whether there is a substring 'what' between 'beg'
 * and end of line. */
static int
md_line_contains(MD_CTX* ctx, OFF beg, const CHAR* what, SZ what_len, OFF* p_end)
{
    OFF i;
    for(i = beg; i + what_len < ctx->size; i++) {
        if(ISNEWLINE(i))
            break;
        if(memcmp(STR(i), what, what_len * sizeof(CHAR)) == 0) {
            *p_end = i + what_len;
            return 0;
        }
    }

    *p_end = i;
    return -1;
}

/* Returns type of HTML block end condition or -1 if not an end condition.
 *
 * Note it fills p_end even when it is not end condition as the caller
 * does not need to analyze contents of a raw HTML block.
 */
static int
md_is_html_block_end_condition(MD_CTX* ctx, OFF beg, OFF* p_end)
{
    switch(ctx->html_block_type) {
        case 1:
        {
            OFF off = beg;

            while(off < ctx->size  &&  !ISNEWLINE(off)) {
                if(CH(off) == _T('<')) {
                    if(md_str_case_eq(STR(off), _T("</script>"), 9) == 0) {
                        *p_end = off + 9;
                        return 1;
                    }

                    if(md_str_case_eq(STR(off), _T("</style>"), 8) == 0) {
                        *p_end = off + 8;
                        return 1;
                    }

                    if(md_str_case_eq(STR(off), _T("</pre>"), 6) == 0) {
                        *p_end = off + 6;
                        return 1;
                    }
                }

                off++;
            }
            *p_end = off;
            return -1;
        }

        case 2:
            return (md_line_contains(ctx, beg, _T("-->"), 3, p_end) == 0 ? 2 : -1);

        case 3:
            return (md_line_contains(ctx, beg, _T("?>"), 2, p_end) == 0 ? 3 : -1);

        case 4:
            return (md_line_contains(ctx, beg, _T(">"), 1, p_end) == 0 ? 4 : -1);

        case 5:
            return (md_line_contains(ctx, beg, _T("]]>"), 3, p_end) == 0 ? 5 : -1);

        case 6:     /* Pass through */
        case 7:
            *p_end = beg;
            return (ISNEWLINE(beg) ? ctx->html_block_type : -1);

        default:
            MD_UNREACHABLE();
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
    if(pivot_line->type == MD_LINE_FENCEDCODE  &&  line->quote_level == pivot_line->quote_level) {
        /* We are another MD_LINE_FENCEDCODE unless we are closing fence
         * which we transform into MD_LINE_BLANK. */
        if(line->indent < ctx->code_indent_offset) {
            if(md_is_closing_code_fence(ctx, off, &off) == 0) {
                line->type = MD_LINE_BLANK;
                goto done;
            }
        }

        /* Change indentation accordingly to the initial code fence. */
        if(line->indent > pivot_line->indent)
            line->indent -= pivot_line->indent;
        else
            line->indent = 0;

        line->type = MD_LINE_FENCEDCODE;
        goto done;
    }

    /* Check whether we are indented code line.
     * Note indented code block cannot interrupt paragraph. */
    if((pivot_line->type == MD_LINE_BLANK || pivot_line->type == MD_LINE_INDENTEDCODE)
        && line->indent >= ctx->code_indent_offset)
    {
        line->type = MD_LINE_INDENTEDCODE;
        line->indent -= ctx->code_indent_offset;
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

    /* Check whether we are HTML block continuation. */
    if(pivot_line->type == MD_LINE_HTML  &&  ctx->html_block_type > 0) {
        int html_block_type;

        html_block_type = md_is_html_block_end_condition(ctx, off, &off);
        if(html_block_type >= 0) {
            MD_ASSERT(html_block_type == ctx->html_block_type);

            /* Make sure this is the last line of the block. */
            ctx->html_block_type = 0;

            /* Some end conditions serve as blank lines at the same time. */
            if(html_block_type == 6 || html_block_type == 7) {
                line->type = MD_LINE_BLANK;
                line->indent = 0;
                goto done;
            }
        }

        line->type = MD_LINE_HTML;
        goto done;
    }

    /* Check whether we are blank line.
     * Note blank lines after indented code are treated as part of that block.
     * If they are at the end of the block, it is discarded by caller.
     */
    if(off >= ctx->size  ||  ISNEWLINE(off)) {
        line->indent = 0;
        if(pivot_line->type == MD_LINE_INDENTEDCODE  &&  line->quote_level == pivot_line->quote_level)
            line->type = MD_LINE_INDENTEDCODE;
        else
            line->type = MD_LINE_BLANK;
        goto done;
    }

    /* Check whether we are ATX header.
     * (We check the indentation to fix http://spec.commonmark.org/0.26/#example-40) */
    if(line->indent < ctx->code_indent_offset  &&  CH(off) == _T('#')) {
        if(md_is_atxheader_line(ctx, off, &line->beg, &off) == 0) {
            line->type = MD_LINE_ATXHEADER;
            goto done;
        }
    }

    /* Check whether we are Setext underline. */
    if(line->indent < ctx->code_indent_offset  &&  pivot_line->type == MD_LINE_TEXT
        &&  line->quote_level == pivot_line->quote_level
        && (CH(off) == _T('=') || CH(off) == _T('-')))
    {
        if(md_is_setext_underline(ctx, off, &off) == 0) {
            line->type = MD_LINE_SETEXTUNDERLINE;
            goto done;
        }
    }

    /* Check whether we are thematic break line.
     * (We check the indentation to fix http://spec.commonmark.org/0.26/#example-19)
     * (Keep this after check for Setext underline as that one has higher priority). */
    if(line->indent < ctx->code_indent_offset  &&  ISANYOF(off, _T("-_*"))) {
        if(md_is_hr_line(ctx, off, &off) == 0) {
            line->type = MD_LINE_HR;
            goto done;
        }
    }

    /* Check whether we are starting code fence. */
    if(CH(off) == _T('`') || CH(off) == _T('~')) {
        if(md_is_opening_code_fence(ctx, off, &off) == 0) {
            ctx->code_fence_indent = line->indent;
            line->type = MD_LINE_FENCEDCODE;
            goto done;
        }
    }

    /* Check whether we are start of raw HTML block. */
    if(CH(off) == _T('<')  &&  !(ctx->r.flags & MD_FLAG_NOHTMLBLOCKS)
        &&  line->indent < ctx->code_indent_offset)
    {
        ctx->html_block_type = md_is_html_block_start_condition(ctx, off);

        /* HTML block type 7 cannot interrupt paragraph. */
        if(ctx->html_block_type == 7  &&  pivot_line->type == MD_LINE_TEXT)
            ctx->html_block_type = -1;

        if(ctx->html_block_type > 0) {
            /* The line itself also may immediately close the block. */
            if(md_is_html_block_end_condition(ctx, off, &off) == ctx->html_block_type) {
                /* Make sure this is the last line of the block. */
                ctx->html_block_type = 0;
            }

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
    if(line->type != MD_LINE_INDENTEDCODE  &&  line->type != MD_LINE_FENCEDCODE) {
        while(line->end > line->beg && CH(line->end-1) == _T(' '))
            line->end--;
    }

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

    memset(&det, 0, sizeof(det));

    /* Make sure the processed leaf block lives in the proper block quote
     * nesting level. */
    MD_CHECK(md_process_blockquote_nesting(ctx, lines[0].quote_level));

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
            block_type = MD_BLOCK_CODE;
            break;

        case MD_LINE_FENCEDCODE:
            block_type = MD_BLOCK_CODE;
            if(ctx->code_fence_info_beg < ctx->code_fence_info_end)
                det.code.info = STR(ctx->code_fence_info_beg);
            else
                det.code.info = NULL;
            det.code.info_size = ctx->code_fence_info_end - ctx->code_fence_info_beg;

            det.code.lang = det.code.info;
            det.code.lang_size = 0;
            while(det.code.lang_size < det.code.info_size
                        &&  !ISWHITESPACE_(det.code.lang[det.code.lang_size]))
                det.code.lang_size++;

            break;

        case MD_LINE_TEXT:
            block_type = MD_BLOCK_P;
            break;

        case MD_LINE_HTML:
            block_type = MD_BLOCK_HTML;
            break;

        case MD_LINE_SETEXTUNDERLINE:
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
            ret = md_process_code_block(ctx, lines, n_lines);
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
    MD_LINE* lines = NULL;
    int alloc_lines = 0;
    int n_lines = 0;
    int pivot_line_index = -1;  /* Points to a line determining type of block. */
    OFF off = 0;
    int ret = 0;

    MD_ENTER_BLOCK(MD_BLOCK_DOC, NULL);

    while(off < ctx->size) {
        static const MD_LINE dummy_line = { MD_LINE_BLANK, 0 };
        const MD_LINE* pivot_line;
        MD_LINE* line;

        if(n_lines >= alloc_lines) {
            MD_LINE* new_lines;

            alloc_lines = (alloc_lines == 0 ? 32 : alloc_lines * 2);
            new_lines = (MD_LINE*) realloc(lines, alloc_lines * sizeof(MD_LINE));
            if(new_lines == NULL) {
                MD_LOG("realloc() failed.");
                ret = -1;
                goto abort;
            }

            lines = new_lines;
        }

        pivot_line = (pivot_line_index >= 0 ? &lines[pivot_line_index] : &dummy_line);

        md_analyze_line(ctx, off, &off, pivot_line, &lines[n_lines]);
        line = &lines[n_lines];

        /* Some line types form block on their own. */
        if(line->type == MD_LINE_HR || line->type == MD_LINE_ATXHEADER) {
            /* Flush accumulated lines. */
            MD_CHECK(md_process_block(ctx, lines, n_lines));

            /* Flush ourself. */
            MD_CHECK(md_process_block(ctx, line, 1));

            pivot_line_index = -1;
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
            MD_CHECK(md_process_block(ctx, lines, n_lines));

            /* Keep the current line as the new pivot. */
            if(line != &lines[0])
                memcpy(&lines[0], line, sizeof(MD_LINE));
            pivot_line_index = 0;
            n_lines = 1;
            continue;
        }

        /* Otherwise we just accumulate the line into ongoing block. */
        n_lines++;
    }

    /* Process also the last block. */
    MD_CHECK(md_process_block(ctx, lines, n_lines));

    /* Close any dangling parent blocks. */
    MD_CHECK(md_process_blockquote_nesting(ctx, 0));

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
    int ret;

    /* Setup context structure. */
    memset(&ctx, 0, sizeof(MD_CTX));
    ctx.text = text;
    ctx.size = size;
    memcpy(&ctx.r, renderer, sizeof(MD_RENDERER));
    ctx.userdata = userdata;

    /* Offset for indented code block. */
    ctx.code_indent_offset = (ctx.r.flags & MD_FLAG_NOINDENTEDCODEBLOCKS) ? (OFF)(-1) : 4;

    /* All the work. */
    ret = md_process_doc(&ctx);

    /* Clean-up. */
    free(ctx.marks);

    return ret;
}
