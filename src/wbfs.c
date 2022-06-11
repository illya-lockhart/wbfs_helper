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
#define WBFS_VALID(WBFS)                                               \
    {                                                                  \
        if (!(WBFS)) return e_wbfs_segfault;                           \
        if (!(WBFS)->fp) return e_wbfs_segfault;                       \
        if (!(WBFS)->file_header) return e_wbfs_segfault;              \
        if ((WBFS)->valid != WBFS_MAGIC) return e_wbfs_invalid_handle; \
    }

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
    fread(wbfs_fh, sizeof(WbfsFileHeader) - sizeof(uint8_t*), 1, fp);

    // This is the first time we encounter endianness as a problem, and we'll explain it here once. The file
    // format stores thing in big endian, which is most significant bytes first. However, most PCs are litle
    // endian, when we try to stuff raw data into the uint32_t we put it in the wrong order. However fields
    // that are only one byte long can't be in the wrong order, so we only need to swap bytes for the long
    // fields
    wbfs_helper_reverse_endian_32(&wbfs_fh->magic);
    wbfs_helper_reverse_endian_32(&wbfs_fh->hd_sector_count);

    // Check that the magic number matched
    if (wbfs_fh->magic != WBFS_MAGIC) {
        WBFS_INVALIDATE(wbfs_handle, e_wbfs_magic_fail_wbfs);
    }

    // Extract the sector sizes by raising 2 to the power of the sector size
    wbfs_handle->hd_sector_size = 1ull << wbfs_fh->hd_sector_shift;
    wbfs_handle->wbfs_sector_size = 1ull << wbfs_fh->wbfs_sector_shift;
    wbfs_handle->wbfs_file_size = wbfs_fh->hd_sector_count * wbfs_handle->hd_sector_size;

    // Calculate the number of wbfs sectors required to fit in a 2 sided wii disk
    uint64_t wii_disc_byte_count = WII_DISC_2_SECTOR_COUNT * WII_DISC_SECTOR_SIZE;
    wbfs_handle->wbfs_sectors_per_disc =
      (wii_disc_byte_count + wbfs_handle->wbfs_sector_size - 1) / wbfs_handle->wbfs_sector_size;

    // Got to the end succssfully, mark the wbfs as sucessful
    wbfs_handle->valid = WBFS_MAGIC;
    return e_wbfs_success;
}

wbfs_enum wbfs_file_disc_table_parse(Wbfs* wbfs)
{
    WBFS_VALID(wbfs);
    if (!wbfs->file_header->disc_table) return e_wbfs_segfault;

    // Seek to the start of the file, and then after those 12 bytes that make up the rest of the header
    fseek(wbfs->fp, 12, SEEK_SET);
    fread(wbfs->file_header->disc_table, 1, wbfs->hd_sector_size - 12, wbfs->fp);

    // I only know how to pass one disc at a time, so in this case evaluate that the first byte of the disc
    // table is 0x1
    if (wbfs->file_header->disc_table[0] != 0x1) {
        return e_wbfs_invalid_disc_table;
    }
    wbfs->wii_disc_count = 1;
    return e_wbfs_success;
}

/*
 * Wii Disc Section
 */

#define DISC_VALID(DISC)                                                \
    {                                                                   \
        if (!(DISC)) return e_wbfs_segfault;                            \
        if ((DISC)->wbfs_offset == 0) return e_wbfs_invalid_disc_table; \
        if (!(DISC)->wbfs) return e_wbfs_segfault;                      \
        WBFS_VALID((DISC)->wbfs);                                       \
    }

wbfs_enum wbfs_disc_get_offset(WiiDisc* disc, Wbfs* wbfs, uint8_t index)
{
    WBFS_VALID(wbfs);
    if (!wbfs->file_header->disc_table || !disc) return e_wbfs_segfault;

    // Set up wii disc including associating it to a wbfs struct owner
    disc->wbfs = wbfs;

    // look up offset sector in the wbfs file header
    disc->wbfs_offset = wbfs->file_header->disc_table[index] * wbfs->hd_sector_size;
    if (disc->wbfs_sector_lookup == 0) return e_wbfs_invalid_disc_table;

    return e_wbfs_success;
}

