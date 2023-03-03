/* KallistiOS ##version##

   newlib_exit.c
   Copyright (C)2004 Megan Potter

*/

#include <arch/arch.h>

extern void arch_newlib_exit(int ret_code) __noreturn;

void _exit(int code) {
    arch_newlib_exit(code);
}
