/* KallistiOS ##version##

   seekdir.c
   Copyright (C)2004 Megan Potter

*/

#include <kos/dbglog.h>
#include <sys/dirent.h>

void seekdir(DIR *dir __attribute__((unused)), off_t offset __attribute__((unused))) {
    dbglog(DBG_WARNING, "seekdir: call ignored\n");
}
