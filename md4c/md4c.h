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
#if defined _WIN32  &&  defined UNICODE  &&  !defined MD4C_DISABLE_WIN_UNICODE
    #include <windows.h>

    #define MD4C_USE_WIN_UNICODE
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

    /* <hr> */
    MD_BLOCK_HR,

    /* <p>...</p> */
    MD_BLOCK_P
};


/* Span represents an in-line piece of a document which should be rendered with
 * the same font, color and other attributes. A sequence of spans forms a block
 * like paragraph or list item. */
typedef enum MD_SPANTYPE_tag MD_SPANTYPE;
enum MD_SPANTYPE_tag {
    MD_SPAN_DUMMY = 0       /* not yet used... */
};


/* Text is the actual textual contents of span. */
typedef enum MD_TEXTTYPE_tag MD_TEXTTYPE;
enum MD_TEXTTYPE_tag {
    /* Normal text. */
    MD_TEXT_NORMAL = 0
};


/* Caller-provided callbacks.
 *
 * For some block/span types, more detailed information is provided in a
 * type-specific structure pointed by the argument 'detail'.
 *
 * The last argument of all callbacks, 'userdata', is just propagated from
 * md_parse() and is available for ue by the caller.
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
