# Wii Sports Hacking : WBFS 

Wii sports is a particularly exciting game for me, it was a large part of my university experience. One year for my birthday I hosted a Wii Golf invitational, so I'd like to spend some time making one of my favourite games even more fun!

I think the most important thing to start with is how to understand the ROM files are structured, the most common is the WBFS. It's a form of archive file, it contains all of the assets inside the game plus the executable that makes up the game itself. These files can be ran on the Dolphin emulator, and on a real Wii using a loader. So to mod our game, it's a good idea to start here.

##  Header

Like all file formats, the WBFS file format has a header, it's the start of the file which tells the user all of the information required. Let's look at the structure of the header

```json
0x00 - 0x03 : Magic Number
0x04 - 0x07 : Sector Count
0x08 : Sector Size
0x09 : WBFS Sector Size
0x0A : WBFS File Version
0x0B : Padding
0x0C - 0x?? : Disc Table
```

Let's start with the `Magic Number`, This is a concept used in most files to verify that you're actually reading in the file type you expect. In this case we're looking for the string 'WBFS' which we can represent as one 32bit number.

```c
#define WBFS_MAGIC ('W' << 24 | 'B' << 16 | 'F' << 8 | 'S')
```

So a simple enough test would be reading the first 4 bytes of the header into a 32bit integer, so let's write a quick test!

```c
FILE* fp = fopen("test.wbfs", "rb");
uint32_t magic;
fread(&magic, sizeof(uint32_t), 1, fp);

if (magic == WBFS_MAGIC){ printf("MAGIC NUMBER VERIFIED"); }
else { printf("MAGIC NUMBER FAILED"); }
```

and as expected the output of this test is `>> MAGIC NUMBER FAILED` Wait what?! How are we already messing up the very first 4 bytes, well that leads me onto my next topic 

### Endianness

This refers to the order in which bytes are stored in memory, with Big Endian storing the most significant byte first at the lowest address, and Little Endian storing the most significant byte at the highest memory address for that object. So let's take a look at how our magic number would be stored in memory under these two different methods. 

| Endian | 0x0      | 0x1      | 0x2      | 0x3      |
| ------ | -------- | -------- | -------- | -------- |
| Big    | 0x57 (W) | 0x42 (B) | 0x46 (F) | 0x53 (S) |
| Little | 0x53 (S) | 0x46(F)  | 0x42 (B) | 0x57 (W) |

Performing a hex dump of the test WBFS file we can see that the data is stored in Big Endian!

```bash
$ hexdump -n4 -C test.wbfs
00000000  57 42 46 53   |WBFS|
```

Which is where the problem is coming from, as my x86_64 CPU is Little Endian! And so by thoughtlessly copying Big Endian data into a Little Endian interpretation, when the value of this `magic` is loaded into the CPU, it's bytes are in the wrong order.

So how do we get around this problem? Well we know which order the bytes are coming in, and we also know that the bit shift operator is not effected by Endianness. This is because the bit shift is performed on the *value* of the data, not its *binary representation*. So our new code will load the file into a byte array, and then perform the same bit shift we used to create the magic number.

```c
FILE* fp = fopen("test.wbfs", "rb");

uint8_t buffer[sizeof(uint32_t)];
fread(buffer, sizeof(uint32_t), 1, fp);

uint32_t magic = buffer[0] << 24 | buffer[1] << 16 
    | buffer[2] << 8 | buffer[3];

if (magic == WBFS_MAGIC){ printf("MAGIC NUMBER VERIFIED"); }
else { printf("MAGIC NUMBER FAILED"); }
```

And so finally as expected this gives us the `>> MAGIC NUMBER VERIFIED` we wanted!

### Defining The Header

So now that we know how to read the larger data types contained in the header, lets read in the entirety of the information, and then talk about what the different values mean. Note, only the first to fields in the header are larger than one byte, so once those are correctly aligned, the rest of the elements can be read directly.

```c
typedef struct WbfsHeader {
    uint32_t magic;
    uint32_t sectorCount;
    uint8_t  sectorSize;
    uint8_t  wbfs_sectorSize;
    uint8_t  wbfsVersion;
    uint8_t  padding;
    uint8_t* discTable;
} WbfsHeader;

uint32_t wbfs_parse_header(WbfsHeader* header, FILE* fp)
{
    fseek(fp, 0, SEEK_SET);
    uint8_t* buffer = header;
    fread(buffer, sizeof(WbfsHeader) - 1, 1, fp);
    
	// Ensure the correct endianness for the first two elements 
    header->magic = buffer[0] << 24 | buffer[1] << 16 | buffer[2] << 8 | buffer[3];
    header->sectorCount = buffer[4] << 24 | buffer[5] << 16 | buffer[6] << 8 | buffer[7];

    if (header->magic != WBFS_MAGIC) {
        printf("ERROR: Magic number didn't match!\n");
        return -1;
    }

    return 0;
}
```

