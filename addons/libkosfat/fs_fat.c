/* KallistiOS ##version##

   fs_fat.c
   Copyright (C) 2012, 2013, 2014, 2016, 2019 Lawrence Sebald
*/

#include <time.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/queue.h>

#include <kos/fs.h>
#include <kos/mutex.h>
#include <kos/dbglog.h>

#include <fat/fs_fat.h>

#include "fatfs.h"
#include "directory.h"
#include "bpb.h"
#include "ucs.h"

#ifdef __STRICT_ANSI__
/* These don't necessarily get prototyped in string.h in standard-compliant mode
   as they are extensions to the standard. Declaring them this way shouldn't
   hurt. */
char *strtok_r(char *, const char *, char **);
char *strdup(const char *);
#endif

#define MAX_FAT_FILES 16

typedef struct fs_fat_fs {
    LIST_ENTRY(fs_fat_fs) entry;

    vfs_handler_t *vfsh;
    fat_fs_t *fs;
    uint32_t mount_flags;
} fs_fat_fs_t;

LIST_HEAD(fat_list, fs_fat_fs);
static struct fat_list fat_fses;
static mutex_t fat_mutex;

static struct {
    int opened;
    fat_dentry_t dentry;
    uint32_t dentry_cluster;
    uint32_t dentry_offset;
    uint32_t cluster;
    int mode;
    uint32_t ptr;
    dirent_t dent;
    fs_fat_fs_t *fs;
} fh[MAX_FAT_FILES];

static uint16_t longname_buf[256];

static void *fs_fat_open(vfs_handler_t *vfs, const char *fn, int mode) {
    file_t fd;
    fs_fat_fs_t *mnt = (fs_fat_fs_t *)vfs->privdata;
    int rv;

    /* Make sure if we're going to be writing to the file that the fs is mounted
       read/write. */
    if((mode & (O_TRUNC | O_WRONLY | O_RDWR)) &&
       !(mnt->mount_flags & FS_FAT_MOUNT_READWRITE)) {
        errno = EROFS;
        return NULL;
    }

    /* Find a free file handle */
    mutex_lock(&fat_mutex);

    for(fd = 0; fd < MAX_FAT_FILES; ++fd) {
        if(fh[fd].opened == 0) {
            break;
        }
    }

    if(fd >= MAX_FAT_FILES) {
        errno = ENFILE;
        mutex_unlock(&fat_mutex);
        return NULL;
    }

    /* Find the object in question... */
    if((rv = fat_find_dentry(mnt->fs, fn, &fh[fd].dentry,
                             &fh[fd].dentry_cluster, &fh[fd].dentry_offset))) {
        /* XXXX Handle creating files... */
        mutex_unlock(&fat_mutex);
        errno = -rv;
        return NULL;
    }

    /* Make sure we're not trying to open a directory for writing */
    if((fh[fd].dentry.attr & FAT_ATTR_DIRECTORY) &&
       ((mode & O_WRONLY) || !(mode & O_DIR))) {
        errno = EISDIR;
        fh[fd].dentry_cluster = fh[fd].dentry_offset = 0;
        mutex_unlock(&fat_mutex);
        return NULL;
    }

    /* Make sure if we're trying to open a directory that we have a directory */
    if((mode & O_DIR) && !(fh[fd].dentry.attr & FAT_ATTR_DIRECTORY)) {
        errno = ENOTDIR;
        fh[fd].dentry_cluster = fh[fd].dentry_offset = 0;
        mutex_unlock(&fat_mutex);
        return NULL;
    }

    /* XXXX: Handle truncating the file if we need to (for writing). */

    /* Fill in the rest of the handle */
    fh[fd].mode = mode;
    fh[fd].ptr = 0;
    fh[fd].fs = mnt;
    fh[fd].cluster = fh[fd].dentry.cluster_low |
        (fh[fd].dentry.cluster_high << 16);
    fh[fd].opened = 1;

    mutex_unlock(&fat_mutex);
    return (void *)(fd + 1);
}

static int fs_fat_close(void *h) {
    file_t fd = ((file_t)h) - 1;

    mutex_lock(&fat_mutex);

    if(fd < MAX_FAT_FILES && fh[fd].mode) {
        fh[fd].opened = 0;
        fh[fd].dentry_offset = fh[fd].dentry_cluster = 0;
    }

    mutex_unlock(&fat_mutex);
    return 0;
}

