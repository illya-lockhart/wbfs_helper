#include <stdlib.h>
#include <string.h>
#include "wbfs.h"

// Error loggers based on how many arguments get parsed to the format string
#define ERROR_EXIT_0(MSG)      \
    fprintf(stderr, MSG "\n"); \
    return -1;
#define ERROR_EXIT_1(MSG, FORMAT)          \
    fprintf(stderr, MSG "\n%s\n", FORMAT); \
    return -1;

int main(int argc, char* argv[])
{
    // Ensure that the args recieved are valid
    if (argc < 2) {
        ERROR_EXIT_0("Argument count smaller than 2, did not recieve a file path\n");
    }

    // Look for the passed in file, * not safe *
    FILE* fp = fopen(argv[1], "rb");
    if (!fp) {
        ERROR_EXIT_1("Could not open file", argv[1]);
    }

    // Make space for the handle representing the WBFS and the file header and then read in the raw bytes
    // representing the header
    Wbfs wbfs_handle;
    WbfsFileHeader wbfs_file_header;
    wbfs_enum result = wbfs_file_header_parse(&wbfs_handle, &wbfs_file_header, fp);
    if (result != e_wbfs_success) {
        ERROR_EXIT_1("Failed when parsing the WBFS file header with the following reason",
                     wbfs_helper_enum_lookup(result));
    }

    // Allocate enough space for the disc table and then read in the disc table
    wbfs_handle.file_header->disc_table = malloc(wbfs_helper_disc_table_size(&wbfs_handle));
    if (!wbfs_handle.file_header->disc_table) {
        ERROR_EXIT_0("Failed to allocate space for disc table");
    }
    wbfs_file_disc_table_parse(&wbfs_handle);

    // Now that we've read the disc table, loop through all of the discs and parse them
    for (uint8_t i = 0; i < wbfs_handle.wii_disc_count; i++) {
        printf(" * Opening Disc %d\n", i);

        // Initalise the disc by finding out where it starts
        WiiDisc disc;
        memset(&disc, 0, sizeof(WiiDisc));
        wbfs_disc_get_offset(&disc, &wbfs_handle, i);

        // Allocate space for the sector look up table for this disc
        disc.wbfs_sector_lookup = malloc(wbfs_helper_sector_table_size(&wbfs_handle));
        if (!disc.wbfs_sector_lookup) {
            ERROR_EXIT_0("Failed to allocate space for this disc's sector lookup table\n");
        }
        wbfs_disc_parse_sector_table(&disc);

        // Once we have the sector table parsed we can now read any part of the disc
        // Read the partition information, which tells us where the partition tables are located
        WiiDiscPartitionInfoEntry partition_info[4];
        wbfs_disc_parse_partition_info(&disc, partition_info);
        printf(" * Found %d partitions\n", partition_info[0].partition_count);

        for (uint32_t j = 0; j < partition_info[0].partition_count; j++) {
            WiiDiscPartitionTableEntry partition_table_entry;
            wbfs_disc_parse_partition_table(&disc, &partition_table_entry, partition_info[i].offset);
            printf(" * partition %d starts at 0x%08x\n", j, partition_table_entry.offset);

            uint8_t encrypted_part_key[16];
            uint8_t init_vector[16];
            wbfs_disc_read_buffer(&disc, encrypted_part_key, partition_table_entry.offset + 0x1bf, 16);
            wbfs_disc_read_buffer(&disc, init_vector, partition_table_entry.offset + 0x1dc, 16);
        }
    }
}
