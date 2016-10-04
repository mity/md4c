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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "md4c.h"
#include "cmdline.h"


/*********************************
 ***  Simple grow-able buffer  ***
 *********************************/

/* We render to a memory buffer instead of directly outputting the rendered
 * documents, as this allows using this utility for evaluating performance
 * of MD4C (--stat option). This allows us to measure just time of the parser,
 * without the I/O.
 */

struct membuffer {
    char* data;
    MD_SIZE asize;
    MD_SIZE size;
};

static void
membuf_init(struct membuffer* buf, MD_SIZE new_asize)
{
    buf->size = 0;
    buf->asize = new_asize;
    buf->data = malloc(buf->asize);
    if(buf->data == NULL) {
        fprintf(stderr, "membuf_init: malloc() failed.");
        exit(1);
    }
}

static void
membuf_fini(struct membuffer* buf)
{
    if(buf->data)
        free(buf->data);
}

static void
membuf_grow(struct membuffer* buf, MD_SIZE new_asize)
{
    buf->data = realloc(buf->data, new_asize);
    if(buf->data == NULL) {
        fprintf(stderr, "membuf_grow: realloc() failed.");
        exit(1);
    }
    buf->asize = new_asize;
}

static void
membuf_append(struct membuffer* buf, const char* data, MD_SIZE size)
{
    if(buf->asize < buf->size + size)
        membuf_grow(buf, (buf->size + size) * 2);
    memcpy(buf->data + buf->size, data, size);
    buf->size += size;
}

#define MEMBUF_APPEND_LITERAL(buf, literal)    membuf_append((buf), (literal), strlen(literal))

#define HTML_NEED_ESCAPE(ch)        ((ch) == '&' || (ch) == '<' || (ch) == '>' || (ch) == '"')

static void
membuf_append_escaped(struct membuffer* buf, const char* data, MD_SIZE size)
{
    MD_OFFSET beg = 0;
    MD_OFFSET off = 0;

    /* Some characters need to be escaped in normal HTML text. */

    while(1) {
        while(off < size  &&  !HTML_NEED_ESCAPE(data[off]))
            off++;
        if(off > beg)
            membuf_append(buf, data + beg, off - beg);

        if(off < size) {
            switch(data[off]) {
                case '&':   MEMBUF_APPEND_LITERAL(buf, "&amp;"); break;
                case '<':   MEMBUF_APPEND_LITERAL(buf, "&lt;"); break;
                case '>':   MEMBUF_APPEND_LITERAL(buf, "&gt;"); break;
                case '"':   MEMBUF_APPEND_LITERAL(buf, "&quot;"); break;
            }
            off++;
        } else {
            break;
        }
        beg = off;
    }
}

/**************************************
 ***  HTML renderer implementation  ***
 **************************************/

static void
open_code_block(struct membuffer* out, const MD_BLOCK_CODE_DETAIL* det)
{
    MEMBUF_APPEND_LITERAL(out, "<pre><code");

    /* If known, output the HTML 5 attribute class="language-LANGNAME". */
    if(det->lang != NULL) {
        MEMBUF_APPEND_LITERAL(out, " class=\"language-");
        membuf_append_escaped(out, det->lang, det->lang_size);
        MEMBUF_APPEND_LITERAL(out, "\"");
    }

    MEMBUF_APPEND_LITERAL(out, ">");
}

static int
enter_block_callback(MD_BLOCKTYPE type, void* detail, void* userdata)
{
    static const char* head[6] = { "<h1>", "<h2>", "<h3>", "<h4>", "<h5>", "<h6>" };
    struct membuffer* out = (struct membuffer*) userdata;

    switch(type) {
        case MD_BLOCK_DOC:      /* noop */ break;
        case MD_BLOCK_HR:       MEMBUF_APPEND_LITERAL(out, "<hr>\n"); break;
        case MD_BLOCK_H:        MEMBUF_APPEND_LITERAL(out, head[((MD_BLOCK_H_DETAIL*)detail)->level - 1]); break;
        case MD_BLOCK_CODE:     open_code_block(out, (const MD_BLOCK_CODE_DETAIL*) detail); break;
        case MD_BLOCK_P:        MEMBUF_APPEND_LITERAL(out, "<p>"); break;
    }

    return 0;
}