static ssize_t fs_fat_read(void *h, void *buf, size_t cnt) {
    file_t fd = ((file_t)h) - 1;
    fat_fs_t *fs;
    uint32_t bs, bo;
    uint8_t *block;
    uint8_t *bbuf = (uint8_t *)buf;
    ssize_t rv;
    uint64_t sz, cl;
    int mode;

    mutex_lock(&fat_mutex);

    /* Check that the fd is valid */
    if(fd >= MAX_FAT_FILES || !fh[fd].opened) {
        mutex_unlock(&fat_mutex);
        errno = EBADF;
        return -1;
    }

    fs = fh[fd].fs->fs;

    /* Make sure the fd is open for reading */
    mode = fh[fd].mode & O_MODE_MASK;
    if(mode != O_RDONLY && mode != O_RDWR) {
        mutex_unlock(&fat_mutex);
        errno = EBADF;
        return -1;
    }

    /* Make sure we're not trying to read a directory with read */
    if(fh[fd].mode & O_DIR) {
        mutex_unlock(&fat_mutex);
        errno = EISDIR;
        return -1;
    }

    /* Did we hit the end of the file? */
    sz = fh[fd].dentry.size;

    if(fat_is_eof(fs, fh[fd].cluster) || fh[fd].ptr >= sz) {
        mutex_unlock(&fat_mutex);
        return 0;
    }

    /* Do we have enough left? */
    if((fh[fd].ptr + cnt) > sz)
        cnt = sz - fh[fd].ptr;

    bs = fat_cluster_size(fs);
    rv = (ssize_t)cnt;
    bo = fh[fd].ptr & (bs - 1);

    /* Handle the first block specially if we are offset within it. */
    if(bo) {
        if(!(block = fat_cluster_read(fs, fh[fd].cluster, &errno))) {
            mutex_unlock(&fat_mutex);
            return -1;
        }

        /* Is there still more to read? */
        if(cnt > bs - bo) {
            memcpy(bbuf, block + bo, bs - bo);
            fh[fd].ptr += bs - bo;
            cnt -= bs - bo;
            bbuf += bs - bo;
            cl = fat_read_fat(fs, fh[fd].cluster, &errno);

            if(cl == 0xFFFFFFFF) {
                mutex_unlock(&fat_mutex);
                return -1;
            }
            else if(fat_is_eof(fs, cl)) {
                mutex_unlock(&fat_mutex);
                errno = EIO;
                return -1;
            }

            fh[fd].cluster = cl;
        }
        else {
            memcpy(bbuf, block + bo, cnt);
            fh[fd].ptr += cnt;

            /* Did we hit the end of the cluster? */
            if(cnt + bo == bs) {
                cl = fat_read_fat(fs, fh[fd].cluster, &errno);

                if(cl == 0xFFFFFFFF) {
                    mutex_unlock(&fat_mutex);
                    return -1;
                }

                fh[fd].cluster = cl;
            }

            cnt = 0;
        }
    }

    /* While we still have more to read, do it. */
    while(cnt) {
        if(!(block = fat_cluster_read(fs, fh[fd].cluster, &errno))) {
            mutex_unlock(&fat_mutex);
            return -1;
        }

        /* Is there still more to read? */
        if(cnt > bs) {
            memcpy(bbuf, block, bs);
            fh[fd].ptr += bs;
            cnt -= bs;
            bbuf += bs;
            cl = fat_read_fat(fs, fh[fd].cluster, &errno);

            if(cl == 0xFFFFFFFF) {
                mutex_unlock(&fat_mutex);
                return -1;
            }
            else if(fat_is_eof(fs, cl)) {
                mutex_unlock(&fat_mutex);
                errno = EIO;
                return -1;
            }

            fh[fd].cluster = cl;
        }
        else {
            memcpy(bbuf, block + bo, cnt);
            fh[fd].ptr += cnt;

            /* Did we hit the end of the cluster? */
            if(cnt + bo == bs) {
                cl = fat_read_fat(fs, fh[fd].cluster, &errno);

                if(cl == 0xFFFFFFFF) {
                    mutex_unlock(&fat_mutex);
                    return -1;
                }

                fh[fd].cluster = cl;
            }

            cnt = 0;
        }
    }

    /* We're done, clean up and return. */
    mutex_unlock(&fat_mutex);
    return rv;
}

