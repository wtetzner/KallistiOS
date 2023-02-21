/* KallistiOS ##version##

   fs_random.c
   Copyright (C) 2023 Luke Benstead
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <malloc.h>
#include <errno.h>
#include <time.h>
#include <arch/types.h>
#include <kos/mutex.h>
#include <dc/fs_random.h>
#include <sys/queue.h>
#include <errno.h>
#include <sys/time.h>

/* This function is declared in <stdlib.h> but behind an if __BSD_VISIBLE
   Declaring as extern here to avoid implicit declaration */
void    arc4random_buf (void *, size_t);

/* File handles */
typedef struct random_fh_str {
    int mode;                           /* mode the file was opened with */

    TAILQ_ENTRY(random_fh_str) listent;    /* list entry */
} random_fh_t;

/* Linked list of open files (controlled by "mutex") */
TAILQ_HEAD(random_fh_list, random_fh_str) random_fh;

/* Thread mutex for random_fh access */
static mutex_t fh_mutex;


/* openfile function */
static random_fh_t *random_open_file(vfs_handler_t * vfs, const char *fn, int mode) {
    (void) vfs;
    (void) fn;

    random_fh_t    * fd;       /* file descriptor */
    int     realmode;

    /* Malloc a new fh struct */
    fd = malloc(sizeof(random_fh_t));

    /* Fill in the filehandle struct */
    fd->mode = mode;

    realmode = mode & O_MODE_MASK;

    /* We only allow reading, not writing */
    if(realmode != O_RDONLY) {
        free(fd);
        return NULL;
    }

    return fd;
}

/* open function */
static void * random_open(vfs_handler_t * vfs, const char *path, int mode) {
    random_fh_t *fh = random_open_file(vfs, path, mode);

    /* link the fh onto the top of the list */
    mutex_lock(&fh_mutex);
    TAILQ_INSERT_TAIL(&random_fh, fh, listent);
    mutex_unlock(&fh_mutex);

    return (void *)fh;
}

/* Verify that a given hnd is actually in the list */
static int random_verify_hnd(void * hnd) {
    random_fh_t    *cur;
    int     rv;

    rv = 0;

    mutex_lock(&fh_mutex);
    TAILQ_FOREACH(cur, &random_fh, listent) {
        if((void *)cur == hnd) {
            rv = 1;
            break;
        }
    }
    mutex_unlock(&fh_mutex);

    if(rv)
        return 1;
    else
        return 0;
}

/* close a file */
static int random_close(void * hnd) {
    random_fh_t *fh;
    int retval = 0;

    /* Check the handle */
    if(!random_verify_hnd(hnd)) {
        errno = EBADF;
        return -1;
    }

    fh = (random_fh_t *)hnd;

    /* Look for the one to get rid of */
    mutex_lock(&fh_mutex);
    TAILQ_REMOVE(&random_fh, fh, listent);
    mutex_unlock(&fh_mutex);

    free(fh);
    return retval;
}

/* read function */
static ssize_t random_read(void * hnd, void *buffer, size_t cnt) {
    random_fh_t *fh;
    uint8_t* buf = buffer;

    /* Check the handle */
    if(!random_verify_hnd(hnd))
        return -1;

    fh = (random_fh_t *)hnd;

    /* make sure we're opened for reading */
    if((fh->mode & O_MODE_MASK) != O_RDONLY && (fh->mode & O_MODE_MASK) != O_RDWR)
        return 0;

    arc4random_buf(buf, cnt);

    return cnt;
}

/* write function */
static ssize_t random_write(void * hnd, const void *buffer, size_t cnt) {
    (void) buffer;
    (void) cnt;

    random_fh_t    *fh;

    /* Check the handle we were given */
    if(!random_verify_hnd(hnd))
        return -1;

    fh = (random_fh_t *)hnd;

    /* Make sure we're opened for writing */
    if((fh->mode & O_MODE_MASK) != O_WRONLY && (fh->mode & O_MODE_MASK) != O_RDWR)
        return -1;

    dbglog(DBG_ERROR, "RANDOMFS: writing entropy is not supported\n");
    return -1;
}


/* Seek elsewhere in a file */
static off_t random_seek(void * hnd, off_t offset, int whence) {
    (void) offset;
    (void) whence;

    /* Check the handle */
    if(!random_verify_hnd(hnd))
        return -1;

    return 0;
}

/* tell the current position in the file */
static off_t random_tell(void * hnd) {
    /* Check the handle */
    if(!random_verify_hnd(hnd))
        return -1;

    return 0;
}

