// This file only has bitmaps related stuff for now from the alloc_blks.
#include "rubix_mkfs.h"
#include "rubix_file_system.h"

// This is a simple linear allocation of the free blocks. Maybe we should have freelist to have to serial block numbers but lets analyze the need for that later ? We might pribably do extents later.
// when i might have to allocate a bunch of free blocks for each extent which is contigous in logical space. That might be helpful as it ll end up contigous in the physical space too.
uint64_t alloc_dblock_from_bitmap(struct super_block *sb)
{
    int i,j,offset,k;
    uint64_t *bmap;
    rubix_sb_info_t *sbi = sb->s_fs_info;
    struct buffer_head *bh;

    for(i=0;i<sbi->map_arrays;i++)
    {
        // First find the suitable buffer head for and then find the free block there.
        bh = *(sbi->bbmap + i);

    printk("buffer_head1 :%p",bh);
        for(j=0;j<128;j++)
        {
           // THis is the free block bitmap. Check this to see if we have any free blocks
           // in the filesystem.
           // Each bmap stores 64 entries do check which ones the one.
           offset = j * 8;
           bmap = (uint64_t*)(bh->b_data + offset);

           BUG_ON(bmap == NULL);

           for(k=0;k<64;k++)
           {
               if(!((*bmap) & (1ULL <<k)))
               // find the first free block.
               {
                   // First free bit. Counting from the right to left.
                   (*bmap) |= (1ULL <<k);
                   i = (i * 8 * sb->s_blocksize) + (j * 64) + k;
                   printk("Returning :%llu\n", i + 2 * sbi->map_arrays + 4);
    printk("buffer_head2 :%p",bh);
                   mark_buffer_dirty(bh);
                   return (i + sbi->meta_data_blks);  // Still the allocation of free blocks is linear.
               }
           }
        }
    }
 
    return -1; // Global disk number.
}

uint64_t alloc_inode_from_bitmap(struct super_block *sb)
{
    int i,j,offset,k;
    uint64_t *ibmap;
    rubix_sb_info_t *sbi = sb->s_fs_info;
    struct buffer_head *bh;
    uint8_t *test_byte;

    for(i=0;i<sbi->map_arrays;i++)
    {
        bh = *(sbi->ibmap +i);
        test_byte = (uint8_t *)bh->b_data;
        printk("while allocating ..:%u \n",*test_byte);

        for(j=0;j<128;j++)
        {
           offset = j * 8;
           ibmap = (uint64_t*)(bh->b_data + offset);

           BUG_ON(ibmap == NULL);
           //FIXME:
           k=0;
           if(i == 0  && j == 0) k++;
           for(;k<64;k++)
           {
               if(!((*ibmap) & (1ULL <<k)))
               {
                   // First free bit. Counting from the right to left.
                   // the first free inode number.
                   (*ibmap) |= (1ULL <<k);
                   i = (i * 8 * sb->s_blocksize) + (j * 64) + k;
                   mark_buffer_dirty(bh);
printk("global_inode :%d \n",i);
                   return i ;  // Still the allocation of free inode is linear.
               }
           }
        }
    }
    return -1; // Global inode number.
}

/*
void alloc_from_log_append_point(struct super_block *sb)
{

    int i,j,offset,k;
    uint64_t *bmap;
    rubix_sb_info_t *sbi = sb->s_fs_info;
    struct buffer_head *bh;

    // append point is the current write pen saved in memory and written to the disk while unmounting. 
    // Allocating a page as we are writing pagewise.
    sbi->append_point = sbi->append_point + (PAGE_SIZE/SIZE_ONE_BLOCK);
 
    // I guess this not needed anymore ?*/

/*    mod = (block - (sbi->meta_data_blks)) % (8 * sb->s_blocksize);
    base = (block -  (sbi->meta_data_blks)) / (8 * sb->s_blocksize);
    BUG_ON(mod < 0);
    BUG_ON(base < 0);

    bh = *(sbi->bbmap + base);
    mod = mod %8;
    base = mod/64;
    bmap = (uint64_t *)(bh->b_data + (base * 8));
    
    BUG_ON((*bmap) & (1ULL << mod));
    (*bmap) |= (1ULL << mod);
    // CHeck if it has set.
    BUG_ON(!((*bmap) | (1ULL << mod)));
   
    mark_buffer_dirty(bh);*/
//}

