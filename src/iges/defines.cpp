#include "defines.h"

#include <cstdio>

void message_to_file(const char* text)
{
    if (text) {
        std::fprintf(stderr, "%s\n", text);
    }
}
