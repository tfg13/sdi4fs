/*
 * File:   Constants.inc
 * Author: Tobias Fleig <tobifleig@gmail.com>
 *
 * Created on August 16, 2015, 4:14 PM
 */

#ifndef SDI4FS_CONSTANTS_INC
#define	SDI4FS_CONSTANTS_INC

#define SDI4FS_MAGIC 1561654773 // sdi4fs magic is SDI4FSFS in hex (0x5D14F5F5 = 1.561.654.773)
#define SDI4FS_HEADER_SIZE 4096 // 1 block, because its simple, can be changed
#define SDI4FS_BLOCK_SIZE 4096 // must not be changed!
#define SDI4FS_MAX_NUMBER_OF_BLOCKS 4294967295 // 2^32 - 1 (block ids are 32 bit, zero is empty/invalid)
#define SDI4FS_FS_MIN_SIZE 16384 // 4 blocks: 1 block header, ~1block bmap, 2 blocks (root dir + max 1 file (both with inlined data))
#define SDI4FS_FS_MAX_SIZE 17609365913596 // ~ 16,02 TiB (1block header + bmap for (2^32 -1 blocks = 17.179.869.180) + 2^32 - 1 blocks in log)
#define SDI4FS_MAX_LINKS_PER_DIRENTRYLIST 127 //how many hardlinks per directory entry block (each is 32 bytes, block has 8 bytes overhead: (4K - 8B) / 32B = 127,...)
#define SDI4FS_MAX_LINK_NAME_LENGTH 28 // 8 bytes (includes 1 char for /0, this is saved as a c string)
#define SDI4FS_MAX_DIRENTRYLISTS_PER_DIR 1019 // 4076B after inode header, 4B per entry
#define SDI4FS_MAX_HARDLINKS_PER_DIR 129413 // 127 links per entry block * 1019 entry blocks per INode
#define SDI4FS_MAX_BYTES_PER_DATABLOCK 4088 // 4096B block size - 8B block header
#define SDI4FS_MAX_BYTES_PER_INODE 4076 // 4096B - 20B INode header
#define SDI4FS_MAX_DATABLOCKLISTS_PER_FILE 1019 // 4076B after inode header, 4B per entry
#define SDI4FS_MAX_DATABLOCKS_PER_DATABLOCKLIST 1022 // 4088B after block header, 4B per entry
#define SDI4FS_MAX_DATABLOCKS_PER_FILE 1041418 // 1019 list * 1022 entries per list
#define SDI4FS_MAX_FILE_SIZE 4257316784 // 1019 * 1022 (see above) * 4088B raw data after block header (=3.96GiB)
#define SDI4FS_MAX_NUMBER_OF_LINKS_TO_INODE 65535 // 2^16 - 1 (field in INode header is uint16_t)

#endif	// SDI4FS_CONSTANTS_INC