static _off64_t fs_fat_seek64(void *h, _off64_t offset, int whence) {
    file_t fd = ((file_t)h) - 1;
    off_t rv;
    uint32_t pos, tmp, bs, cl;
    fat_fs_t *fs;
    int err;

    mutex_lock(&fat_mutex);

    /* Check that the fd is valid */
    if(fd >= MAX_FAT_FILES || !fh[fd].opened || (fh[fd].mode & O_DIR)) {
        mutex_unlock(&fat_mutex);
        errno = EINVAL;
        return -1;
    }

    /* Update current position according to arguments */
    switch(whence) {
        case SEEK_SET:
            pos = offset;
            break;

        case SEEK_CUR:
            pos = fh[fd].ptr + offset;
            break;

        case SEEK_END:
            pos = fh[fd].dentry.size;
            break;

        default:
            mutex_unlock(&fat_mutex);
            errno = EINVAL;
            return -1;
    }

    /* Figure out what cluster we're at... */
    fs = fh[fd].fs->fs;
    bs = fat_cluster_size(fs);
    cl = fh[fd].dentry.cluster_low | (fh[fd].dentry.cluster_high << 16);
    tmp = pos;

    while(tmp >= bs) {
        /* This really shouldn't happen... */
        if(fat_is_eof(fs, cl)) {
            errno = EIO;
            mutex_unlock(&fat_mutex);
            return -1;
        }

        cl = fat_read_fat(fs, cl, &err);

        if(cl == 0xFFFFFFFF) {
            errno = err;
            mutex_unlock(&fat_mutex);
            return -1;
        }

        tmp -= bs;
    }

    fh[fd].ptr = pos;
    fh[fd].cluster = cl;

    rv = (_off64_t)fh[fd].ptr;
    mutex_unlock(&fat_mutex);
    return rv;
}

static _off64_t fs_fat_tell64(void *h) {
    file_t fd = ((file_t)h) - 1;
    off_t rv;

    mutex_lock(&fat_mutex);

    if(fd >= MAX_FAT_FILES || !fh[fd].opened || (fh[fd].mode & O_DIR)) {
        mutex_unlock(&fat_mutex);
        errno = EINVAL;
        return -1;
    }

    rv = (_off64_t)fh[fd].ptr;
    mutex_unlock(&fat_mutex);
    return rv;
}

static uint64 fs_fat_total64(void *h) {
    file_t fd = ((file_t)h) - 1;
    size_t rv;

    mutex_lock(&fat_mutex);

    if(fd >= MAX_FAT_FILES || !fh[fd].opened || (fh[fd].mode & O_DIR)) {
        mutex_unlock(&fat_mutex);
        errno = EINVAL;
        return -1;
    }

    rv = fh[fd].dentry.size;
    mutex_unlock(&fat_mutex);
    return rv;
}

static time_t fat_time_to_stat(uint16_t date, uint16_t time) {
    struct tm tmv;

    /* The MS-DOS epoch is January 1, 1980, not January 1, 1970... */
    tmv.tm_year = (date >> 9) + 10;
    tmv.tm_mon = ((date >> 5) & 0x0F) - 1;
    tmv.tm_mday = date & 0x1F;

    tmv.tm_hour = (time >> 11) & 0x1F;
    tmv.tm_min = (time >> 5) & 0x3F;
    tmv.tm_sec = (time & 0x1F) << 1;

    return mktime(&tmv);
}

static void fill_stat_timestamps(const fat_dentry_t *ent, struct stat *buf) {
    if(!ent->cdate) {
        buf->st_ctime = 0;
    }
    else {
        buf->st_ctime = fat_time_to_stat(ent->cdate, ent->ctime);
    }

    if(!ent->adate) {
        buf->st_atime = 0;
    }
    else {
        buf->st_atime = fat_time_to_stat(ent->adate, 0);
    }

    buf->st_mtime = fat_time_to_stat(ent->mdate, ent->mtime);
}

static void copy_shortname(fat_dentry_t *dent, char *fn) {
    int i, j;

    for(i = 0; i < 8 && dent->name[i] != ' '; ++i) {
        fn[i] = dent->name[i];
    }

    fn[i++] = '.';

    for(j = 0; j < 3 && dent->name[8 + j] != ' '; ++j) {
        fn[i + j] = dent->name[8 + j];
    }

    fn[i + j] = '\0';
}

