#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wbfs.h"

#define WBFS_MAGIC_REVERSE ('S' << 24 | 'F' << 16 | 'B' << 8 | 'W')

#define GIGABYTE (1000 * 1000 * 1000)
#define DVD_SIZE_1 (4.7 * GIGABYTE ull)
#define DVD_SIZE_2 (8.54 * GIGABYTE ull)
#define WII_DISC_SECTOR_SIZE (0x8000ull)
#define WII_DISC_1_SECTOR_COUNT (143432ull)
#define WII_DISC_2_SECTOR_COUNT (260620ull)

/*
 * We can mark a WBFS file as valid by putting a magic number at the end of the struct, then if the pointer we
 * recieve is not null but not propperly allocated for we can exit out early and tell the user to correctly
 * perform allocs.
 *
 * We can also define a macro which validates a WBFS handle by setting the magic to the reverse of the magic,
 * it will also return out of the current function, this means we can return errors to the user while making
 * sure they don't euse the handle
 */
#define WBFS_VALID(WBFS)                 \
    if (!(WBFS)) return e_wbfs_segfault; \
    if (!(WBFS)->valid != WBFS_MAGIC) return e_wbfs_invalid_handle;
#define WBFS_INVALIDATE(WBFS, ENUM)     \
    (WBFS)->valid = WBFS_MAGIC_REVERSE; \
    return (ENUM);

wbfs_enum wbfs_file_header_parse(Wbfs* wbfs_handle, WbfsFileHeader* wbfs_fh, FILE* fp)
{
    // Check that all of these are not null, can't use the macro as we haven't marked it as valid yet
    if (!wbfs_handle || !wbfs_fh || !fp) return e_wbfs_segfault;

    // Memset the two user data structs and then associate the pointers together
    memset(wbfs_handle, 0, sizeof(Wbfs));
    memset(wbfs_fh, 0, sizeof(WbfsFileHeader));
    wbfs_handle->file_header = wbfs_fh;
    wbfs_handle->fp = fp;

    // Seek to the begining of the file and read in all the data that isn't the disc table
    fseek(fp, 0, SEEK_SET);
    fread(wbfs_fh, 1, sizeof(WbfsFileHeader) - sizeof(uint8_t*), fp);

    // This is the first time we encounter endianness as a problem, and we'll explain it here once. The file
    // format stores thing in big endian, which is most significant bytes first. However, most PCs are litle
    // endian, when we try to stuff raw data into the uint32_t we put it in the wrong order. However fields
    // that are only one byte long can't be in the wrong order, so we only need to swap bytes for the long
    // fields
    wbfs_helper_reverse_endian_32(&wbfs_fh->magic);
    wbfs_helper_reverse_endian_32(&wbfs_fh->hd_sector_count);

    // Check that the magic number matched
    if (wbfs_fh->magic != WBFS_MAGIC) WBFS_INVALIDATE(wbfs_handle, e_wbfs_magic_fail_wbfs);

    // Got to the end succssfully, mark the wbfs as sucessful
    wbfs_handle->valid = WBFS_MAGIC;
    return e_wbfs_success;
}
