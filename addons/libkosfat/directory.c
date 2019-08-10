/* KallistiOS ##version##

   directory.c
   Copyright (C) 2019 Lawrence Sebald
*/

#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <inttypes.h>

#include "fatfs.h"
#include "ucs.h"
#include "directory.h"
#include "fatinternal.h"

#ifdef __STRICT_ANSI__
/* These don't necessarily get prototyped in string.h in standard-compliant mode
   as they are extensions to the standard. Declaring them this way shouldn't
   hurt. */
char *strtok_r(char *, const char *, char **);
char *strdup(const char *);
#endif

static uint16_t longname_buf[256], longname_buf2[256];

static int fat_search_dir(fat_fs_t *fs, const char *fn, uint32_t cluster,
                          fat_dentry_t *rv, uint32_t *rcl, uint32_t *roff) {
    uint8_t *cl;
    int err, done = 0;
    uint32_t i, j = 0, max;
    fat_dentry_t *ent;

    /* Figure out how many directory entries there are in each cluster/block. */
    if(fs->sb.fs_type == FAT_FS_FAT32 || !(cluster & 0x80000000)) {
        /* Either we're working with a regular directory or we're working with
           the root directory on FAT32 (which is the same as a normal
           directory). We care about the number of entries per cluster. */
        max = (fs->sb.bytes_per_sector * fs->sb.sectors_per_cluster) >> 5;
    }
    else {
        /* We're working with the root directory on FAT12/FAT16, so we need to
           look at the number of entries per sector. */
        max = fs->sb.bytes_per_sector >> 5;
    }

    while(!done) {
        if(!(cl = fat_cluster_read(fs, cluster, &err))) {
            dbglog(DBG_ERROR, "Error reading directory at cluster %" PRIu32
                   ": %s\n", cluster, strerror(err));
            return -EIO;
        }

        for(i = 0; i < max && !done; ++i, ++j) {
            ent = (fat_dentry_t *)(cl + (i << 5));

            /* If name[0] is zero, then we've hit the end of the directory. */
            if(ent->name[0] == FAT_ENTRY_EOD) {
                return -ENOENT;
            }
            /* If name[0] == 0xE5, then this entry is empty (but there might
               still be additional entries after it). */
            else if(ent->name[0] == FAT_ENTRY_FREE) {
                continue;
            }
            /* Ignore long name entries. */
            else if(FAT_IS_LONG_NAME(ent)) {
                continue;
            }

            /* We've got something useful, let's see if it's what we're looking
               for... */
            if(!memcmp(fn, ent->name, 11)) {
                *rv = *ent;
                *rcl = cluster;
                *roff = (i << 5);
                return 0;
            }
        }

        if(!(cluster & 0x80000000)) {
            cluster = fat_read_fat(fs, cluster, &err);
            if(cluster == 0xFFFFFFFF)
                return -err;

            if(fat_is_eof(fs, cluster))
                done = 1;
        }
        else {
            ++cluster;

            if(j >= fs->sb.root_dir)
                done = 1;
        }
    }

    return -ENOENT;
}

