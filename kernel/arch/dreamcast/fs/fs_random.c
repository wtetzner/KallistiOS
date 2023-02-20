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

#define KEYSIZE		128

/* ARC4 random generation lovingly adapted from BSD */

struct arc4_stream {
	uint8_t i;
	uint8_t j;
	uint8_t s[256];
} rs = {
	.i = 0,
	.j = 0,
	.s = {
		  0,   1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  12,  13,  14,  15,
		 16,  17,  18,  19,  20,  21,  22,  23,  24,  25,  26,  27,  28,  29,  30,  31,
		 32,  33,  34,  35,  36,  37,  38,  39,  40,  41,  42,  43,  44,  45,  46,  47,
		 48,  49,  50,  51,  52,  53,  54,  55,  56,  57,  58,  59,  60,  61,  62,  63,
		 64,  65,  66,  67,  68,  69,  70,  71,  72,  73,  74,  75,  76,  77,  78,  79,
		 80,  81,  82,  83,  84,  85,  86,  87,  88,  89,  90,  91,  92,  93,  94,  95,
		 96,  97,  98,  99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111,
		112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127,
		128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143,
		144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159,
		160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175,
		176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191,
		192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207,
		208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223,
		224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239,
		240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255
	}
};

static struct {
	uint8_t	rnd[KEYSIZE];
} rdat;

static int rs_stired;
static int arc4_count;
static volatile int rs_data_available = 0;

static mutex_t arc_mutex = MUTEX_INITIALIZER;

static inline uint8_t arc4_getbyte(void) {
	uint8_t si, sj;

	rs.i = (rs.i + 1);
	si = rs.s[rs.i];
	rs.j = (rs.j + si);
	sj = rs.s[rs.j];
	rs.s[rs.i] = sj;
	rs.s[rs.j] = si;

	return (rs.s[(si + sj) & 0xff]);
}

static void arc4_fetch() {
    /* We read backwards from the end of available memory
    and XOR in blocks into the key array, while XORing with the current time.
    If anyone has a better idea for generating entropy then send a patch :) */

	struct timeval tv;
    gettimeofday(&tv, NULL);

    const int block_size = 128;

    uint8_t* src = ((uint8_t*) _arch_mem_top);
    uint8_t* dst = rdat.rnd;
    for(int i = 0; i < KEYSIZE; ++i) {
        uint8_t b = tv.tv_usec % 255;

        for(int j = 0; j < block_size; ++j) {
            b ^= *--src;
        }

        *dst = b;
        ++dst;
    }
}

static inline void arc4_addrandom(u_char *dat, int datlen)
{
	int     n;
	uint8_t si;

	rs.i--;
	for (n = 0; n < 256; n++) {
		rs.i = (rs.i + 1);
		si = rs.s[rs.i];
		rs.j = (rs.j + si + dat[n % datlen]);
		rs.s[rs.i] = rs.s[rs.j];
		rs.s[rs.j] = si;
	}
	rs.j = rs.i;
}

static void arc4_stir() {
	int n;

	if (!rs_data_available) {
		arc4_fetch();
	}
	rs_data_available = 0;
	__sync_synchronize();

	arc4_addrandom((u_char *)&rdat, KEYSIZE);

    /* Throw away the first 1024 bytes to improve randomness */
	for (n = 0; n < 1024; n++)
		(void) arc4_getbyte();

	arc4_count = 1600000;
	rs_stired = 1;
}

static inline int arc4_check_stir(void) {
	if (!rs_stired || arc4_count <= 0) {
		arc4_stir();
		return 1;
	}
	return 0;
}

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


    int did_stir = 0;
    int n = cnt;

    mutex_lock(&arc_mutex);

    while(n--) {
        if(arc4_check_stir())
            did_stir = 1;

        buf[n] = arc4_getbyte();
        arc4_count--;
    }

    mutex_unlock(&arc_mutex);

    if(did_stir) {
		arc4_fetch();
		rs_data_available = 1;
		__sync_synchronize();
    }

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