static void copy_longname(fat_dentry_t *dent) {
    fat_longname_t *lent;
    int fnlen;

    lent = (fat_longname_t *)dent;

    /* We've got our expected long name block... Deal with it. */
    fnlen = ((lent->order - 1) & 0x3F) * 13;

    /* Build out the filename component we have. */
    memcpy(&longname_buf[fnlen], lent->name1, 10);
    memcpy(&longname_buf[fnlen + 5], lent->name2, 12);
    memcpy(&longname_buf[fnlen + 11], lent->name3, 4);
}

static dirent_t *fs_fat_readdir(void *h) {
    file_t fd = ((file_t)h) - 1;
    fat_fs_t *fs;
    uint32_t bs, cl;
    uint8_t *block;
    int err, has_longname = 0;
    fat_dentry_t *dent;

    mutex_lock(&fat_mutex);

    /* Check that the fd is valid */
    if(fd >= MAX_FAT_FILES || !fh[fd].opened || !(fh[fd].mode & O_DIR)) {
        mutex_unlock(&fat_mutex);
        errno = EBADF;
        return NULL;
    }

    fs = fh[fd].fs->fs;

    /* The block size we use here requires a bit of thought...
       If the filesystem is FAT12/FAT16, we use the raw sector size if we're
       reading the root directory. In all other cases (a non-root directory or
       a FAT32 filesystem), we use the cluster size. This is because FAT12 and
       FAT16 do not store their root directory in the data clusters of the
       volume (but all other directories are stored in the data clusters). FAT32
       stores all of its directories in the data area of the volume. */
    if(fat_fs_type(fs) == FAT_FS_FAT32 || fh[fd].dentry_cluster)
        bs = fat_cluster_size(fs);
    else
        bs = fat_block_size(fs);

    /* Make sure we're not at the end of the directory. */
    if(fat_is_eof(fs, fh[fd].cluster)) {
        mutex_unlock(&fat_mutex);
        return NULL;
    }

    /* Read the block we're looking at... */
    if(!(block = fat_cluster_read(fs, fh[fd].cluster, &err))) {
        errno = err;
        mutex_unlock(&fat_mutex);
        return NULL;
    }

    memset(&fh[fd].dent, 0, sizeof(dirent_t));
    memset(longname_buf, 0, sizeof(uint16_t) * 256);

    /* Grab the entry. */
    do {
        dent = (fat_dentry_t *)(block + (fh[fd].ptr & (bs - 1)));
        fh[fd].ptr += 32;

        /* If this is a long name entry, copy the name out... */
        if(FAT_IS_LONG_NAME(dent)) {
            has_longname = 1;
            copy_longname(dent);
        }

        /* Did we hit the end? */
        if(dent->name[0] == FAT_ENTRY_EOD) {
            /* This will work for all versions of FAT, because of how the
               fat_is_eof() function works. */
            fh[fd].cluster = 0x0FFFFFF8;
            mutex_unlock(&fat_mutex);
            return NULL;
        }
        /* This entry is empty, so move onto the next one... */
        else if(dent->name[0] == FAT_ENTRY_FREE || FAT_IS_LONG_NAME(dent)) {
            /* Are we at the end of this block/cluster? */
            if((fh[fd].ptr & (bs - 1)) == 0) {
                if(fat_fs_type(fs) == FAT_FS_FAT32 || fh[fd].dentry_cluster) {
                    cl = fat_read_fat(fs, fh[fd].cluster, &err);

                    if(cl == 0xFFFFFFFF) {
                        errno = err;
                        mutex_unlock(&fat_mutex);
                        return NULL;
                    }
                    else if(fat_is_eof(fs, cl)) {
                        /* We've actually hit the end of the directory... */
                        mutex_unlock(&fat_mutex);
                        return NULL;
                    }
                }
                else {
                    /* Are we at the end of the directory? */
                    if((fh[fd].ptr >> 5) >= fat_rootdir_length(fs)) {
                        fh[fd].cluster = 0x0FFFFFF8;
                        mutex_unlock(&fat_mutex);
                        return NULL;
                    }

                    ++fh[fd].cluster;
                }
            }
        }
    } while(dent->name[0] == FAT_ENTRY_FREE || FAT_IS_LONG_NAME(dent));

    /* We now have a dentry to work with... Fill in the static dirent_t. */
    if(!has_longname)
        copy_shortname(dent, fh[fd].dent.name);
    else
        fat_ucs2_to_utf8((uint8_t *)fh[fd].dent.name, longname_buf, 256,
                         fat_strlen_ucs2(longname_buf));

    fh[fd].dent.size = dent->size;
    fh[fd].dent.time = fat_time_to_stat(dent->mdate, dent->mtime);

    if(dent->attr & FAT_ATTR_DIRECTORY)
        fh[fd].dent.attr = O_DIR;

    /* We're done. Return the static dirent_t. */
    mutex_unlock(&fat_mutex);
    return &fh[fd].dent;
}