wbfs_enum wbfs_disc_parse_sector_table(WiiDisc* disc)
{
    DISC_VALID(disc);

    // The wii disc - wbfs header is located at the start of the offset
    // There is then a partial copy of the wii disc info, but not the entire thing
    // So seek to the offset plus the size of the partial header
    fseek(disc->wbfs->fp, disc->wbfs_offset + 256, SEEK_SET);
    fread(disc->wbfs_sector_lookup, sizeof(uint16_t), disc->wbfs->wbfs_sectors_per_disc, disc->wbfs->fp);

    // For each element in the disc table we have to reverse the endianness since the type is larger than one
    // byte
    for (uint32_t i = 0; i < disc->wbfs->wbfs_sectors_per_disc; i++) {
        wbfs_helper_reverse_endian_16(disc->wbfs_sector_lookup + i);
    }

    return e_wbfs_success;
}

wbfs_enum wbfs_disc_read_buffer(WiiDisc* disc, void* data, uint64_t address, uint64_t size)
{
    DISC_VALID(disc);
    Wbfs* wbfs = disc->wbfs;

    // We need to be careful to ensure our reads don't cross a boundry of the wbfs sector
    uint64_t bytes_read = 0;
    while (bytes_read < size) {
        // Advance the address to account for all of the reads that have taken place so far
        uint64_t local_address = address + bytes_read;

        // The wbfs sectors are not in order. So we need to get which sector the address is in, and then
        // lookup that index in the sector table. This will tell us where in the wbfs file this sector
        // actually starts.
        uint16_t sector_index = local_address / wbfs->wbfs_sector_size;
        uint16_t lookup_index = disc->wbfs_sector_lookup[sector_index];

        // We can't look in the first wbfs sector because that's where the wbfs header is stored
        if (lookup_index == 0) return e_wbfs_invalid_disc_table;

        // To navigate to the read from we take the remaing bytes from the address and add then to the new
        // address since they are both local to the same sector count
        uint64_t address_remainder = local_address % wbfs->wbfs_sector_size;
        uint64_t read_address = lookup_index * wbfs->wbfs_sector_size + address_remainder;
        fseek(wbfs->fp, read_address, SEEK_SET);

        // Calculate how much space is left within this sector, and if that is enough space to fill the
        // buffer. If there is not enough space to fill the buffer, we'll read the rest of this sector, and
        // move over to the next one
        uint64_t address_left = wbfs->wbfs_sector_size - address_remainder;
        uint64_t req_read_size = ((size - bytes_read) <= address_left) ? size - bytes_read : address_left;

        uint64_t read_size = fread((uint8_t*)(data) + bytes_read, sizeof(uint8_t), req_read_size, wbfs->fp);
        if (read_size != req_read_size) return e_wbfs_failed_file_read;
        bytes_read += read_size;
    }

    // Read al of the requested bytes without encountering an error.
    return e_wbfs_success;
}

wbfs_enum wbfs_disc_parse_partition_info(WiiDisc* disc, WiiDiscPartitionInfoEntry info[4])
{
    // The partition info starts at offset 0x4000 into the disc, so we can use the buffer reader
    wbfs_enum err = wbfs_disc_read_buffer(disc, info, 0x40000, 4 * sizeof(WiiDiscPartitionInfoEntry));

    for (uint32_t i = 0; i < 4; i++) {
        // reverse all the endianness
        wbfs_helper_reverse_endian_32(&info[i].offset);
        wbfs_helper_reverse_endian_32(&info[i].partition_count);

        // shift the partition offset
        info[i].offset = info[i].offset << 2;
    }

    return err;
}

wbfs_enum wbfs_disc_parse_partition_table(WiiDisc* disc, WiiDiscPartitionTableEntry* table, uint64_t address)
{
    wbfs_enum err = wbfs_disc_read_buffer(disc, table, address, sizeof(WiiDiscPartitionTableEntry));

    // reverse endianness and shift 
    wbfs_helper_reverse_endian_32(&table->type);
    wbfs_helper_reverse_endian_32(&table->offset);
    table->offset = table->offset << 2;

    return err;
}
