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
        fprintf(stderr, "Failed when parsing the WBFS file header with the following reason :\n %s",
                wbfs_helper_enum_lookup(result));
    }
}