static int fs_fat_fcntl(void *h, int cmd, va_list ap) {
    file_t fd = ((file_t)h) - 1;
    int rv = -1;

    (void)ap;

    mutex_lock(&fat_mutex);

    if(fd >= MAX_FAT_FILES || !fh[fd].opened) {
        mutex_unlock(&fat_mutex);
        errno = EBADF;
        return -1;
    }

    switch(cmd) {
        case F_GETFL:
            rv = fh[fd].mode;
            break;

        case F_SETFL:
        case F_GETFD:
        case F_SETFD:
            rv = 0;
            break;

        default:
            errno = EINVAL;
    }

    mutex_unlock(&fat_mutex);
    return rv;
}

static int fs_fat_stat(vfs_handler_t *vfs, const char *path, struct stat *buf,
                       int flag) {
    fs_fat_fs_t *fs = (fs_fat_fs_t *)vfs->privdata;
    uint32_t sz, bs;
    int irv = 0;
    fat_dentry_t ent;
    uint32_t cl, off;

    (void)flag;

    mutex_lock(&fat_mutex);

    /* Find the object in question */
    if((irv = fat_find_dentry(fs->fs, path, &ent, &cl, &off)) < 0) {
        errno = -irv;
        mutex_unlock(&fat_mutex);
        return -1;
    }

    /* Fill in the structure */
    memset(buf, 0, sizeof(struct stat));
    irv = 0;
    buf->st_dev = (dev_t)((ptr_t)fs->vfsh);
    buf->st_ino = ent.cluster_low | (ent.cluster_high << 16);
    buf->st_nlink = 1;
    buf->st_uid = 0;
    buf->st_gid = 0;
    buf->st_blksize = fat_cluster_size(fs->fs);

    /* Read the mode bits... */
    buf->st_mode = S_IRUSR | S_IRGRP | S_IROTH | S_IXUSR | S_IXGRP | S_IXOTH;
    if(!(ent.attr & FAT_ATTR_READ_ONLY)) {
        buf->st_mode |= S_IWUSR | S_IWGRP | S_IWOTH;
    }

    /* Fill in the timestamps... */
    fill_stat_timestamps(&ent, buf);

    /* The rest depends on what type of object this is... */
    if(ent.attr & FAT_ATTR_DIRECTORY) {
        buf->st_mode |= S_IFDIR;
        buf->st_size = 0;
        buf->st_blocks = 0;
    }
    else {
        buf->st_mode |= S_IFREG;
        sz = ent.size;

        if(sz > LONG_MAX) {
            errno = EOVERFLOW;
            irv = -1;
        }

        buf->st_size = sz;
        bs = fat_cluster_size(fs->fs);
        buf->st_blocks = sz / bs;

        if(sz & (bs - 1))
            ++buf->st_blocks;
    }

    mutex_unlock(&fat_mutex);

    return irv;
}

static int fs_fat_rewinddir(void *h) {
    file_t fd = ((file_t)h) - 1;

    mutex_lock(&fat_mutex);

    /* Check that the fd is valid */
    if(fd >= MAX_FAT_FILES || !fh[fd].opened || !(fh[fd].mode & O_DIR)) {
        mutex_unlock(&fat_mutex);
        errno = EBADF;
        return -1;
    }

    /* Rewind to the beginning of the directory. */
    fh[fd].ptr = 0;
    fh[fd].cluster = fh[fd].dentry.cluster_low |
        (fh[fd].dentry.cluster_high << 16);

    mutex_unlock(&fat_mutex);
    return 0;
}