/* return the filesize */
static size_t random_total(void * fd) {
    /* Check the handle */
    if(!random_verify_hnd(fd))
        return -1;

    /* The size of /dev/urandom always returns 0 */
    return 0;
}


/* Delete a file */
static int random_unlink(vfs_handler_t * vfs, const char *path) {
    (void) vfs;
    (void) path;

    dbglog(DBG_ERROR, "RANDOMFS: Attempted to delete system file\n");
    return -1;
}

static int random_stat(vfs_handler_t *vfs, const char *fn, struct stat *rv,
                    int flag) {
    (void)vfs;
    (void)fn;
    (void)flag;

    memset(rv, 0, sizeof(struct stat));
    rv->st_mode = S_IRUSR;
    rv->st_nlink = 1;

    return 0;
}

static int random_fcntl(void *fd, int cmd, va_list ap) {
    int rv = -1;

    (void)ap;

    /* Check the handle */
    if(!random_verify_hnd(fd)) {
        errno = EBADF;
        return -1;
    }

    switch(cmd) {
        case F_GETFL:
            rv = O_RDONLY;
            break;

        case F_SETFL:
        case F_GETFD:
        case F_SETFD:
            rv = 0;
            break;

        default:
            errno = EINVAL;
    }

    return rv;
}

static int random_fstat(void *fd, struct stat *st) {

    /* Check the handle */
    if(!random_verify_hnd(fd)) {
        errno = EBADF;
        return -1;
    }

    memset(st, 0, sizeof(struct stat));
    st->st_mode = S_IFREG | S_IRUSR;
    st->st_nlink = 1;

    return 0;
}

/* handler interface */
static vfs_handler_t vh = {
    /* Name handler */
    {
        "/dev/urandom", /* name */
        0,              /* tbfi */
        0x00010000,     /* Version 1.0 */
        0,              /* flags */
        NMMGR_TYPE_VFS, /* VFS handler */
        NMMGR_LIST_INIT
    },
    0, NULL,            /* In-kernel, privdata */

    random_open,
    random_close,
    random_read,
    random_write,
    random_seek,
    random_tell,
    random_total,
    NULL,
    NULL,               /* ioctl */
    NULL,               /* rename/move */
    random_unlink,
    NULL,
    NULL,               /* complete */
    random_stat,           /* stat */
    NULL,               /* mkdir */
    NULL,               /* rmdir */
    random_fcntl,
    NULL,               /* poll */
    NULL,               /* link */
    NULL,               /* symlink */
    NULL,               /* seek64 */
    NULL,               /* tell64 */
    NULL,               /* total64 */
    NULL,               /* readlink */
    NULL,
    random_fstat
};

/* Exactly the same, but aliased under /dev/random */
static vfs_handler_t vh2 = {
    /* Name handler */
    {
        "/dev/random", /* name */
        0,              /* tbfi */
        0x00010000,     /* Version 1.0 */
        0,              /* flags */
        NMMGR_TYPE_VFS, /* VFS handler */
        NMMGR_LIST_INIT
    },
    0, NULL,            /* In-kernel, privdata */

    random_open,
    random_close,
    random_read,
    random_write,
    random_seek,
    random_tell,
    random_total,
    NULL,
    NULL,               /* ioctl */
    NULL,               /* rename/move */
    random_unlink,
    NULL,
    NULL,               /* complete */
    random_stat,           /* stat */
    NULL,               /* mkdir */
    NULL,               /* rmdir */
    random_fcntl,
    NULL,               /* poll */
    NULL,               /* link */
    NULL,               /* symlink */
    NULL,               /* seek64 */
    NULL,               /* tell64 */
    NULL,               /* total64 */
    NULL,               /* readlink */
    NULL,
    random_fstat
};

int fs_random_init() {
    TAILQ_INIT(&random_fh);
    mutex_init(&fh_mutex, MUTEX_TYPE_NORMAL);
    nmmgr_handler_add(&vh.nmmgr);
    nmmgr_handler_add(&vh2.nmmgr);
    return 0;
}

int fs_random_shutdown() {
    random_fh_t * c, * n;

    c = TAILQ_FIRST(&random_fh);

    while(c) {
        n = TAILQ_NEXT(c, listent);
        free(c);
        c = n;
    }

    mutex_destroy(&fh_mutex);

    return nmmgr_handler_remove(&vh.nmmgr);
}
