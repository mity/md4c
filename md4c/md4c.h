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

#ifndef MD4C_MARKDOWN_H
#define MD4C_MARKDOWN_H

#ifdef __cplusplus
    extern "C" {
#endif


/* Magic to support UTF16-LE (i.e. what is called Unicode among Windows
 * developers) input/output on Windows.
 *
 * On most platforms, we handle char strings and do not care about encoding
 * as far as the controlling Markdown syntax is actually ASCII-friendly.
 * The actual text is provided into callbacks as it is.
 *
 * On Windows, when UNICODE is defined, we by default switch to WCHAR.
 * This behavior may be disabled by predefining MD4C_DISABLE_WIN_UNICODE.
 */
#if defined MD4C_USE_WIN_UNICODE
    #include <windows.h>
    typedef WCHAR   MD_CHAR;
#else
    typedef char    MD_CHAR;
#endif

typedef unsigned MD_SIZE;
typedef unsigned MD_OFFSET;


/* Block represents a part of document hierarchy structure like a paragraph
 * or list item. */
typedef enum MD_BLOCKTYPE_tag MD_BLOCKTYPE;
enum MD_BLOCKTYPE_tag {
    /* <body>...</body> */
    MD_BLOCK_DOC = 0,

    /* <blockquote>...</blockquote> */
    MD_BLOCK_QUOTE,

    /* <ul>...</ul> */
    MD_BLOCK_UL,

    /* <ol>...</ol>
     * Detail: Structure MD_BLOCK_OL_DETAIL. */
    MD_BLOCK_OL,

    /* <li>...</li> */
    MD_BLOCK_LI,

    /* <hr> */
    MD_BLOCK_HR,

    /* <h1>...</h1> (for levels up to 6)
     * Detail: Structure MD_BLOCK_H_DETAIL. */
    MD_BLOCK_H,

    /* <pre><code>...</code></pre>
     * Note the text lines within code blocks are terminated with '\n'
     * instead of explicit MD_TEXT_BR. */
    MD_BLOCK_CODE,

    /* Raw HTML block. This itself does not correspond to any particular HTML
     * tag. The contents of it _is_ raw HTML source intended to be put
     * in verbatim form to the HTML output. */
    MD_BLOCK_HTML,

    /* <p>...</p> */
    MD_BLOCK_P,

    /* <table>...</table> and its contents.
     * Detail: Structure MD_BLOCK_TD_DETAIL (used with MD_BLOCK_TH and MD_BLOCK_TD)
     * Note all of these are used only if extension MD_FLAG_TABLES is enabled. */
    MD_BLOCK_TABLE,
    MD_BLOCK_THEAD,
    MD_BLOCK_TBODY,
    MD_BLOCK_TR,
    MD_BLOCK_TH,
    MD_BLOCK_TD
};

/* Span represents an in-line piece of a document which should be rendered with
 * the same font, color and other attributes. A sequence of spans forms a block
 * like paragraph or list item. */
typedef enum MD_SPANTYPE_tag MD_SPANTYPE;
enum MD_SPANTYPE_tag {
    /* <em>...</em> */
    MD_SPAN_EM,

    /* <strong>...</strong> */
    MD_SPAN_STRONG,

    /* <a href="xxx">...</a>
     * Detail: Structure MD_SPAN_A_DETAIL. */
    MD_SPAN_A,

    /* <img src="xxx">...</a>
     * Detail: Structure MD_SPAN_IMG_DETAIL. */
    MD_SPAN_IMG,

    /* <code>...</code> */
    MD_SPAN_CODE
};

/* Text is the actual textual contents of span. */
typedef enum MD_TEXTTYPE_tag MD_TEXTTYPE;
enum MD_TEXTTYPE_tag {
    /* Normal text. */
    MD_TEXT_NORMAL = 0,

    /* NULL character. Markdown is supposed to replace NULL character with
     * the replacement char U+FFFD but since we are encoding agnostic, caller
     * has to do that. */
    MD_TEXT_NULLCHAR,

    /* Line breaks.
     * Note these are only sent within MD_BLOCK_CODE or MD_BLOCK_HTML. */
    MD_TEXT_BR,         /* <br> (hard break) */
    MD_TEXT_SOFTBR,     /* '\n' in source text where it is not semantically meaningful (soft break) */

    /* Entity.
     * (a) Named entity, e.g. &nbsp; 
     *     (Note MD4C does not have a lsit of known entities.
     *     Anything matching the regexp /&[A-Za-z][A-Za-z0-9]{1,47};/ is
     *     treated as a named entity.)
     * (b) Numerical entity, e.g. &#1234;
     * (c) Hexadecimal entity, e.g. &#x12AB;
     *
     * As MD4C is encoding agnostic, application gets the verbatim entity
     * text into the MD_RENDERER::text_callback(). */
    MD_TEXT_ENTITY,

    /* Text in a code block (inside MD_BLOCK_CODE) or inlined code (`code`).
     * If it is inside MD_BLOCK_CODE, it includes spaces for indentation and
     * '\n' for new lines. MD_TEXT_BR and MD_TEXT_SOFTBR are not sent for this
     * kind of text. */
    MD_TEXT_CODE,

    /* Text is a raw HTML. */
    MD_TEXT_HTML
};


/* Alignment enumeration. */
typedef enum MD_ALIGN_tag MD_ALIGN;
enum MD_ALIGN_tag {
    MD_ALIGN_DEFAULT = 0,   /* When unspecified. */
    MD_ALIGN_LEFT,
    MD_ALIGN_CENTER,
    MD_ALIGN_RIGHT
};


/* Detailed info for MD_BLOCK_H. */
typedef struct MD_BLOCK_OL_DETAIL_tag MD_BLOCK_OL_DETAIL;
struct MD_BLOCK_OL_DETAIL_tag {
    unsigned start;         /* Start index of the ordered list. */
};

/* Detailed info for MD_BLOCK_H. */
typedef struct MD_BLOCK_H_DETAIL_tag MD_BLOCK_H_DETAIL;
struct MD_BLOCK_H_DETAIL_tag {
    unsigned level;         /* Header level (1 - 6) */
};

/* Detailed info for MD_BLOCK_CODE. */
typedef struct MD_BLOCK_CODE_DETAIL_tag MD_BLOCK_CODE_DETAIL;
struct MD_BLOCK_CODE_DETAIL_tag {
    /* Complete "info string" */
    const MD_CHAR* info;
    MD_SIZE info_size;

    /* Language portion of the info string. */
    const MD_CHAR* lang;
    MD_SIZE lang_size;
};

/* Detailed info for MD_BLOCK_TH and MD_BLOCK_TD. */
typedef struct MD_BLOCK_TD_DETAIL_tag MD_BLOCK_TD_DETAIL;
struct MD_BLOCK_TD_DETAIL_tag {
    MD_ALIGN align;
};

/* Detailed info for MD_SPAN_A. */
typedef struct MD_SPAN_A_DETAIL_tag MD_SPAN_A_DETAIL;
struct MD_SPAN_A_DETAIL_tag {
    const MD_CHAR* href;
    MD_SIZE href_size;

    const MD_CHAR* title;
    MD_SIZE title_size;
};

/* Detailed info for MD_SPAN_IMG. */
typedef struct MD_SPAN_IMG_DETAIL_tag MD_SPAN_IMG_DETAIL;
struct MD_SPAN_IMG_DETAIL_tag {
    const MD_CHAR* src;
    MD_SIZE src_size;

    const MD_CHAR* alt;
    MD_SIZE alt_size;

    const MD_CHAR* title;
    MD_SIZE title_size;
};


/* Flags specifying Markdown dialect.
 *
 * By default (when MD_RENDERER::flags == 0), we follow CommonMark specification.
 * The following flags may allow some extensions or deviations from it.
 */
#define MD_FLAG_COLLAPSEWHITESPACE          0x0001  /* In MD_TEXT_NORMAL, collapse non-trivial whitespace into single ' ' */
#define MD_FLAG_PERMISSIVEATXHEADERS        0x0002  /* Do not require space in ATX headers ( ###header ) */
#define MD_FLAG_PERMISSIVEURLAUTOLINKS      0x0004  /* Recognize URLs as autolinks even without '<', '>' */
#define MD_FLAG_PERMISSIVEEMAILAUTOLINKS    0x0008  /* Recognize e-mails as autolinks even without '<', '>' and 'mailto:' */
#define MD_FLAG_PERMISSIVEAUTOLINKS         (MD_FLAG_PERMISSIVEURLAUTOLINKS | MD_FLAG_PERMISSIVEEMAILAUTOLINKS)
#define MD_FLAG_NOINDENTEDCODEBLOCKS        0x0010  /* Disable indented code blocks. (Only fenced code works.) */
#define MD_FLAG_NOHTMLBLOCKS                0x0020  /* Disable raw HTML blocks. */
#define MD_FLAG_NOHTMLSPANS                 0x0040  /* Disable raw HTML (inline). */
#define MD_FLAG_NOHTML                      (MD_FLAG_NOHTMLBLOCKS | MD_FLAG_NOHTMLSPANS)
#define MD_FLAG_TABLES                      0x0100  /* Enable tables extension. */

/* Caller-provided callbacks.
 *
 * For some block/span types, more detailed information is provided in a
 * type-specific structure pointed by the argument 'detail'.
 *
 * The last argument of all callbacks, 'userdata', is just propagated from
 * md_parse() and is available for any use by the application.
 *
 * Callbacks may abort further parsing of the document by returning non-zero.
 */
typedef struct MD_RENDERER_tag MD_RENDERER;
struct MD_RENDERER_tag {
    int (*enter_block)(MD_BLOCKTYPE /*type*/, void* /*detail*/, void* /*userdata*/);
    int (*leave_block)(MD_BLOCKTYPE /*type*/, void* /*detail*/, void* /*userdata*/);

    int (*enter_span)(MD_SPANTYPE /*type*/, void* /*detail*/, void* /*userdata*/);
    int (*leave_span)(MD_SPANTYPE /*type*/, void* /*detail*/, void* /*userdata*/);

    int (*text)(MD_TEXTTYPE /*type*/, const MD_CHAR* /*text*/, MD_SIZE /*size*/, void* /*userdata*/);

    /* If not NULL and something goes wrong, this function gets called.
     * This is intended for debugging and problem diagnosis for developers;
     * it is not intended to provide any errors suitable for displaying to an
     * end user.
     */
    void (*debug_log)(const char* /*msg*/, void* /*userdata*/);

    /* Dialect options. */
    unsigned flags;
};


/* Parse the Markdown document stored in the string 'text' of size 'size'.
 * The renderer provides callbacks to be called during the parsing so the
 * caller can render the document on the screen or convert the Markdown
 * to another format.
 *
 * Zero is returned on success. If a runtime error occurs (e.g. a memory
 * fails), -1 is returned. If an internal error occurs (i.e. an internal
 * assertion fails, implying there is a bug in MD4C), then -2 is returned.
 * If the processing is aborted due any callback returning non-zero,
 * md_parse() returns return value of the callback.
 */
int md_parse(const MD_CHAR* text, MD_SIZE size, const MD_RENDERER* renderer, void* userdata);


#ifdef __cplusplus
    }  /* extern "C" { */
#endif

#endif  /* MD4C_MARKDOWN_H */