static int fs_fat_fstat(void *h, struct stat *buf) {
    fs_fat_fs_t *fs;
    uint32_t sz, bs;
    file_t fd = ((file_t)h) - 1;
    int irv = 0;
    fat_dentry_t *ent;

    mutex_lock(&fat_mutex);

    if(fd >= MAX_FAT_FILES || !fh[fd].opened) {
        mutex_unlock(&fat_mutex);
        errno = EBADF;
        return -1;
    }

    /* Find the object in question */
    ent = &fh[fd].dentry;
    fs = fh[fd].fs;

    /* Fill in the structure */
    memset(buf, 0, sizeof(struct stat));
    buf->st_dev = (dev_t)((ptr_t)fs->vfsh);
    buf->st_ino = ent->cluster_low | (ent->cluster_high << 16);
    buf->st_nlink = 1;
    buf->st_uid = 0;
    buf->st_gid = 0;
    buf->st_blksize = fat_cluster_size(fs->fs);

    /* Read the mode bits... */
    buf->st_mode = S_IRUSR | S_IRGRP | S_IROTH | S_IXUSR | S_IXGRP | S_IXOTH;
    if(!(ent->attr & FAT_ATTR_READ_ONLY)) {
        buf->st_mode |= S_IWUSR | S_IWGRP | S_IWOTH;
    }

    /* Fill in the timestamps... */
    fill_stat_timestamps(ent, buf);

    /* The rest depends on what type of object this is... */
    if(ent->attr & FAT_ATTR_DIRECTORY) {
        buf->st_mode |= S_IFDIR;
        buf->st_size = 0;
        buf->st_blocks = 0;
    }
    else {
        buf->st_mode |= S_IFREG;
        sz = ent->size;

        if(sz > LONG_MAX) {
            errno = EOVERFLOW;
            irv = -1;
        }

        buf->st_size = sz;
        bs = fat_cluster_size(fs->fs);
        buf->st_blocks = sz / bs;

        if(sz & (bs - 1))
            ++buf->st_blocks;
    }

    mutex_unlock(&fat_mutex);

    return irv;
}

/* This is a template that will be used for each mount */
static vfs_handler_t vh = {
    /* Name Handler */
    {
        { 0 },                  /* name */
        0,                      /* in-kernel */
        0x00010000,             /* Version 1.0 */
        NMMGR_FLAGS_NEEDSFREE,  /* We malloc each VFS struct */
        NMMGR_TYPE_VFS,         /* VFS handler */
        NMMGR_LIST_INIT         /* list */
    },

    0, NULL,                    /* no cacheing, privdata */

    fs_fat_open,                /* open */
    fs_fat_close,               /* close */
    fs_fat_read,                /* read */
    NULL,                       /* write */
    NULL,                       /* seek */
    NULL,                       /* tell */
    NULL,                       /* total */
    fs_fat_readdir,             /* readdir */
    NULL,                       /* ioctl */
    NULL,                       /* rename */
    NULL,                       /* unlink */
    NULL,                       /* mmap */
    NULL,                       /* complete */
    fs_fat_stat,                /* stat */
    NULL,                       /* mkdir */
    NULL,                       /* rmdir */
    fs_fat_fcntl,               /* fcntl */
    NULL,                       /* poll */
    NULL,                       /* link */
    NULL,                       /* symlink */
    fs_fat_seek64,              /* seek64 */
    fs_fat_tell64,              /* tell64 */
    fs_fat_total64,             /* total64 */
    NULL,                       /* readlink */
    fs_fat_rewinddir,           /* rewinddir */
    fs_fat_fstat                /* fstat */
};

static int initted = 0;

