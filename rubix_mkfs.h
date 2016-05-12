#ifndef __RUBIX_MKFS__
#define __RUBIX_MKFS__
// This is weird but let it be for now.
typedef unsigned int uint32_t;
typedef unsigned short uint16_t;
typedef unsigned char  uint8_t;
typedef unsigned long long uint64_t;

#define NUM_ENTRY_PER_BLK    
#define NUM_BLKS_PER_EB      20 * 1024 //  20MB blocks 
#define SIZE_ONE_BLOCK       1024 // One block is 1024 bytes here.
#define NUM_BLKS_PER_INODE   12 // Number of data blocks per inode[0-19 ]INdx
#define IDX_INDIRECT_BLK    8 // First one
#define IDX_INDIRECT_BLK2  520 // 2nd one After this we are going into 3rd level indirection. 
#define IDX_INDIRECT_BLK3  ((512 * 512 + 520)) // Roughly thats the third block number IDX.
#define NUM_INODES_FS        16 // This means the number of files/directories in the tree
#define SIZE_ON_DSK_INODE    128 // Can change this if we really need more.   
#define NUM_INODES_PER_BLK  (SIZE_ONE_BLOCK/SIZE_ON_DSK_INODE) 
#define SIZE_FS_BLKS NUM_BLKS_PER_INODE * NUM_INODES_FS
#define RUBIX_FILE_LEN       10
#define ROOT 0 
#define INODE_INDEX          3
#define IBMAP_INDEX          1
#define BBMAP_INDEX          2
#define TEST 1 
#define DUMPCODE 1
#define kassert(expr)       BUG_ON(!(expr))
#define CHECKPOINT          0

//FYI  SO we have 16 inodes which take one block in the structure and each inode can store upto 64
// blocks of data which means we support a filesystem of size only 1M so far. 

typedef struct _dir_entry
{
    uint32_t          inode;                  /* Inode number */
    uint16_t          rec_len;                /* Directory entry length */
    uint16_t          name_len;               /* Name length */
    char              name[RUBIX_FILE_LEN];    /* File name */
    uint8_t           file_type;
}rubix_dir_entry_t; 
// Lets keep the rubix_inode for the sake of data blocks
typedef struct _rubix_inode
{
    uint16_t          i_mode;
    uint16_t          i_uid;
    uint16_t          i_gid;
    uint16_t          i_nlinks;
    uint32_t          i_atime;
    uint32_t          i_mtime;

    uint64_t          i_size;
    uint64_t          dblocks[NUM_BLKS_PER_INODE];
    uint32_t          i_blocks;   // NUmber of data blocks in this inode.
    uint32_t          i_ctime;
}rubix_inode_t;

typedef struct _assert_test
{
    uint16_t          i_mode;
    uint16_t          i_uid;
    uint16_t          i_size;
    
    uint16_t          i_gid;
    long              mytime[10];
    uint16_t          i_nlinks;
    uint16_t          i_blocks;   // NUmber of data blocks in this inode. 
  // uint8_t           dblocks[NUM_BLKS_PER_INODE];

}test_assert_t;

// This is the least block which will be used to clean up for the groomer. 
typedef struct _erase_block_
{
   uint64_t eb_num;  /// This will distinctly tell the number on the drive. We then calculate the page_num and blk based on that. 
   uint64_t free_blks;
   uint64_t start_blk;
   uint64_t end_blk;
   uint64_t cur_blk; /// We are still writing only blk wise.
   uint64_t LAST_EB; // For now this is the end. When we groom we keep updating it as required.
}EB;

typedef struct __super_block__
{    
   uint64_t  free_blocks; /// Initially the size of the disk in terms of blocks.
   uint64_t  num_arrays; 
   // Deal in only erase blocks instead of blocks in this filesystem.
   EB        *append_point; ///append point for the written log.
}super_block_t;

typedef struct __inode_entry__
{
   uint64_t blk;    // Latest blk for the rubix_inode_t entry on the disk.
   uint64_t offset; // The offset into the data blk.

}inode_entry;

typedef struct __inode_table__
{
    inode_entry **i_ent;
}inode_table;
//extern void make_file_system_init(struct super_block *sb);
#endif
