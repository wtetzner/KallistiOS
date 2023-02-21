/* KallistiOS ##version##

   newlib_getentropy.c
   Copyright (C) 2023 Luke Benstead

*/

#include <errno.h>
#include <unistd.h>
#include <sys/time.h>

int getentropy(void *ptr, size_t len) {
    /* We read backwards from the end of available memory
    and XOR in blocks into the key array, while XORing with the current time.
    If anyone has a better idea for generating entropy then send a patch :) */

    struct timeval tv;
    gettimeofday(&tv, NULL);

    const int block_size = 128;

    uint8_t* src = ((uint8_t*) _arch_mem_top);
    uint8_t* dst = ptr;
    for(size_t i = 0; i < len; ++i) {
        uint8_t b = tv.tv_usec % 255;

        for(int j = 0; j < block_size; ++j) {
            b ^= *--src;
        }

        *dst = b;
        ++dst;
    }
    return 0;
}