static int read_longname(fat_fs_t *fs, uint32_t *cluster, uint32_t *offset,
                         uint32_t max, int32_t *max2) {
    int err, done = 0;
    uint32_t i, fnlen;
    fat_dentry_t *ent;
    fat_longname_t *lent;
    uint8_t *cl;

    while(!done) {
        if(!(cl = fat_cluster_read(fs, *cluster, &err))) {
            dbglog(DBG_ERROR, "Error reading directory at cluster %" PRIu32
                   ": %s\n", *cluster, strerror(err));
            return -EIO;
        }

        for(i = (*offset) + 1; i < max; ++i) {
            ent = (fat_dentry_t *)(cl + (i << 5));

            /* If name[0] is zero, then we've hit the end of the directory. */
            if(ent->name[0] == FAT_ENTRY_EOD) {
                return -EIO;
            }
            /* If name[0] == 0xE5, then this entry is empty (but there might
               still be additional entries after it). */
            else if(ent->name[0] == FAT_ENTRY_FREE) {
                return -EIO;
            }
            /* Non-long name entries in the middle of a long name entry are
               bad... */
            else if(!FAT_IS_LONG_NAME(ent)) {
                return -EIO;
            }

            lent = (fat_longname_t *)ent;

            /* We've got our expected long name block... Deal with it. */
            fnlen = ((lent->order - 1) & 0x3F) * 13;

            /* Build out the filename component we have. */
            memcpy(&longname_buf[fnlen], lent->name1, 10);
            memcpy(&longname_buf[fnlen + 5], lent->name2, 12);
            memcpy(&longname_buf[fnlen + 11], lent->name3, 4);

            /* XXXX: Check the checksum here. */

            if((lent->order & 0x3F) == 1) {
                *offset = i;
                return 0;
            }
        }

        if(!(*cluster & 0x80000000)) {
            *cluster = fat_read_fat(fs, *cluster, &err);
            if(*cluster == 0xFFFFFFFF)
                return -EIO;

            if(fat_is_eof(fs, *cluster))
                done = 1;
        }
        else {
            ++*cluster;

            *max2 -= max;
            if(*max2 <= 0)
                return -EIO;
        }
    }

    return -EIO;
}

static int fat_search_long(fat_fs_t *fs, const char *fn, uint32_t cluster,
                           fat_dentry_t *rv, uint32_t *rcl, uint32_t *roff) {
    uint8_t *cl;
    int err, done = 0;
    uint32_t i, max, skip = 0;
    int32_t max2;
    fat_dentry_t *ent;
    fat_longname_t *lent;
    size_t l = strlen(fn);
    uint32_t fnlen;

    /* Figure out how many directory entries there are in each cluster/block. */
    if(fs->sb.fs_type == FAT_FS_FAT32 || !(cluster & 0x80000000)) {
        /* Either we're working with a regular directory or we're working with
           the root directory on FAT32 (which is the same as a normal
           directory). We care about the number of entries per cluster. */
        max = (fs->sb.bytes_per_sector * fs->sb.sectors_per_cluster) >> 5;
        max2 = 0xFFFFFFFF;
    }
    else {
        /* We're working with the root directory on FAT12/FAT16, so we need to
           look at the number of entries per sector. */
        max = fs->sb.bytes_per_sector >> 5;
        max2 = (int32_t)fs->sb.root_dir;
    }

    fat_utf8_to_ucs2(longname_buf2, (const uint8_t *)fn, 256, l);

    while(!done) {
        if(!(cl = fat_cluster_read(fs, cluster, &err))) {
            dbglog(DBG_ERROR, "Error reading directory at cluster %" PRIu32
                   ": %s\n", cluster, strerror(err));
            return -EIO;
        }

        for(i = 0; i < max && !done; ++i) {
            ent = (fat_dentry_t *)(cl + (i << 5));

            /* Are we skipping this entry? */
            if(skip) {
                --skip;
                continue;
            }

            /* If name[0] is zero, then we've hit the end of the directory. */
            if(ent->name[0] == FAT_ENTRY_EOD) {
                return -ENOENT;
            }
            /* If name[0] == 0xE5, then this entry is empty (but there might
               still be additional entries after it). */
            else if(ent->name[0] == FAT_ENTRY_FREE) {
                continue;
            }
            /* Ignore entries that aren't long name entries... */
            else if(!FAT_IS_LONG_NAME(ent)) {
                continue;
            }

            lent = (fat_longname_t *)ent;

            /* We've got a long name entry, see if we even care about it (i.e,
               if the length is feasible and if the entry is sane). */
            if(!(lent->order & FAT_ORDER_LAST))
                continue;

            fnlen = (lent->order & 0x3F) * 13;
            if(l > fnlen) {
                skip = (lent->order & 0x3F);
                continue;
            }

            /* Build out the filename component we have. */
            fnlen -= 13;
            memcpy(&longname_buf[fnlen], lent->name1, 10);
            memcpy(&longname_buf[fnlen + 5], lent->name2, 12);
            memcpy(&longname_buf[fnlen + 11], lent->name3, 4);
            longname_buf[fnlen + 14] = 0;

            /* XXXX: Check the checksum here. */

            /* Now, is the filename length *actually* right? */
            fnlen += fat_strlen_ucs2(longname_buf + fnlen);
            if(l > fnlen) {
                skip = (lent->order & 0x3F);
                continue;
            }

            /* Is this the only long name entry needed? */
            if(lent->order != 0x41) {
                if(read_longname(fs, &cluster, &i, max, &max2))
                    return -EIO;
            }

            fat_ucs2_tolower(longname_buf, fnlen);
            fat_ucs2_tolower(longname_buf2, fnlen);

            if(!memcmp(longname_buf, longname_buf2, fnlen)) {
                /* The next entry should be the dentry we want (that is to say,
                   the short name entry for this long name). */
                if(i < max) {
                    ent = (fat_dentry_t *)(cl + ((i + 1) << 5));
                    *rv = *ent;
                    *rcl = cluster;
                    *roff = (i << 5);
                    return 0;
                }
                else {
                    /* Ugh. The short entry is in another cluster, so read it
                       in to return it. */
                    if(!(cluster & 0x80000000)) {
                        cluster = fat_read_fat(fs, cluster, &err);
                        if(cluster == 0xFFFFFFFF)
                            return -err;

                        if(fat_is_eof(fs, cluster))
                            return -EIO;
                    }
                    else {
                        ++cluster;
                        max2 -= max;

                        if(max2 <= 0)
                            return -EIO;
                    }

                    if(!(cl = fat_cluster_read(fs, cluster, &err))) {
                        dbglog(DBG_ERROR, "Error reading directory at cluster %"
                               PRIu32 ": %s\n", cluster, strerror(err));
                        return -EIO;
                    }

                    ent = (fat_dentry_t *)cl;
                    *rv = *ent;
                    *rcl = cluster;
                    *roff = 0;
                    return 0;
                }
            }
            else
                skip = 1;
        }

        if(!(cluster & 0x80000000)) {
            cluster = fat_read_fat(fs, cluster, &err);
            if(cluster == 0xFFFFFFFF)
                return -err;

            if(fat_is_eof(fs, cluster))
                done = 1;
        }
        else {
            ++cluster;
            max2 -= max;

            if(max2 <= 0)
                done = 1;
        }
    }

    return -ENOENT;
}

