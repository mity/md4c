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
#include "entity.h"


/* Global options. */
static unsigned renderer_flags = 0;
static int want_fullhtml = 0;
static int want_stat = 0;
static int want_verbatim_entities = 0;


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

#define ISDIGIT(ch)     ('0' <= (ch) && (ch) <= '9')
#define ISLOWER(ch)     ('a' <= (ch) && (ch) <= 'z')
#define ISUPPER(ch)     ('A' <= (ch) && (ch) <= 'Z')
#define ISALNUM(ch)     (ISLOWER(ch) || ISUPPER(ch) || ISDIGIT(ch))

static void
membuf_append_escaped(struct membuffer* buf, const char* data, MD_SIZE size)
{
    MD_OFFSET beg = 0;
    MD_OFFSET off = 0;

    /* Some characters need to be escaped in normal HTML text. */
    #define HTML_NEED_ESCAPE(ch)                                            \
            ((ch) == '&' || (ch) == '<' || (ch) == '>' || (ch) == '"')

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

static void
membuf_append_url_escaped(struct membuffer* buf, const char* data, MD_SIZE size)
{
    static const uint8_t hex_chars[] = "0123456789ABCDEF";
    MD_OFFSET beg = 0;
    MD_OFFSET off = 0;

    #define URL_NEED_ESCAPE(ch)                                             \
            (!ISALNUM(ch)  &&  strchr("-_.+!*'(),%#@?=;:/,+&$", ch) == NULL)

    while(1) {
        while(off < size  &&  !URL_NEED_ESCAPE(data[off]))
            off++;
        if(off > beg)
            membuf_append(buf, data + beg, off - beg);

        if(off < size) {
            char hex[3];

            switch(data[off]) {
                case '&':   MEMBUF_APPEND_LITERAL(buf, "&amp;"); break;
                case '\'':  MEMBUF_APPEND_LITERAL(buf, "&#x27;"); break;
                default:
                    hex[0] = '%';
                    hex[1] = hex_chars[((unsigned)data[off] >> 4) & 0xf];
                    hex[2] = hex_chars[((unsigned)data[off] >> 0) & 0xf];
                    membuf_append(buf, hex, 3);
                    break;
            }
            off++;
        } else {
            break;
        }

        beg = off;
    }
}


/*****************************************
 ***  HTML rendering helper functions  ***
 *****************************************/

static void
open_ol_block(struct membuffer* out, const MD_BLOCK_OL_DETAIL* det)
{
    char buf[64];

    if(det->start == 1) {
        MEMBUF_APPEND_LITERAL(out, "<ol>");
        return;
    }

    snprintf(buf, sizeof(buf), "<ol start=\"%u\">", det->start);
    MEMBUF_APPEND_LITERAL(out, buf);
}

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

static void
open_td_block(struct membuffer* out, const char* cell_type, const MD_BLOCK_TD_DETAIL* det)
{
    MEMBUF_APPEND_LITERAL(out, "<");
    MEMBUF_APPEND_LITERAL(out, cell_type);

    switch(det->align) {
        case MD_ALIGN_LEFT:     MEMBUF_APPEND_LITERAL(out, " align=\"left\">"); break;
        case MD_ALIGN_CENTER:   MEMBUF_APPEND_LITERAL(out, " align=\"center\">"); break;
        case MD_ALIGN_RIGHT:    MEMBUF_APPEND_LITERAL(out, " align=\"right\">"); break;
        default:                MEMBUF_APPEND_LITERAL(out, ">"); break;
    }
}

static void
open_a_span(struct membuffer* out, const MD_SPAN_A_DETAIL* det)
{
    MEMBUF_APPEND_LITERAL(out, "<a href=\"");
    membuf_append_url_escaped(out, det->href, det->href_size);

    if(det->title != NULL) {
        MEMBUF_APPEND_LITERAL(out, "\" title=\"");
        membuf_append_escaped(out, det->title, det->title_size);
    }

    MEMBUF_APPEND_LITERAL(out, "\">");
}

static void
open_img_span(struct membuffer* out, const MD_SPAN_IMG_DETAIL* det)
{
    MEMBUF_APPEND_LITERAL(out, "<img src=\"");
    membuf_append_url_escaped(out, det->src, det->src_size);

    MEMBUF_APPEND_LITERAL(out, "\" alt=\"");
    membuf_append_escaped(out, det->alt, det->alt_size);

    if(det->title != NULL) {
        MEMBUF_APPEND_LITERAL(out, "\" title=\"");
        membuf_append_escaped(out, det->title, det->title_size);
    }

    MEMBUF_APPEND_LITERAL(out, "\">");
}

static unsigned
hex_val(char ch)
{
    if('0' <= ch && ch <= '9')
        return ch - '0';
    if('A' <= ch && ch <= 'Z')
        return ch - 'A' + 10;
    else
        return ch - 'a' + 10;
}

static void
render_utf8_codepoint(struct membuffer* out, unsigned codepoint)
{
    static const char utf8_replacement_char[] = { 0xef, 0xbf, 0xbd };

    unsigned char utf8[4];
    size_t n;

    if(codepoint <= 0x7f) {
        n = 1;
        utf8[0] = codepoint;
    } else if(codepoint <= 0x7ff) {
        n = 2;
        utf8[0] = 0xc0 | ((codepoint >>  6) & 0x1f);
        utf8[1] = 0x80 + ((codepoint >>  0) & 0x3f);
    } else if(codepoint <= 0xffff) {
        n = 3;
        utf8[0] = 0xe0 | ((codepoint >> 12) & 0xf);
        utf8[1] = 0x80 + ((codepoint >>  6) & 0x3f);
        utf8[2] = 0x80 + ((codepoint >>  0) & 0x3f);
    } else {
        n = 4;
        utf8[0] = 0xf0 | ((codepoint >> 18) & 0x7);
        utf8[1] = 0x80 + ((codepoint >> 12) & 0x3f);
        utf8[2] = 0x80 + ((codepoint >>  6) & 0x3f);
        utf8[3] = 0x80 + ((codepoint >>  0) & 0x3f);
    }

    if(0 < codepoint  &&  codepoint <= 0x10ffff)
        membuf_append_escaped(out, (char*)utf8, n);
    else
        membuf_append(out, utf8_replacement_char, 3);
}

/* Translate entity to its UTF-8 equivalent, or output the verbatim one
 * if such entity is unknown (or if the translation is disabled). */
static void
render_entity(struct membuffer* out, const MD_CHAR* text, MD_SIZE size)
{
    if(want_verbatim_entities) {
        membuf_append(out, text, size);
        return;
    }

    /* We assume UTF-8 output is what is desired. */
    if(size > 3 && text[1] == '#') {
        unsigned codepoint = 0;

        if(text[2] == 'x' || text[2] == 'X') {
            /* Hexadecimal entity (e.g. "&#x1234abcd;")). */
            int i;
            for(i = 3; i < size-1; i++)
                codepoint = 16 * codepoint + hex_val(text[i]);
        } else {
            /* Decimal entity (e.g. "&1234;") */
            int i;
            for(i = 2; i < size-1; i++)
                codepoint = 10 * codepoint + (text[i] - '0');
        }

        render_utf8_codepoint(out, codepoint);
        return;
    } else {
        /* Named entity (e.g. "&nbsp;". */
        const char* ent;

        ent = entity_lookup(text, size);
        if(ent != NULL) {
            membuf_append_escaped(out, ent, strlen(ent));
            return;
        }
    }

    membuf_append_escaped(out, text, size);
}


/**************************************
 ***  HTML renderer implementation  ***
 **************************************/

static int
enter_block_callback(MD_BLOCKTYPE type, void* detail, void* userdata)
{
    static const char* head[6] = { "<h1>", "<h2>", "<h3>", "<h4>", "<h5>", "<h6>" };
    struct membuffer* out = (struct membuffer*) userdata;

    switch(type) {
        case MD_BLOCK_DOC:      /* noop */ break;
        case MD_BLOCK_QUOTE:    MEMBUF_APPEND_LITERAL(out, "<blockquote>\n"); break;
        case MD_BLOCK_UL:       MEMBUF_APPEND_LITERAL(out, "<ul>\n"); break;
        case MD_BLOCK_OL:       open_ol_block(out, (const MD_BLOCK_OL_DETAIL*)detail); break;
        case MD_BLOCK_LI:       MEMBUF_APPEND_LITERAL(out, "<li>"); break;
        case MD_BLOCK_HR:       MEMBUF_APPEND_LITERAL(out, "<hr>\n"); break;
        case MD_BLOCK_H:        MEMBUF_APPEND_LITERAL(out, head[((MD_BLOCK_H_DETAIL*)detail)->level - 1]); break;
        case MD_BLOCK_CODE:     open_code_block(out, (const MD_BLOCK_CODE_DETAIL*) detail); break;
        case MD_BLOCK_HTML:     /* noop */ break;
        case MD_BLOCK_P:        MEMBUF_APPEND_LITERAL(out, "<p>"); break;
        case MD_BLOCK_TABLE:    MEMBUF_APPEND_LITERAL(out, "<table>\n"); break;
        case MD_BLOCK_THEAD:    MEMBUF_APPEND_LITERAL(out, "<thead>\n"); break;
        case MD_BLOCK_TBODY:    MEMBUF_APPEND_LITERAL(out, "<tbody>\n"); break;
        case MD_BLOCK_TR:       MEMBUF_APPEND_LITERAL(out, "<tr>\n"); break;
        case MD_BLOCK_TH:       open_td_block(out, "th", (MD_BLOCK_TD_DETAIL*)detail); break;
        case MD_BLOCK_TD:       open_td_block(out, "td", (MD_BLOCK_TD_DETAIL*)detail); break;
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
        case MD_BLOCK_QUOTE:    MEMBUF_APPEND_LITERAL(out, "</blockquote>\n"); break;
        case MD_BLOCK_UL:       MEMBUF_APPEND_LITERAL(out, "</ul>\n"); break;
        case MD_BLOCK_OL:       MEMBUF_APPEND_LITERAL(out, "</ol>\n"); break;
        case MD_BLOCK_LI:       MEMBUF_APPEND_LITERAL(out, "</li>\n"); break;
        case MD_BLOCK_HR:       /*noop*/ break;
        case MD_BLOCK_H:        MEMBUF_APPEND_LITERAL(out, head[((MD_BLOCK_H_DETAIL*)detail)->level - 1]); break;
        case MD_BLOCK_CODE:     MEMBUF_APPEND_LITERAL(out, "</code></pre>\n"); break;
        case MD_BLOCK_HTML:     /* noop */ break;
        case MD_BLOCK_P:        MEMBUF_APPEND_LITERAL(out, "</p>\n"); break;
        case MD_BLOCK_TABLE:    MEMBUF_APPEND_LITERAL(out, "</table>\n"); break;
        case MD_BLOCK_THEAD:    MEMBUF_APPEND_LITERAL(out, "</thead>\n"); break;
        case MD_BLOCK_TBODY:    MEMBUF_APPEND_LITERAL(out, "</tbody>\n"); break;
        case MD_BLOCK_TR:       MEMBUF_APPEND_LITERAL(out, "</tr>\n"); break;
        case MD_BLOCK_TH:       MEMBUF_APPEND_LITERAL(out, "</th>\n"); break;
        case MD_BLOCK_TD:       MEMBUF_APPEND_LITERAL(out, "</td>\n"); break;
    }

    return 0;
}

static int
enter_span_callback(MD_SPANTYPE type, void* detail, void* userdata)
{
    struct membuffer* out = (struct membuffer*) userdata;

    switch(type) {
        case MD_SPAN_EM:        MEMBUF_APPEND_LITERAL(out, "<em>"); break;
        case MD_SPAN_STRONG:    MEMBUF_APPEND_LITERAL(out, "<strong>"); break;
        case MD_SPAN_A:         open_a_span(out, (MD_SPAN_A_DETAIL*) detail); break;
        case MD_SPAN_IMG:       open_img_span(out, (MD_SPAN_IMG_DETAIL*) detail); break;
        case MD_SPAN_CODE:      MEMBUF_APPEND_LITERAL(out, "<code>"); break;
    }

    return 0;
}

static int
leave_span_callback(MD_SPANTYPE type, void* detail, void* userdata)
{
    struct membuffer* out = (struct membuffer*) userdata;

    switch(type) {
        case MD_SPAN_EM:        MEMBUF_APPEND_LITERAL(out, "</em>"); break;
        case MD_SPAN_STRONG:    MEMBUF_APPEND_LITERAL(out, "</strong>"); break;
        case MD_SPAN_A:         MEMBUF_APPEND_LITERAL(out, "</a>"); break;
        case MD_SPAN_IMG:       /* noop */ break;
        case MD_SPAN_CODE:      MEMBUF_APPEND_LITERAL(out, "</code>"); break;
    }

    return 0;
}

static int
text_callback(MD_TEXTTYPE type, const MD_CHAR* text, MD_SIZE size, void* userdata)
{
    struct membuffer* out = (struct membuffer*) userdata;

    switch(type) {
        case MD_TEXT_NULLCHAR:  render_utf8_codepoint(out, 0x0000); break;
        case MD_TEXT_BR:        MEMBUF_APPEND_LITERAL(out, "<br>\n"); break;
        case MD_TEXT_SOFTBR:    MEMBUF_APPEND_LITERAL(out, "\n"); break;
        case MD_TEXT_HTML:      membuf_append(out, text, size); break;
        case MD_TEXT_ENTITY:    render_entity(out, text, size); break;
        default:                membuf_append_escaped(out, text, size); break;
    }

    return 0;
}

static void
debug_log_callback(const char* msg, void* userdata)
{
    fprintf(stderr, "Error: %s\n", msg);
}


/**********************
 ***  Main program  ***
 **********************/

static int
process_file(FILE* in, FILE* out)
{
    MD_RENDERER renderer = {
        enter_block_callback,
        leave_block_callback,
        enter_span_callback,
        leave_span_callback,
        text_callback,
        debug_log_callback,
        renderer_flags
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
    if(want_fullhtml) {
        fprintf(out, "<html>\n");
        fprintf(out, "<head>\n");
        fprintf(out, "<title></title>\n");
        fprintf(out, "<meta name=\"generator\" content=\"md2html\">\n");
        fprintf(out, "</head>\n");
        fprintf(out, "<body>\n");
    }

    fwrite(buf_out.data, 1, buf_out.size, out);

    if(want_fullhtml) {
        fprintf(out, "</body>\n");
        fprintf(out, "</html>\n");
    }

    if(want_stat) {
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
    { "fverbatim-entities",          0,  'E', OPTION_ARG_NONE },
    { "fpermissive-atx-headers",     0,  'A', OPTION_ARG_NONE },
    { "fpermissive-url-autolinks",   0,  'U', OPTION_ARG_NONE },
    { "fpermissive-email-autolinks", 0,  '@', OPTION_ARG_NONE },
    { "fpermissive-autolinks",       0,  'V', OPTION_ARG_NONE },
    { "fno-indented-code",           0,  'I', OPTION_ARG_NONE },
    { "fno-html-blocks",             0,  'F', OPTION_ARG_NONE },
    { "fno-html-spans",              0,  'G', OPTION_ARG_NONE },
    { "fno-html",                    0,  'H', OPTION_ARG_NONE },
    { "fcollapse-whitespace",        0,  'W', OPTION_ARG_NONE },
    { "ftables",                     0,  'T', OPTION_ARG_NONE },
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
        "  -o  --output=FILE    Output file (default is standard output)\n"
        "  -f, --full-html      Generate full HTML document, including header\n"
        "  -s, --stat           Measure time of input parsing\n"
        "  -h, --help           Display this help and exit\n"
        "\n"
        "Markdown extension options:\n"
        "      --fcollapse-whitespace\n"
        "                       Collapse non-trivial whitespace\n"
        "      --fverbatim-entities\n"
        "                       Do not translate entities\n"
        "      --fpermissive-atx-headers\n"
        "                       Allow ATX headers without delimiting space\n"
        "      --fpermissive-url-autolinks\n"
        "                       Allow URL autolinks without '<', '>'\n"
        "      --fpermissive-email-autolinks  \n"
        "                       Allow e-mail autolinks without '<', '>' and 'mailto:'\n"
        "      --fpermissive-autolinks\n"
        "                       Same as --fpermissive-url-autolinks --fpermissive-email-autolinks\n"
        "      --fno-indented-code\n"
        "                       Disable indented code blocks\n"
        "      --fno-html-blocks\n"
        "                       Disable raw HTML blocks\n"
        "      --fno-html-spans\n"
        "                       Disable raw HTML spans\n"
        "      --fno-html       Same as --fno-html-blocks --fno-html-spans\n"
        "      --ftables        Enable tables\n"
    );
}

static const char* input_path = NULL;
static const char* output_path = NULL;

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

        case 'E':   want_verbatim_entities = 1; break;
        case 'A':   renderer_flags |= MD_FLAG_PERMISSIVEATXHEADERS; break;
        case 'I':   renderer_flags |= MD_FLAG_NOINDENTEDCODEBLOCKS; break;
        case 'F':   renderer_flags |= MD_FLAG_NOHTMLBLOCKS; break;
        case 'G':   renderer_flags |= MD_FLAG_NOHTMLSPANS; break;
        case 'H':   renderer_flags |= MD_FLAG_NOHTML; break;
        case 'W':   renderer_flags |= MD_FLAG_COLLAPSEWHITESPACE; break;
        case 'U':   renderer_flags |= MD_FLAG_PERMISSIVEURLAUTOLINKS; break;
        case '@':   renderer_flags |= MD_FLAG_PERMISSIVEEMAILAUTOLINKS; break;
        case 'V':   renderer_flags |= MD_FLAG_PERMISSIVEAUTOLINKS; break;
        case 'T':   renderer_flags |= MD_FLAG_TABLES; break;

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

    ret = process_file(in, out);
    if(in != stdin)
        fclose(in);
    if(out != stdout)
        fclose(out);

    return ret;
}
