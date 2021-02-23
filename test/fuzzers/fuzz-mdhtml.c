#include <stdint.h>
#include <stdlib.h>
#include "md4c-html.h"

static void
process_output(const MD_CHAR* text, MD_SIZE size, void* userdata)
{
   /* This is  dummy function because we dont need any processing on the data */
   return;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size){
    if (size < 8) {
        return 0;
    }

    unsigned int parser_flags = *(unsigned int*)data;
    data += 4; size -= 4;
    unsigned int renderer_flags = *(unsigned int*)data;
    data += 4; size -= 4;

    /* Allocate enough space */
    char *out = malloc(size*3);
    md_html(data, size, process_output, out, parser_flags, renderer_flags);
    free(out);

    return 0;
}