static void normalize_shortname(const char *fn, char out[11]) {
    size_t l = strlen(fn), i;
    char *dot = strrchr(fn, '.');
    size_t le = 0;

    if(dot)
        le = strlen(dot) - 1;

    for(i = 0; i < 8 && i < l && dot != fn + i; ++i) {
        out[i] = toupper((int)fn[i]);
    }

    for(; i < 8; ++i) {
        out[i] = ' ';
    }

    for(i = 0; i < 3 && i < le; ++i) {
        out[8 + i] = toupper((int)dot[i + 1]);
    }

    for(; i < 3; ++i) {
        out[8 + i] = ' ';
    }
}

static int is_component_short(const char *fn) {
    size_t l = strlen(fn), i;
    char *dot = strrchr(fn, '.');
    int dotct = 0;

    /* 8.3 == 12 characters, so anything more than that can't be a shortname. */
    if(l > 12)
        return 0;

    /* Short filenames can't start with a dot. */
    if(dot == fn)
        return 0;

    /* If the basename of the component is more than 8 characters, then it
       can't be a 8.3 filename. */
    if(dot > fn + 8)
        return 0;

    if(dot) {
        /* If we have more than 4 characters including the dot, then it can't be
           a shortname. */
        if(strlen(dot) > 4)
            return 0;
    }
    else {
        /* We can't have more than 8 characters if there's no dot. */
        if(l > 8)
            return 0;
    }

    /* Short names can't have certain characters... */
    for(i = 0; i < l; ++i) {
        if(fn[i] == '+' || fn[i] == ',' || fn[i] == ';' || fn[i] == '[' ||
           fn[i] == ']' || fn[i] == ' ' || fn[i] == '=')
            return 0;

        if(fn[i] == '.')
            ++dotct;

        /* Technically, these characters aren't allowed in long names either. */
        if(fn[i] == '*' || fn[i] == ':' || fn[i] == '/' || fn[i] == '\\' ||
           fn[i] == '|' || fn[i] == '"' || fn[i] == '?' || fn[i] == '<' ||
           fn[i] == '>')
            return 0;
    }

    /* We can't have more than one dot in a shortname */
    if(dotct > 1)
        return 0;

    /* If we get here, it should be fine... */
    return 1;
}