- Magic - The magic number to ensure that the file we're reading really is a WBFS file.

- Sector Count - The number of sectors on the physical hard drive that the partition takes up.

- Sector Size - The size of the sectors on the physical hard drive hosting the partition. The actual value is this number raised to the power of 2. So this value is usually 9 corresponding to:
  $$
  2^9 = 512
  $$

- WBFS Sector Size - Sector sizes can be dynamic, this uses the same principal to calculate the value as the above sector size.

- WBFS Version - The version of WBFS being used, usually 0.

You might have noticed that all of this talk of sectors sounds less like a file type and more like a file system, and you'd be correct! WBFS is a way to partition a drive, and was used in the early days of Wii homebrew before FAT file systems were supported. 

### WBFS Sectors

WBFS has a dynamic sector size, so we need to extract the information relating to the WBFS sector size and count from the values in the header.

```c
// Sector sizes calculated as a power of 2
uint32_t hd_sectorSize = 1 << header.sectorSize;
uint32_t wbfs_sectorSize = 1 << header.wbfs_sectorSize;

// Size of the Wbfs partition in bytes
uint64_t wbfs_sizeInBytes = header.sectorCount * hd_sectorSize;
// Number of Wbfs sectors inside the disc.
// The value is rounded down to avoid overflowing the partition
uint32_t wbfs_sectorCount = wbfs_sizeInBytes / wbfs_sectorSize;
```

## Wii Disc

