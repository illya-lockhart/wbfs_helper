#include "wbfs.h"

void wbfs_helper_reverse_endian_32(uint32_t* d)
{
    *d = ((0x000000FF & *d) << 24) | ((0x0000FF00 & *d) << 8) | ((0x00FF0000 & *d) >> 8) |
         ((0xFF000000 & *d) >> 24);
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
        default:
            return "Unknown error code???";
            break;
    }
}
