#include "wbfs.h"

void wbfs_helper_reverse_endian_32(uint32_t* d)
{
    *d = ((0x000000FF & *d) << 24) | ((0x0000FF00 & *d) << 8) | ((0x00FF0000 & *d) >> 8) |
         ((0xFF000000 & *d) >> 24);
}

void wbfs_helper_reverse_endian_16(uint16_t* d) { *d = ((0xFF00 & *d) >> 8) | ((0x00FF & *d) << 8); }

size_t wbfs_helper_disc_table_size(Wbfs* wbfs)
{
    // The wbfs file header takes up the first hard drive sector, every that isn't the disc table takes up 12
    // bytes
    return wbfs->hd_sector_size - 12;
}

size_t wbfs_helper_sector_table_size(Wbfs* wbfs)
{
    // A block table uses 16 bits per sector to store each sectors location
    return wbfs->wbfs_sectors_per_disc * sizeof(uint16_t);
}

const char* wbfs_helper_enum_lookup(wbfs_enum e)
{
    switch (e) {
        case e_wbfs_success:
            return "Success!";
            break;
        case e_wbfs_segfault:
            return "A null pointer was detected, most likely you didn't back the memory passed into this "
                   "function";
            break;
        case e_wbfs_magic_fail_wbfs:
            return "When trying to parse the header in the first sector of the WBFS file, the first 4 bytes "
                   "were not WBFS, most likely you have not parsed a WBFS file";
            break;
        case e_wbfs_invalid_handle:
            return "An invalid WBFS handle was passed, this can happen if a previous function has failed, or "
                   "the pointer passed to the function wasn't null but didn't point to valid memory";
            break;
        case e_wbfs_invalid_disc_table:
            return "Currently I only know how to read one disc on the disc table, so good luck checking "
                   "anything else";
            break;
        default:
            return "Unknown error code???";
            break;
    }
}