int fat_find_child(fat_fs_t *fs, const char *fn, fat_dentry_t *parent,
                   fat_dentry_t *rv, uint32_t *rcl, uint32_t *roff) {
    char *fnc = strdup(fn), comp[11];
    uint32_t cl;
    int err;

    cl = parent->cluster_low | (parent->cluster_high << 16);

    if(is_component_short(fnc)) {
        normalize_shortname(fnc, comp);

        err = fat_search_dir(fs, fnc, cl, rv, rcl, roff);
    }
    else {
        err = fat_search_long(fs, fnc, cl, rv, rcl, roff);
    }

    free(fnc);
    return err;
}

int fat_find_dentry(fat_fs_t *fs, const char *fn, fat_dentry_t *rv,
                    uint32_t *rcl, uint32_t *roff) {
    char *fnc = strdup(fn), comp[11], *tmp, *tok;
    int err = -ENOENT;
    fat_dentry_t cur;
    uint32_t cl, off;

    /* First thing is first, tokenize the filename into components... */
    tok = strtok_r(fnc, "/", &tmp);

    /* If we don't get anything back here, they've asked for the root directory.
       Make up a dentry for it. */
    if(!tok) {
        memset(rv, 0, sizeof(fat_dentry_t));
        rv->attr = FAT_ATTR_DIRECTORY;

        if(fs->sb.fs_type == FAT_FS_FAT32) {
            rv->cluster_high = fs->sb.root_dir >> 16;
            rv->cluster_low = (uint16_t)fs->sb.root_dir;
        }
        else {
            rv->cluster_high = 0x8000;
            rv->cluster_low = (uint16_t)(fs->sb.reserved_sectors +
                                         (fs->sb.num_fats * fs->sb.fat_size));
        }

        *rcl = 0;
        *roff = 0;
        err = 0;
        goto out;
    }

    if(fs->sb.fs_type == FAT_FS_FAT32) {
        cl = fs->sb.root_dir;
    }
    else {
        cl = 0x80000000 | (fs->sb.reserved_sectors + (fs->sb.num_fats *
            fs->sb.fat_size));
    }

    if(is_component_short(tok)) {
        normalize_shortname(tok, comp);

        if((err = fat_search_dir(fs, comp, cl, &cur, &cl, &off)) < 0)
            goto out;
    }
    else {
        if((err = fat_search_long(fs, tok, cl, &cur, &cl, &off)) < 0)
            goto out;
    }

    tok = strtok_r(NULL, "/", &tmp);
    cl = cur.cluster_low | (cur.cluster_high << 16);


    while(tok) {
        /* Make sure what we're looking at is a directory... */
        if(!(cur.attr & FAT_ATTR_DIRECTORY)) {
            err = -ENOTDIR;
            goto out;
        }

        if(is_component_short(tok)) {
            normalize_shortname(tok, comp);

            if((err = fat_search_dir(fs, comp, cl, &cur, &cl, &off)) < 0) {
                goto out;
            }
        }
        else {
            if((err = fat_search_long(fs, tok, cl, &cur, &cl, &off)) < 0)
                goto out;
        }

        tok = strtok_r(NULL, "/", &tmp);
        cl = cur.cluster_low | (cur.cluster_high << 16);
    }

    /* One last check... If the filename the user passed in ends with a '/'
       character, make sure we actually have a directory... */
    if(fn[strlen(fn) - 1] == '/' && !(cur.attr & FAT_ATTR_DIRECTORY)) {
        err = -ENOTDIR;
        goto out;
    }

    *rv = cur;
    *rcl = cl;
    *roff = off;

out:
    free(fnc);
    return err;
}

