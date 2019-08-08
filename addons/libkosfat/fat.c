/* KallistiOS ##version##

   fat.c
   Copyright (C) 2012, 2013, 2019 Lawrence Sebald
*/

#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "fatfs.h"
#include "fatinternal.h"

/* This is basically the same as bgrad_cache from fs_iso9660 */
static void make_mru(fat_fs_t *fs, fat_cache_t **cache, int block) {
    int i;
    fat_cache_t *tmp;

    /* Don't try it with the end block */
    if(block < 0 || block >= fs->cache_size - 1)
        return;

    /* Make a copy and scoot everything down */
    tmp = cache[block];

    for(i = block; i < fs->cache_size - 1; ++i) {
        cache[i] = cache[i + 1];
    }

    cache[fs->cache_size - 1] = tmp;
}


static int fat_fatblock_read_nc(fat_fs_t *fs, uint32_t bn, uint8_t *rv) {
    if(fs->sb.fat_size <= bn)
        return -EINVAL;

    if(fs->dev->read_blocks(fs->dev, bn, 1, rv))
        return -EIO;

    return 0;
}

static int fat_fatblock_write_nc(fat_fs_t *fs, uint32_t bn,
                                 const uint8_t *blk) {
    if(fs->sb.fat_size <= bn)
        return -EINVAL;

    if(fs->dev->write_blocks(fs->dev, bn, 1, blk))
        return -EIO;

    return 0;
}

static uint8_t *fat_read_fatblock(fat_fs_t *fs, uint32_t block, int *err) {
    int i;
    uint8_t *rv;
    fat_cache_t **cache = fs->fcache;

    /* Look through the cache from the most recently used to the least recently
       used entry. */
    for(i = fs->fcache_size - 1; i >= 0; --i) {
        if(cache[i]->block == block && cache[i]->flags) {
            rv = cache[i]->data;
            make_mru(fs, cache, i);
            goto out;
        }
    }

    /* If we didn't get anything, did we end up with an invalid entry or do we
       need to boot someone out? */
    if(i < 0) {
        i = 0;

        /* Make sure that if the block is dirty, we write it back out. */
        if(cache[0]->flags & FAT_CACHE_FLAG_DIRTY) {
            if(fat_fatblock_write_nc(fs, cache[0]->block, cache[0]->data)) {
                /* XXXX: Uh oh... */
                *err = EIO;
                return NULL;
            }
        }
    }

    /* Try to read the block in question. */
    if(fat_fatblock_read_nc(fs, block, cache[i]->data)) {
        *err = EIO;
        cache[i]->flags = 0;            /* Mark it as invalid... */
        return NULL;
    }

    cache[i]->block = block;
    cache[i]->flags = FAT_CACHE_FLAG_VALID;
    rv = cache[i]->data;
    make_mru(fs, cache, i);

out:
    return rv;
}

static int fat_fatblock_mark_dirty(fat_fs_t *fs, uint32_t bn) {
    int i;
    fat_cache_t **cache = fs->fcache;

    /* Look through the cache from the most recently used to the least recently
       used entry. */
    for(i = fs->fcache_size - 1; i >= 0; --i) {
        if(cache[i]->block == bn && cache[i]->flags) {
            cache[i]->flags |= FAT_CACHE_FLAG_DIRTY;
            make_mru(fs, cache, i);
            return 0;
        }
    }

    return -EINVAL;
}

int fat_fatblock_cache_wb(fat_fs_t *fs) {
    int i, err;
    fat_cache_t **cache = fs->fcache;

    /* Don't even bother if we're mounted read-only. */
    #if 0
    if(!(fs->mnt_flags & FAT_MNT_FLAG_RW))
        return 0;
    #endif

    for(i = fs->fcache_size - 1; i >= 0; --i) {
        if(cache[i]->flags & FAT_CACHE_FLAG_DIRTY) {
            if((err = fat_fatblock_write_nc(fs, cache[i]->block, cache[i]->data)))
                return err;

            cache[i]->flags &= ~FAT_CACHE_FLAG_DIRTY;
        }
    }

    return 0;
}