/* These two functions borrow heavily from the same functions in fs_romdisk */
int fs_fat_mount(const char *mp, kos_blockdev_t *dev, uint32_t flags) {
    fat_fs_t *fs;
    fs_fat_fs_t *mnt;
    vfs_handler_t *vfsh;

    if(!initted)
        return -1;

    if((flags & FS_FAT_MOUNT_READWRITE) && !dev->write_blocks) {
        dbglog(DBG_DEBUG, "fs_fat: device does not support writing, cannot "
               "mount filesystem as read-write\n");
        return -1;
    }

    if((flags & FS_FAT_MOUNT_READWRITE)) {
        dbglog(DBG_DEBUG, "fs_fat: Read/write mode not yet supported.\n");
        return -1;
    }

    mutex_lock(&fat_mutex);

    /* Try to initialize the filesystem */
    if(!(fs = fat_fs_init(dev, flags))) {
        mutex_unlock(&fat_mutex);
        dbglog(DBG_DEBUG, "fs_fat: device does not contain a valid FAT FS.\n");
        return -1;
    }

    /* Create a mount structure */
    if(!(mnt = (fs_fat_fs_t *)malloc(sizeof(fs_fat_fs_t)))) {
        dbglog(DBG_DEBUG, "fs_fat: out of memory creating fs structure\n");
        fat_fs_shutdown(fs);
        mutex_unlock(&fat_mutex);
        return -1;
    }

    mnt->fs = fs;
    mnt->mount_flags = flags;

    /* Create a VFS structure */
    if(!(vfsh = (vfs_handler_t *)malloc(sizeof(vfs_handler_t)))) {
        dbglog(DBG_DEBUG, "fs_fat: out of memory creating vfs handler\n");
        free(mnt);
        fat_fs_shutdown(fs);
        mutex_unlock(&fat_mutex);
        return -1;
    }

    memcpy(vfsh, &vh, sizeof(vfs_handler_t));
    strcpy(vfsh->nmmgr.pathname, mp);
    vfsh->privdata = mnt;
    mnt->vfsh = vfsh;

    /* Add it to our list */
    LIST_INSERT_HEAD(&fat_fses, mnt, entry);

    /* Register with the VFS */
    if(nmmgr_handler_add(&vfsh->nmmgr)) {
        dbglog(DBG_DEBUG, "fs_fat: couldn't add fs to nmmgr\n");
        free(vfsh);
        free(mnt);
        fat_fs_shutdown(fs);
        mutex_unlock(&fat_mutex);
        return -1;
    }

    mutex_unlock(&fat_mutex);
    return 0;
}

int fs_fat_unmount(const char *mp) {
    fs_fat_fs_t *i;
    int found = 0, rv = 0;

    /* Find the fs in question */
    mutex_lock(&fat_mutex);
    LIST_FOREACH(i, &fat_fses, entry) {
        if(!strcmp(mp, i->vfsh->nmmgr.pathname)) {
            found = 1;
            break;
        }
    }

    if(found) {
        LIST_REMOVE(i, entry);

        /* XXXX: We should probably do something with open files... */
        nmmgr_handler_remove(&i->vfsh->nmmgr);
        fat_fs_shutdown(i->fs);
        free(i->vfsh);
        free(i);
    }
    else {
        errno = ENOENT;
        rv = -1;
    }

    mutex_unlock(&fat_mutex);
    return rv;
}

int fs_fat_sync(const char *mp) {
    fs_fat_fs_t *i;
    int found = 0, rv = 0;

    /* Find the fs in question */
    mutex_lock(&fat_mutex);
    LIST_FOREACH(i, &fat_fses, entry) {
        if(!strcmp(mp, i->vfsh->nmmgr.pathname)) {
            found = 1;
            break;
        }
    }

    if(found) {
        /* fat_fs_sync() will set errno if there's a problem. */
        rv = fat_fs_sync(i->fs);
    }
    else {
        errno = ENOENT;
        rv = -1;
    }

    mutex_unlock(&fat_mutex);
    return rv;
}

int fs_fat_init(void) {
    if(initted)
        return 0;

    LIST_INIT(&fat_fses);
    mutex_init(&fat_mutex, MUTEX_TYPE_NORMAL);
    initted = 1;

    memset(fh, 0, sizeof(fh));

    return 0;
}

int fs_fat_shutdown(void) {
    fs_fat_fs_t *i, *next;

    if(!initted)
        return 0;

    /* Clean up the mounted filesystems */
    i = LIST_FIRST(&fat_fses);
    while(i) {
        next = LIST_NEXT(i, entry);

        /* XXXX: We should probably do something with open files... */
        nmmgr_handler_remove(&i->vfsh->nmmgr);
        fat_fs_shutdown(i->fs);
        free(i->vfsh);
        free(i);

        i = next;
    }

    mutex_destroy(&fat_mutex);
    initted = 0;

    return 0;
}