The WBFS format is specifically designed to store Wii Discs, one per section. The number of different discs that WBFS can store is usually attributed to be [500](https://gbatemp.net/threads/wbfs-format-1tb-wd-only-gets-500-games-limit.220966/). However this is actually based on how the WBFS was partitioned.

### Disc Table 

In fact, the WBFS header takes up one sector on the hosting hard drive. The header explained above is 12 bytes large. The rest of the header is then used to represent the disc table, at one byte per entry.  

Since the most common hard drive sector size is 512 bytes, most WBFS partitions can only host 500 different discs. However by repartitioning the partition to use larger sector sizes, you can in fact store more discs.

```c
// Now allocate enough space for the disc table
size_t discTableSize = hd_sectorSize - 12;
header.discTable = malloc(discTableSize);
if (!header.discTable) return -1;

// Read the disc table into memory
fseek(fp, 0x0C, SEEK_SET);
fread(header.discTable, discTableSize, 1, fp);
```

This is where reversing this gets a bit difficult. As the content we're trying to read only contains one disc, and as such there's not exactly much information here to go on. In fact, the only information contained here is the first byte, which I assume is the number of disks as the value is `0x01`

### Wii Disc Size

We know that we're going to be reading one Wii disc, it's probably a good idea to know how large that disc is likely to be. Well [we know](https://en.wikipedia.org/wiki/Nintendo_optical_discs#:~:text=The%20Wii%20Optical%20Disc%20) that the Wii uses a proprietary form of the DVD, which supports sizes of either 4.7GB or 8.54GB.

```c
#define GIGABYTE (1000 * 1000 * 1000)
#define DVD_SIZE_1 (4.7 * GIGABYTE)
#define DVD_SIZE_2 (8.54 * GIGABYTE)
```

Next we're going to find out how many sectors can be found within those discs, it seems once again according to the Dolphin source code that the Wii disc has a sector size of `0x8000`. Unfortunately I have been unable to find out where this information comes from. 

However we can Disc sizes by the sector size to find out how many sectors there are on a Wii disc. To avoid any in place rounding errors, I'm going to calculate the defines and then place their value in a define.

```c
#define WII_DISC_SECTOR_SIZE (0x8000)
#define WII_DISC_1_SECTOR_COUNT (143432)
#define WII_DISC_2_SECTOR_COUNT (260620)
```

These two different numbers come from the single sided discs and double sided discs. It seems Dolphin defines this as one number to be `2 * 143432`. My guess is that they're multiplying the single disc size by two for the double disc, and then fitting the single disc inside the double disc. 

I think this might actually be a little bit too big, since the Wiki claims that double sided disc is not double the size of the single disc. However I can disregard this, as Wii sports is a single sided disc.

The last bit of size information needed for now is how many WBFS partitions are needed to read the disc. In order to calculate this, we'll round up the division, when working with ints the easiest way to do this is to add (Denominator - 1) to the numerator:

```c
uint32_t wbfs_sectorsPerDisc =
      (WII_DISC_1_SECTOR_COUNT * WII_DISC_SECTOR_SIZE + 
      wbfs_sectorSize - 1) / wbfs_sectorSize;
```

### Wii Disc Information

Now that we've read the WBFS header in it's entirely, lets take a peak at the next sectors that are coming up. This is where we'll find the Wii Disc information.

The disc information is made up of 4 different parts:

| Addresses | Name                  |
| --------- | --------------------- |
| ??        | Disc Info Header      |
| ??        | Partition Information |
| ??        | Region Settings       |
| ??        | Disc Info Magic       |

I'll get to each of these sections as they come up, as it's not immediately clear how large they are. [Dolphin's source code](https://github.com/dolphin-emu/dolphin/blob/ac267a29405ae768037a8774b84b805a4180d1af/Source/Core/DiscIO/WbfsBlob.cpp#L23) seems to suggest that this disc info is 256 bytes, while [Wii brew](https://wiibrew.org/wiki/Wii_disc) suggests just the header alone is 1024 bytes. This is a mystery that will be solved later on.

### Wii Disc Header

For now let's start by reading in the next hard drive sector after the WBFS header. I know that the sector size on this WBFS partition is 512.

```bash
hexdump -C -s512 -n512 "./Wii Sports(EU).wbfs"

00000200    |RSPP01..........|
00000210    |........].......|
00000220    |SPORTS PACK for |
00000230    |REVOLUTION......|
00000240    |................|
*
000002e0    |............MD5#|
000002f0    |I..`.G..........|
00000300    |................|
00000310    |................|
*
000003f0    |................|
```

And with the majority of the next header being empty aside from some key points, this looks like we have another header here!  Searching that code at the top brings up results for Wii sports which sounds promising. In fact we're looking at the Wii disc header for wii sports.

| Address | Name                      | Value                        | Explanation of the value                                     |
| ------- | ------------------------- | ---------------------------- | ------------------------------------------------------------ |
| 0x00    | Disc ID                   | 'R'                          | Stands for Revolution which is the code name for the Wii, There are other options such as a Diagnostic disc which will boot automatically |
| 0x01    | Game Code                 | 'SP'                         | Sports Pack, the working title for Wii Sports                |
| 0x03    | Region Code               | 'P'                          | Pal which means European                                     |
| 0x04    | Maker Code                | '01'                         | I imagine this is reserved for Nintendo. Note this is the string '01' not the number 0x01 |
| 0x06    | Disc Number               | 0                            | For games that use more than one disk at a time ours does not |
| 0x07    | Disc Version              | 1                            | -                                                            |
| 0x08    | Use Audio Streaming       | 0                            | No Wii games use audio streaming so we can ignore this       |
| 0x09    | Streaming Buffer Size     | 0                            | Can ignore due to the above                                  |
| 0x18    | Wii Disc Magic Word       | 0x5D1C9EA3                   | Identifies the disc is a Wii disc, 0 0therwise. In this case it verifies that we have a wii disc |
| 0x1C    | GameCube Disc Magic Word  | 0                            | Identifies the disc as a GameCube disc, or 0. In this case the value is 0 |
| 0x20    | Game Title                | "SPORTS PACK FOR REVOLUTION" | -                                                            |
| 0x60    | Disable Hash Verification | 0                            | Disable hash verification when set to non zero, on retail consoles this disabled reading the disc |
| 0x61    | Disable Disc Encryption   | 0                            | Disable disc encryption when set to non zero, so once again that causes the disc read to fail on retail devices. |

### Wait Something's Wrong

So looking at this we have a lot of eyebrows raised. After this hard drive sector, there is a large section of bytes all set to 0. The other big red flag is the extra data which is not listed in the Wiki.

```
4d 44 35 23 49 0a b6 60  |MD5#I..`|
d2 47 88 13 e6 83 10 fc  |.G......|
```

So I tried to find the Disc Info Magic number again to see if there was another copy, and I did find the actual Wii disc header at address `0x20000` in the WBFS file. In this case it no longer has the extra information found in the previous example, and we can assume that this is where the Wii Disc header actually starts.

So what's going wrong here? Initially I thought this meant that the data was offset by the wbfs sector size instead of the hard drive sector size, but that is not the case!  In fact, our first hint was that extra data. And it's the reason we can't strip the WBFS header and boot it in Dolphin, the WBFS sectors are not necessarily in the correct order! 

### Wii Disc Partition Information

Once again we have to go another layer deep inside the partitions, as the Wii disc can contain a couple different partitions. For example a lot of games come with an extra partition to update the Wii's operating system, so that the system can be updated without an internet connection.

Thankfully Wii sports came out at the same time as the Wii, so we don't need to worry about that update partition. Let's have a go at reading the partition information, there are 4 entries in the partition information in the following form  

| Addresses         | Name                                                         |
| ----------------- | ------------------------------------------------------------ |
| 0x40000 - 0x40003 | Total number of partitions on the Wii Disc                   |
| 0x40004 - 0x40007 | Offset from the partition info to get the entry in the partition table |

For some reason the partition count is repeated in every single one of the entries, and can be optional. Let's take a look at the code now!

```c
typedef struct WiiDiscPartitionInfoEntry {
        uint32_t partitionCount;
        uint32_t offset;
} WiiDiscPartitionInfoEntry;
WiiDiscPartitionInfoEntry info_entry;

fseek(fp, wbfs_sectorSize + 0x40000, SEEK_SET);
fread(&info_entry, sizeof(info_entry), 1, fp);

wbfs_helper_reverse_endian_32(&info_entry.offset);
wbfs_helper_reverse_endian_32(&info_entry.partitionCount);
info_entry.offset = info_entry.offset << 2;
```

And after running this code we get the expected result of there being only 1 partition, and the partition table is located at address `0x40020` in the Wii Disc. Which in this case is immediately after the   partition information section. 

Now we can read the first entry in the actual partition table, This uses a similar pattern as the above code, but the structure definition is different. This is because the partition table entry needs to specify which type of partition it is 

```c
typedef struct WiiDiscPartitionTableEntry {
    uint32_t offset;
    uint32_t type;
} WiiDiscPartitionTableEntry;
WiiDiscPartitionTableEntry partition_table_entry;

fseek(fp, wbfs_sectorSize + info_entry.offset, SEEK_SET);
fread(&partition_table_entry, 
	sizeof(partition_table_entry), 1, fp);
   
wbfs_helper_reverse_endian_32(&partition_table_entry.type);
wbfs_helper_reverse_endian_32(&partition_table_entry.offset);

partition_table_entry.offset =
	partition_table_entry.offset << 2;
```

Finally after reading this we know that the first (and only partition) is of type 0 (meaning it contains game data) and that it starts at address `0xF800000`. Next we're going to cover actually reading that partition!

## Reading The Partition

After all this work, we've only found the start of the partition data, and we're going to do a bit more work to uncover the files contained in the partition. For example, the data in the partition is going to be encrypted and we're going to have to figure out how to decrypt it.

A Partition starts with ticket, a certificate chain and some meta data. The certificate chain is used to sign the save game data or the game files itself. Thankfully we won't need to look at that until we pack the files back up.

| Addresses     | Description                                        |
| ------------- | -------------------------------------------------- |
| 0x000 - 0x2A3 | Partition Ticket                                   |
| 0x2A4 - 0x2A7 | Meta data size                                     |
| 0x2A8 - 0x2AF | Certificate chain size                             |
| 0x2AC - 0x2B3 | Meta data offset, needs to be shifted by 2         |
| 0x2B0 - 0x2B7 | Certificate chain offset, needs to be shifted by 2 |
| 0x2B4 - 0x2AB | Offset to the fixed size hash table                |
| 0x2B8 - 0x2BB | Partition Data Offset                              |
| 0x2BC - 0x2BF | Partition Data Size                                |
| 0x2C0 - ????? | Meta Data                                          |
| ????? - ????? | Partition Data                                     |

Obviously the end goal is going to be the partition data, and as a result of this, we're not going to be interested in all of this. 

### Decryption Key

As we're working with Nintendo, we're going to need to decrypt the data, as a result we need the decryption key for this game. The key is stored inside the partition ticket, however that key itself is also encrypted.

The partition key is encrypted with a pair of common keys, all Wiis know one part of this key, and it is referred to as the common key. This common key was extracted from Wii consoles using a pair of tweezers, and the key itself can be found [here](https://hackmii.com/2008/04/keys-keys-keys/).  

The Partition ticket is pretty large, but we're only looking for 2 variables. The 16 byte long encrypted partition key at `0x1BF`, and the 16 byte long Initialization vector at `0x1DC`.