uint32_t fat_read_fat(fat_fs_t *fs, uint32_t cl, int *err) {
    uint32_t sn, off, val;
    const uint8_t *blk, *blk2;

    /* Figure out what sector the value is on... */
    switch(fs->sb.fs_type) {
        case FAT_FS_FAT32:
            cl <<= 2;
            sn = fs->sb.reserved_sectors + (cl / fs->sb.bytes_per_sector);
            off = cl & (fs->sb.bytes_per_sector - 1);

            /* Read the FAT block. */
            blk = fat_read_fatblock(fs, sn, err);
            if(!blk)
                return 0xFFFFFFFF;

            val = blk[off] | (blk[off + 1] << 8) | (blk[off + 2] << 16) |
                (blk[off + 3] << 24);
            break;

        case FAT_FS_FAT16:
            cl <<= 1;
            sn = fs->sb.reserved_sectors + (cl / fs->sb.bytes_per_sector);
            off = cl & (fs->sb.bytes_per_sector - 1);

            /* Read the FAT block. */
            blk = fat_read_fatblock(fs, sn, err);
            if(!blk)
                return 0xFFFFFFFF;

            val = blk[off] | (blk[off + 1] << 8);
            break;

        case FAT_FS_FAT12:
            off = (cl >> 1) + cl;
            sn = fs->sb.reserved_sectors + (off / fs->sb.bytes_per_sector);
            off = off & (fs->sb.bytes_per_sector - 1);

            /* Read the FAT block. */
            blk = fat_read_fatblock(fs, sn, err);
            if(!blk)
                return 0xFFFFFFFF;

            /* See if we have the very special case of the entry spanning two
               blocks... This is why we can't have nice things... */
            if(off == (uint16_t)(fs->sb.bytes_per_sector - 1)) {
                blk2 = fat_read_fatblock(fs, sn + 1, err);

                if(!blk2)
                    return 0xFFFFFFFF;

                /* The bright side here is that we at least know that the
                   cluster number is odd... */
                val = (blk[off] | (blk2[0] << 8)) >> 4;
            }
            else {
                val = blk[off] | (blk[off + 1] << 8);

                /* Which 12 bits do we want? */
                if(cl & 1)
                    val = val >> 4;
                else
                    val = val & 0x0FFF;
            }
            break;

        default:
            return 0xFFFFFFFF;
    }

    return val;
}

int fat_write_fat(fat_fs_t *fs, uint32_t cl, uint32_t val) {
    uint32_t sn, off;
    uint8_t *blk, *blk2;
    int err;

    /* Figure out what sector the value is on... */
    switch(fs->sb.fs_type) {
        case FAT_FS_FAT32:
            cl <<= 2;
            sn = fs->sb.reserved_sectors + (cl / fs->sb.bytes_per_sector);
            off = cl & (fs->sb.bytes_per_sector - 1);

            /* Read the FAT block. */
            blk = fat_read_fatblock(fs, sn, &err);
            if(!blk)
                return err;

            blk[off] = (uint8_t)val;
            blk[off + 1] = (uint8_t)(val >> 8);
            blk[off + 2] = (uint8_t)(val >> 16);

            /* Don't overwrite the top 4 bits... */
            blk[off + 3] = (blk[off + 3] & 0xF0) | ((val >> 24) & 0x0F);

            /* Mark it as dirty... */
            fat_fatblock_mark_dirty(fs, sn);
            break;

        case FAT_FS_FAT16:
            cl <<= 1;
            sn = fs->sb.reserved_sectors + (cl / fs->sb.bytes_per_sector);
            off = cl & (fs->sb.bytes_per_sector - 1);

            /* Read the FAT block. */
            blk = fat_read_fatblock(fs, sn, &err);
            if(!blk)
                return err;

            blk[off] = (uint8_t)val;
            blk[off + 1] = (uint8_t)(val >> 8);

            /* Mark it as dirty... */
            fat_fatblock_mark_dirty(fs, sn);
            break;

        case FAT_FS_FAT12:
            off = (cl >> 1) + cl;
            sn = fs->sb.reserved_sectors + (off / fs->sb.bytes_per_sector);
            off = off & (fs->sb.bytes_per_sector - 1);

            /* Read the FAT block. */
            blk = fat_read_fatblock(fs, sn, &err);
            if(!blk)
                return err;

            /* See if we have the very special case of the entry spanning two
               blocks... This is why we can't have nice things... */
            if(off == (uint16_t)(fs->sb.bytes_per_sector - 1)) {
                blk2 = fat_read_fatblock(fs, sn + 1, &err);

                if(!blk2)
                    return err;

                /* The bright side here is that we at least know that the
                   cluster number is odd... */
                val <<= 4;
                blk[off] = (uint8_t)((blk[off] & 0x0F) | (val & 0xF0));
                blk2[0] = (uint8_t)(val >> 8);

                /* Mark it as dirty... */
                fat_fatblock_mark_dirty(fs, sn);
                fat_fatblock_mark_dirty(fs, sn + 1);
            }
            else {
                if(cl & 1) {
                    val <<= 4;
                    blk[off] = (uint8_t)((blk[off] & 0x0F) | (val & 0xF0));
                    blk[off + 1] = (uint8_t)(val >> 8);
                }
                else {
                    blk[off + 1] = (uint8_t)((blk[off + 1] & 0xF0) |
                        ((val >> 8) & 0x0F));
                    blk[off] = (uint8_t)(val);
                }

                /* Mark it as dirty... */
                fat_fatblock_mark_dirty(fs, sn);
            }
            break;
    }

    return 0;
}

int fat_is_eof(fat_fs_t *fs, uint32_t cl) {
    switch(fs->sb.fs_type) {
        case FAT_FS_FAT32:
            return ((cl & 0x0FFFFFFF) >= 0x0FFFFFF8);

        case FAT_FS_FAT16:
            return (cl >= 0xFFF8 && !(cl & 0x80000000));

        case FAT_FS_FAT12:
            return (cl >= 0x0FF8 && !(cl & 0x80000000));
    }

    return -1;
}
