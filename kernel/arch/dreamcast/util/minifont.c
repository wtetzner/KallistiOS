/* KallistiOS ##version##

   util/minifont.c
   Copyright (C) 2020 Lawrence Sebald

*/

#include <string.h>
#include "minifont.h"

#define CHAR_WIDTH 8
#define CHAR_HEIGHT 16

#define BYTES_PER_CHAR ((CHAR_WIDTH / 8) * CHAR_HEIGHT)

int minifont_draw(uint16 *buffer, uint32 bufwidth, uint32 c) {
    int pos, i, j, k;
    uint8 byte;
    uint16 *cur;

    if(c < 33 || c > 126)
        return (CHAR_WIDTH / 8);

    pos = (c - 33) * BYTES_PER_CHAR;

    for(i = 0; i < CHAR_HEIGHT; ++i) {
        cur = buffer;

        for(j = 0; j < CHAR_WIDTH; ++j) {
            byte = minifont_data[pos + j];

            for(k = 0; k < 8; ++k) {
                if(byte & (1 << (7 - k)))
                    *cur++ = 0xFFFF;
                else
                    ++cur;
            }
        }

        buffer += bufwidth;
    }

    return (CHAR_WIDTH / 8);
}

int minifont_draw_str(uint16 *buffer, uint32 bufwidth, const char *str) {
    char c;
    int adv = 0;

    while((c = *str++)) {
        adv += minifont_draw(buffer + adv, bufwidth, c);
    }

    return adv;
}
