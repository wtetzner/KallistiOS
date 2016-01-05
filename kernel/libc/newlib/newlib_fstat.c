/* KallistiOS ##version##

   newlib_fstat.c
   Copyright (C) 2004 Dan Potter
   Copyright (C) 2016 Lawrence Sebald

*/

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include <kos/fs.h>

int _fstat_r(struct _reent *reent, int fd, struct stat *pstat) {
    int err = errno, rv;

    (void)reent;

    /* Try to use the native stat function first... */
    if(!(rv = fs_fstat(fd, pstat)) || errno != ENOSYS)
        return rv;

    /* If this filesystem doesn't implement fstat, do what we always used to do
       in the past (which isn't very useful)... */
    errno = err;
    memset(pstat, 0, sizeof(struct stat));
    pstat->st_mode = S_IFCHR;

    return 0;
}