void clear_inode_bitmap(struct super_block *sb, int ino)
{
    uint64_t mod,base;
    struct buffer_head *bh;
    rubix_sb_info_t *sbi = sb->s_fs_info;
    uint64_t *ibmap;

    mod = ino  % (8 * sb->s_blocksize);
    base = ino / (8 * sb->s_blocksize);
    bh = *(sbi->ibmap + base);
    mod = mod %8 ;
    base = mod/64;
    ibmap = (uint64_t *)(bh->b_data + (base * 8));
printk("mod:%llu \n",mod);
    // Should been set before this.
    BUG_ON(!((*ibmap) | (1ULL << mod)));
    (*ibmap) &= ~(1ULL << mod);
    // Has been cleared.
    BUG_ON((*ibmap) & (1ULL << mod));
   
    mark_buffer_dirty(bh);
    // ONly reading the values so no need to relse.
    //brelse(bh);
}

// why is this being called from vim edits. ?? 
void clear_dblock_bitmap(struct super_block *sb, uint64_t block)
{
    uint64_t mod,base;
    struct buffer_head *bh;
    rubix_sb_info_t *sbi = sb->s_fs_info;
    uint64_t *bmap;
 
    // Cannot be zero.
    BUG_ON(block == 0);

    mod = (block - (sbi->meta_data_blks)) % (8 * sb->s_blocksize);
    base = (block-  (sbi->meta_data_blks)) / (8 * sb->s_blocksize);
printk("block :%llu mods :%llu base:%llu \n",block,mod,base);
    BUG_ON(mod < 0);
    BUG_ON(base < 0);

    bh = *(sbi->bbmap + base);
    mod = mod %8;
    base = mod/64;
    printk("buffer_head3 :%p",bh);
    bmap = (uint64_t *)(bh->b_data + (base * 8));
    printk("bmap :%llu",*bmap);
    
    BUG_ON(!((*bmap) | (1ULL << mod)));
    (*bmap) &= ~(1ULL << mod);
    // CHeck if it has cleared.
    BUG_ON((*bmap) & (1ULL << mod));
   
 printk("bmap :%llu",*bmap);
    mark_buffer_dirty(bh);
}

// Clearing all the indirect blocks for the inode.
void clear_indirect_blocks(struct super_block *sb, uint64_t block)
{
    struct buffer_head *bh ,*bh1, *bh2;

    bh = sb_bread(sb, block);
    // Reset the bh->b_data.
    memset(bh->b_data,0,1024);
    mark_buffer_dirty(bh);
    brelse(bh);

    bh1 = sb_bread(sb, block + 1);
    // Reset the bh->b_data.
    memset(bh1->b_data,0,1024);
    mark_buffer_dirty(bh1);
    brelse(bh1);

    bh2 = sb_bread(sb, block + 2);
    // Reset the bh->b_data.
    memset(bh2->b_data,0,1024);
    mark_buffer_dirty(bh2);
    brelse(bh2);
}

// create the block allocation chain here.
// This is common code called for allocation and mapping(map_bh)
// Case 1: In case of allocation parse through the chain to check for allocation and if not present allocate the parent blocks.
// Case 2: In case of mapping(map_bh) we BUG_ON each component from the chain parser to make sure we have all the parents and the allocated block and then map_bh the value.
/*
int create_chain(int *chain, int blk_num, int *len)
{

    int i=0,j;
    
    // Direct block mapping
    if(blk_num < IDX_INDIRECT_BLK)
    {
        // Done with chain building. 
        chain[i++] = blk_num;
        *len = i; 
        return 0;
    }
    else
    {
        chain[i++] = IDX_INDIRECT_BLK;// MAx for 1st indirect.
        blk_num -= IDX_INDIRECT_BLK;
    
        if(blk_num < IDX_INDIRECT_BLK2)     // First level of indirection.
        {
            // Done with chain building. 
            chain[i++] = blk_num;
            *len = i; 
            return 0;
        }
        else
        {
            chain[i++] = IDX_INDIRECT_BLK2;// MAx for 1st indirect.
            blk_num -= IDX_INDIRECT_BLK2;
    
            if(blk_num < IDX_INDIRECT_BLK3)     // First level of indirection.
            {
               // Done with chain building.
               chain[i++] = blk_num;
               *len = i; 
               return 0;
            }
            else// Third level which is the last for now.THis is the last supporting 512 * 512 * 525 * 1k
            {
                chain[i++] = IDX_INDIRECT_BLK3; 
                chain[i++] = blk_num- IDX_INDIRECT_BLK3;
                if(blk_num < END_LIMIT)
                   return -ENOMEM;
            }
        }
   }
   *len = i; 
   return 0;
}*/
 

