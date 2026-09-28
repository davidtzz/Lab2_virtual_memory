#include <errno.h>
#include <stdlib.h>
#include "parse.h"

int parse_u64(const char *text, uint64_t *out)
{
    char *end;
    if (text[0] == '-' || text[0] == '\0')
        return -1;
    errno = 0;
    unsigned long long value = strtoull(text, &end, 0);
    if (errno != 0 || *end != '\0')
        return -1;
    *out = (uint64_t)value;
    return 0;
}