static int
leave_block_callback(MD_BLOCKTYPE type, void* detail, void* userdata)
{
    static const char* head[6] = { "</h1>\n", "</h2>\n", "</h3>\n", "</h4>\n", "</h5>\n", "</h6>\n" };
    struct membuffer* out = (struct membuffer*) userdata;

    switch(type) {
        case MD_BLOCK_DOC:      /*noop*/ break;
        case MD_BLOCK_HR:       /*noop*/ break;
        case MD_BLOCK_H:        MEMBUF_APPEND_LITERAL(out, head[((MD_BLOCK_H_DETAIL*)detail)->level - 1]); break;
        case MD_BLOCK_CODE:     MEMBUF_APPEND_LITERAL(out, "</code></pre>\n"); break;
        case MD_BLOCK_P:        MEMBUF_APPEND_LITERAL(out, "</p>\n"); break;
    }

    return 0;
}

static int
enter_span_callback(MD_SPANTYPE type, void* detail, void* userdata)
{
    return 0;
}

static int
leave_span_callback(MD_SPANTYPE type, void* detail, void* userdata)
{
    return 0;
}

static int
text_callback(MD_TEXTTYPE type, const MD_CHAR* text, MD_SIZE size, void* userdata)
{
    struct membuffer* out = (struct membuffer*) userdata;

    switch(type) {
    default:                membuf_append_escaped(out, text, size); break;
    }

    return 0;
}

static void
debug_log_callback(const char* msg, void* userdata)
{
    fprintf(stderr, "Error:%s\n", msg);
}


/**********************
 ***  Main program  ***
 **********************/

static int
process_file(FILE* in, FILE* out, unsigned flags, int fullhtml, int print_stat)
{
    MD_RENDERER renderer = {
        enter_block_callback,
        leave_block_callback,
        enter_span_callback,
        leave_span_callback,
        text_callback,
        debug_log_callback,
        flags
    };

    MD_SIZE n;
    struct membuffer buf_in = {0};
    struct membuffer buf_out = {0};
    int ret = -1;
    clock_t t0, t1;

    membuf_init(&buf_in, 32 * 1024);

    /* Read the input file into a buffer. */
    while(1) {
        if(buf_in.size >= buf_in.asize)
            membuf_grow(&buf_in, 2 * buf_in.asize);

        n = fread(buf_in.data + buf_in.size, 1, buf_in.asize - buf_in.size, in);
        if(n == 0)
            break;
        buf_in.size += n;
    }

    /* Input size is good estimation of output size. Add some more reserve to
     * deal with the HTML header/footer and tags. */
    membuf_init(&buf_out, buf_in.size + buf_in.size/8 + 64);

    /* Parse the document. This shall call our callbacks provided via the
     * md_renderer_t structure. */
    t0 = clock();
    ret = md_parse(buf_in.data, buf_in.size, &renderer, (void*) &buf_out);
    t1 = clock();
    if(ret != 0) {
        fprintf(stderr, "Parsing failed.\n");
        goto out;
    }

    /* Write down the document in the HTML format. */
    if(fullhtml) {
        fprintf(out, "<html>\n");
        fprintf(out, "<head>\n");
        fprintf(out, "<title></title>\n");
        fprintf(out, "<meta name=\"generator\" content=\"md2html\">\n");
        fprintf(out, "</head>\n");
        fprintf(out, "<body>\n");
    }

    fwrite(buf_out.data, 1, buf_out.size, out);

    if(fullhtml) {
        fprintf(out, "</body>\n");
        fprintf(out, "</html>\n");
    }

    if(print_stat) {
        if(t0 != (clock_t)-1  &&  t1 != (clock_t)-1) {
            double elapsed = (double)(t1 - t0) / CLOCKS_PER_SEC;
            if (elapsed < 1)
                fprintf(stderr, "Time spent on parsing: %7.2f ms.\n", elapsed*1e3);
            else
                fprintf(stderr, "Time spent on parsing: %6.3f s.\n", elapsed);
        }
    }

    /* Success if we have reached here. */
    ret = 0;

out:
    membuf_fini(&buf_in);
    membuf_fini(&buf_out);

    return ret;
}


