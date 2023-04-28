/* KallistiOS ##version##

   scandir.c
   Copyright (C)2004 Megan Potter

*/

#include <kos/dbglog.h>
#include <errno.h>

int scandir(const char *dir             __attribute__((unused)), 
    struct dirent ***namelist           __attribute__((unused)), 
    int(*filter)(const struct dirent *) __attribute__((unused)), 
    int(*compar)(const struct dirent ** __attribute__((unused)), 
                const struct dirent **) __attribute__((unused))) {
    dbglog(DBG_WARNING, "scandir: call ignored\n");
    errno = ENOSYS;
    return -1;
}