#ifdef FAT_DEBUG
void fat_dentry_print(const fat_dentry_t *ent) {
    uint32_t cl = ent->cluster_low | (ent->cluster_high << 16);

    dbglog(DBG_KDEBUG, "Filename: %.11s\n", ent->name);
    dbglog(DBG_KDEBUG, "Attributes: %02x\n", ent->attr);
    dbglog(DBG_KDEBUG, "Cluster: %" PRIu32 "\n", cl);
    dbglog(DBG_KDEBUG, "Size: %" PRIu32 "\n", ent->size);
}

static void fat_regdir_print(fat_fs_t *fs, uint32_t cluster) {
    uint8_t *cl;
    int err, done = 0;
    uint32_t i, max;
    fat_dentry_t *ent;

    /* Figure out how many directory entries there are in a cluster. */
    max = (fs->sb.bytes_per_sector * fs->sb.sectors_per_cluster) >> 5;

    while(!done) {
        if(!(cl = fat_cluster_read(fs, cluster, &err))) {
            dbglog(DBG_ERROR, "Error reading directory at cluster %" PRIu32
                   ": %s\n", cluster, strerror(err));
            return;
        }

        for(i = 0; i < max && !done; ++i) {
            ent = (fat_dentry_t *)(cl + (i << 5));

            /* If name[0] is zero, then we've hit the end of the directory. */
            if(ent->name[0] == FAT_ENTRY_EOD) {
                done = 1;
                break;
            }
            /* If name[0] == 0xE5, then this entry is empty (but there might
               still be additional entries after it). */
            else if(ent->name[0] == FAT_ENTRY_FREE) {
                continue;
            }
            /* Ignore long name entries, for now. */
            else if((ent->attr & FAT_ATTR_LONG_NAME_MASK) == FAT_ATTR_LONG_NAME) {
                continue;
            }

            /* We've got something useful, let's print it out. */
            dbglog(DBG_KDEBUG, "%.11s\n", ent->name);
        }

        cluster = fat_read_fat(fs, cluster, &err);
        if(cluster == 0xFFFFFFFF)
            done = 1;

        if(fat_is_eof(fs, cluster))
            done = 1;
    }
}

static void fat_fat16_root_print(fat_fs_t *fs) {
    uint8_t cl[fs->sb.bytes_per_sector];
    int done = 0;
    uint32_t i, max, max2, block;
    fat_dentry_t *ent;

    /* Figure out how many directory entries there are in a block. */
    max = fs->sb.bytes_per_sector >> 5;
    max2 = fs->sb.root_dir;
    block = fs->sb.reserved_sectors + (fs->sb.num_fats * fs->sb.fat_size);

    while(!done && max2) {
        if(fs->dev->read_blocks(fs->dev, block, 1, cl)) {
            dbglog(DBG_ERROR, "Error reading directory at block %" PRIu32
                   ": %s\n", block, strerror(EIO));
            return;
        }

        for(i = 0; i < max && !done; ++i) {
            ent = (fat_dentry_t *)(cl + (i << 5));

            /* If name[0] is zero, then we've hit the end of the directory. */
            if(ent->name[0] == FAT_ENTRY_EOD) {
                done = 1;
                break;
            }
            /* If name[0] == 0xE5, then this entry is empty (but there might
               still be additional entries after it). */
            else if(ent->name[0] == FAT_ENTRY_FREE) {
                continue;
            }
            /* Ignore long name entries, for now. */
            else if((ent->attr & FAT_ATTR_LONG_NAME_MASK) == FAT_ATTR_LONG_NAME) {
                continue;
            }

            /* We've got something useful, let's print it out. */
            dbglog(DBG_KDEBUG, "%.11s\n", ent->name);
        }

        max2 -= max;
        ++block;
    }
}

void fat_directory_print(fat_fs_t *fs, uint32_t cluster) {
    /* The root directory has to be handled specially... */
    if(cluster == 0) {
        switch(fs->sb.fs_type) {
            case FAT_FS_FAT32:
                fat_regdir_print(fs, fs->sb.root_dir);
                return;

            default:
                fat_fat16_root_print(fs);
                return;
        }
    }
    else {
        fat_regdir_print(fs, cluster);
    }
}
#endif