#define OPTION_ARG_NONE         0
#define OPTION_ARG_REQUIRED     1
#define OPTION_ARG_OPTIONAL     2

static const option cmdline_options[] = {
    { "output",                     'o', 'o', OPTION_ARG_REQUIRED },
    { "full-html",                  'f', 'f', OPTION_ARG_NONE },
    { "stat",                       's', 's', OPTION_ARG_NONE },
    { "help",                       'h', 'h', OPTION_ARG_NONE },
    { "fpermissive-atx-headers",     0,  'A', OPTION_ARG_NONE },
    { "fno-indented-code",           0,  'I', OPTION_ARG_NONE },
    { 0 }
};

static void
usage(void)
{
    printf(
        "Usage: md2html [OPTION]... [FILE]\n"
        "Convert input FILE (or standard input) in Markdown format to HTML.\n"
        "\n"
        "General options:\n"
        "  -o  --output=FILE        output file (default is standard output)\n"
        "  -f, --full-html          generate full HTML document, including header\n"
        "  -s, --stat               measure time of input parsing\n"
        "  -h, --help               display this help and exit\n"
        "\n"
        "Markdown dialect options:\n"
        "      --fpermissive-atx-headers    allow ATX headers without delimiting space\n"
        "      --fno-indented-code          disabled indented code blocks\n"
    );
}

static const char* input_path = NULL;
static const char* output_path = NULL;
static unsigned renderer_flags = 0;
static int want_fullhtml = 0;
static int want_stat = 0;

static int
cmdline_callback(int opt, char const* value, void* data)
{
    switch(opt) {
        case 0:
            if(input_path) {
                fprintf(stderr, "Too many arguments. Only one input file can be specified.\n");
                fprintf(stderr, "Use --help for more info.\n");
                exit(1);
            }
            input_path = value;
            break;

        case 'o':   output_path = value; break;
        case 'f':   want_fullhtml = 1; break;
        case 's':   want_stat = 1; break;
        case 'h':   usage(); exit(0); break;

        case 'A':   renderer_flags |= MD_FLAG_PERMISSIVEATXHEADERS; break;
        case 'I':   renderer_flags |= MD_FLAG_NOINDENTEDCODE; break;

        default:
            fprintf(stderr, "Illegal option: %s\n", value);
            fprintf(stderr, "Use --help for more info.\n");
            exit(1);
            break;
    }

    return 0;
}

int
main(int argc, char** argv)
{
    FILE* in = stdin;
    FILE* out = stdout;
    int ret = 0;

    if(readoptions(cmdline_options, argc, argv, cmdline_callback, NULL) < 0) {
        usage();
        exit(1);
    }

    if(input_path != NULL && strcmp(input_path, "-") != 0) {
        in = fopen(input_path, "rb");
        if(in == NULL) {
            fprintf(stderr, "Cannot open %s.\n", input_path);
            exit(1);
        }
    }
    if(output_path != NULL && strcmp(output_path, "-") != 0) {
        out = fopen(output_path, "wt");
        if(out == NULL) {
            fprintf(stderr, "Cannot open %s.\n", input_path);
            exit(1);
        }
    }

    ret = process_file(in, out, renderer_flags, want_fullhtml, want_stat);
    if(in != stdin)
        fclose(in);
    if(out != stdout)
        fclose(out);

    return ret;
}
