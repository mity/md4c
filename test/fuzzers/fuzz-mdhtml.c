
#include <stdint.h>
#include <stdlib.h>
#include "md4c-html.h"


static void
process_output(const MD_CHAR* text, MD_SIZE size, void* userdata)
{
   /* This is a dummy function because we don't need to generate any output
    * actually. */
   return;
}

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    unsigned parser_flags, renderer_flags;

    if(size < 2 * sizeof(unsigned)) {
        /* We interpret the 1st 8 bytes as parser flags and renderer flags. */
        return 0;
    }

    parser_flags = *(unsigned*)data;
    data += sizeof(unsigned); size -= sizeof(unsigned);

    renderer_flags = *(unsigned*)data;
    data += sizeof(unsigned); size -= sizeof(unsigned);

    /* Allocate enough space */
    md_html(data, size, process_output, NULL, parser_flags, renderer_flags);

    return 0;
}
