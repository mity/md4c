/*
 * MD4C: Markdown parser for C
 * (http://github.com/mity/md4c)
 *
 * Copyright (c) 2016-2024 Martin Mitáš
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

#ifndef MD4C_HTML_H
#define MD4C_HTML_H

#include "md4c.h"

#ifdef __cplusplus
    extern "C" {
#endif


/* If set, debug output from md_parse() is sent to stderr. */
#define MD_HTML_FLAG_DEBUG                  0x0001
#define MD_HTML_FLAG_VERBATIM_ENTITIES      0x0002
#define MD_HTML_FLAG_SKIP_UTF8_BOM          0x0004  /* Deprecated; use MD_FLAG_SKIPBOM on the parser side in new code. */
#define MD_HTML_FLAG_XHTML                  0x0008


/* Simple do-it-all function for converting Markdown to HTML.
 *
 * Note only contents of <body> tag is generated. Caller must generate
 * HTML header/footer manually before/after calling md_html().
 *
 * For more control over the conversion (e.g. to customize the output), you may
 * use more fine-grained API below.
 *
 * Params input and input_size specify the Markdown input.
 * Callback process_output() gets called with chunks of HTML output.
 * (Typical implementation may just output the bytes to a file or append to
 * some buffer).
 * Param userdata is just propagated back to process_output() callback.
 * Param parser_flags are flags from md4c.h propagated to md_parse().
 * Param render_flags is bitmask of MD_HTML_FLAG_xxxx.
 *
 * Returns -1 on error (if md_parse() fails.)
 * Returns 0 on success.
 */
int md_html(const MD_CHAR* input, MD_SIZE input_size,
            void (*process_output)(const MD_CHAR*, MD_SIZE, void*),
            void* userdata, unsigned parser_flags, unsigned renderer_flags);


/* The functions below provide more finer-grained building blocks, which allow
 * application to e.g. customize how (some) Markdown syntax constructions are
 * converted into HTML.
 *
 * The call to md_html() above is morally equivalent to this code:
 *
 * ``` C
 * #include "md4c.h"
 * #include "md4c-html.h"
 *
 * int
 * md_html(const MD_CHAR* input, MD_SIZE input_size,
 *         void (*process_output)(const MD_CHAR*, MD_SIZE, void*),
 *         void* userdata, unsigned parser_flags, unsigned renderer_flags)
 * {
 *     MD_HTML* mh;
 *     MD_PARSER_v2 p;
 *     int ret;
 *
 *     mh = md_html_create(process_output, userdata, parser_flags, renderer_flags);
 *     if(mh == NULL)
 *         return -1;
 *
 *     memset(&p, 0, sizeof(p));
 *     p.abi_version = 2;
 *     p.flags = parser_flags;
 *     p.enter_block = md_html_enter_block;
 *     p.leave_block = md_html_leave_block;
 *     p.enter_span = md_html_enter_span;
 *     p.leave_span = md_html_leave_span;
 *     p.text = md_html_text;
 *     p.debug_log = md_html_debug_log;
 *
 *     ret = md_parse(input, input_size, (MD_PARSER*) &p, (void*) mh);
 *
 *     md_html_destroy(mh);
 *     return ret;
 * }
 * ```
 *
 * This allows application to implement its own callbacks for md_parse()
 * which may provide custom output e.g. for some block and/or span types, and
 * calls the original callback for block/span types it does not want to
 * customize.
 */

/* An opaque structure representing the Markdown-to-HTML converter. */
typedef struct MD_HTML MD_HTML;

/* Create/destroy the Markdown-to-HTML converter structure. */
MD_HTML* md_html_create(void (*process_output)(const MD_CHAR*, MD_SIZE, void*),
                void* userdata, unsigned renderer_flags);
void md_html_destroy(MD_HTML* mh);

/* Standard HTML callbacks for MD_PARSER.
 *
 * (Application can use its own callback and use these functions as "fallback"
 * for stuff it does not want to customize. In such case the application is
 * responsible for propagating MD_HTML* returned from md_html_create() as
 * userdata to these standard callbacks.)
 */
int md_html_enter_block(int block_type, void* detail, void* userdata);
int md_html_leave_block(int block_type, void* detail, void* userdata);
int md_html_enter_span(int span_type, void* detail, void* userdata);
int md_html_leave_span(int span_type, void* detail, void* userdata);
int md_html_text(int text_type, const MD_CHAR* text, MD_SIZE size, void* userdata);
void md_html_debug_log(const char* msg, void* userdata);

/* Functions to call from custom md_parser() callbacks, to make an output. */
void md_html_output_verbatim(MD_HTML* mh, const MD_CHAR* test, MD_SIZE size);
void md_html_output_escaped(MD_HTML* mh, const MD_CHAR* test, MD_SIZE size);
void md_html_output_url_escaped(MD_HTML* mh, const MD_CHAR* test, MD_SIZE size);


#ifdef __cplusplus
    }  /* extern "C" { */
#endif

#endif  /* MD4C_HTML_H */
