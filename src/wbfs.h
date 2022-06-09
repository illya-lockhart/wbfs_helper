/*************************************************************************************************************
 * Based on the work done by the Wiimms iso tools repository : https://github.com/Wiimm/wiimms-iso-tools
 * I'm trying to further understand the wbfs file system, as I want to fully understand every step of the
 * process. I also looked at the dolphin emulator code as a strong basis as well.
 *
 * Author : Illy L
 * License : None
 *************************************************************************************************************/
#ifndef __WFBS_H__
#define __WFBS_H__ (1)
#define WBFS_MAGIC ('W' << 24 | 'B' << 16 | 'F' << 8 | 'S')
#include <stdint.h>
#include <stdio.h>

/*************************************************************************************************************
 * Structure definitions
 *************************************************************************************************************/

/**
 * Start with the header, as this is the first thing that gets read. This corresponds directly to the bytes
 * inside the header, the rest of the information gets extracted into the Wbfs struct.
 * This structure takes up the first sector on the WBFS file, and so the size is dependant on the sector size.
 * This is why the disc table is dynamically sized, as the size is sector size - 12 bytes.
 *
 * The disc table points the user towards the internal Wii discs, although almost all implementations use 512
 * as the sector size, and almost always contain 1 disc only.
 */
typedef struct WbfsFileHeader {
    uint32_t magic;             // Magic number "WBFS"
    uint32_t hd_sector_count;   // How many sectors on the host partition this file needs
    uint8_t hd_sector_shift;    // 2 ^ shift is the size of the sectors on host partition
    uint8_t wbfs_sector_shift;  // 2 ^ shift size of sectors within this Wbfs file
    uint8_t wbfs_version;       // Version number always 0
    uint8_t padding;            // Padding
    uint8_t* disc_table;  // Lists all the Wii discs inside the partition, can store up to hd_sector_size - 12
} WbfsFileHeader;

/**
 * Define the Wbfs struct, this keeps track of all the information extracted from the header, such as tracking
 * the file pointer and the constants that remain the same between discs
 */
typedef struct Wbfs {
    FILE* fp;                     // The underlying wbfs file
    WbfsFileHeader* file_header;  // Interpret the bytes of Wbfs header

    // Variables extracted from the binary file header
    uint64_t wbfs_file_size;         // How large the WBFS file is in bytes
    uint64_t wbfs_sector_size;       // How large the WBFS sectors are
    uint64_t hd_sector_size;         // How large the host hard drive sectors are
    uint8_t wii_disc_count;          // How many Wii disks are in the file
    uint32_t wbfs_sectors_per_disc;  // How many wbfs sectors a Wii disc takes up

    // Repeate the WBFS magic at the end of the struct, this is how we'll ensure the struct we've recieved is
    // propperly allocated
    uint32_t valid;
} Wbfs;

/**
 * Define the Wii disc struct. Although the wbfs contains multiple wii disc, we don't store an array of wii
 * discs as it will be more memory efficient to parse one wii disc at a time and then make the disc hold a
 * reference to the Wbfs. There are other reasons to do this, for example the wbfs address lookup changes
 * based on the current disc being searched
 */
typedef struct WiiDisc {
    Wbfs* wbfs;                    // Pointer to the parent wbfs container
    uint16_t* wbfs_sector_lookup;  // Wbfs sectors aren't guarenteed to be in the correct order
} WiiDisc;

/*************************************************************************************************************
 * Enums for return codes
 *************************************************************************************************************/
typedef enum wbfs_enum {
    e_wbfs_success,
    e_wbfs_segfault,
    e_wbfs_magic_fail_wbfs,
    e_wbfs_invalid_handle,
    e_wbfs_invalid_disc_table,
} wbfs_enum;
/*************************************************************************************************************
 * Functions that do a large portion of the work. None of these functions should ever allocate memory, this is
 * left up to the user to do.
 *
 * Most of these functions should return an enum specifying a sucess code
 *************************************************************************************************************/

/**
 * @brief Reads in the first part of the WBFS file and initalises the data inside the wbfs struct, and stores
 * the pointers the user parsed. This is because this is expected to be the first call in the library, from
 * there users can just pass the handle to library functions. Note that this does not allocate space for the
 * disc table
 * @returns error code, 0 on success
 * @param wbfs_handle Handle to be filled with data from the wbfs file
 * @param wbfs_fh Memory backing the file header
 * @param fp File Pointer to the WBFS, has to be in RB mode
 */
wbfs_enum wbfs_file_header_parse(Wbfs* wbfs_handle, WbfsFileHeader* wbfs_fh, FILE* fp);

/**
 * @brief Once the user has allocated space for the disc table to be read in, and the file pointer has been
 * properly been read, this will read in the disc table
 * @returns error code, 0 on success
 * @param wbfs Pointer to the WBFS handle
 */
wbfs_enum wbfs_file_disc_table_parse(Wbfs* wbfs);

/*************************************************************************************************************
 * Helper functions that don't have a return type, do something simple
 *************************************************************************************************************/

/**
 * WBFS stores the data as Big endian, most PCs are little endian, most PCs are little endian. Provide a
 * function that swaps the endianness of a 32 bit integer
 * @param d The data to be reversed
 */
void wbfs_helper_reverse_endian_32(uint32_t* d);

/**
 * @brief Fetches the wbfs disc table size, added as users might not know how large the header should be
 * @returns Size of the Wbfs disc table in bytes, most likely 500 or 0 on error
 * @param wbfs Valid Wbfs handle
 */
size_t wbfs_helper_disc_table_size(Wbfs* wbfs);

size_t wbfs_sector_table_size(Wbfs* wbfs);

const char* wbfs_helper_enum_lookup(wbfs_enum e);
#endif  // !__WFBS_H__
