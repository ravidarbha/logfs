#include "rubix_file_system.h"

// FIXME: This will be extended now to improve the performance. Its too bad.
//  Get the design for storing the data blocks.

//FIXME: the other patch is the temp fix for current repo to work. THis patch will overwrite the changes later.
///CURRENT_STATUS :Initial snapshotting seems to work with some tweaks to the code. But we need to simplify things. Recreating the tree can be avoided if we save the tree on the disk and can be retrieved quicker.
/// Also the new thread has to overwrite the in_mem->dblocks and for the commits so that crashes can be recovered. We are just going to have pages and then will commit these pages by mapping to a bio which will be sent to the block layer directly instead of using the VFS.
////FIXME: The next thing we are fixing here are the removal to buffer head structures competely.
// Current status: The performance is really bad as there are just too many references to the VFS in the code. Try to get rid of buffer heads and deal with submit_bio directly. For that we need a correct mapping between the Pages in the mapping of the inodes.How is this guy updated ? based on the LBa of the file. ??
// How abt having just a page cache/ page buffer instead of doing buffer heads, and then do the sync filesystem whenever necessary with a new thread.
//     :Dont include definitions of structures in the header files. Always include in the c files.
//TODO :try some test cases with the reads and writes. Verify if data if being written and read correctly.
//     : before we get onto log we need to do some high intensive testing on the existing number.
//     : For this the filesystem should be increased in capacity. This will enable us to create and test
//     : more than 16 direntrys which would require more than one block reads/mapping in all direntry functions.
//     : 
//     :Get the design of the log structure ready - I am getting the initial design.
//TODO: These will be handled later
/// Optimizations will be done later on- Getting the ondisk bitmap for inodes and data blocks into memory.
//// TODO:Revisit the bitmap for th blocks - WE need to make sure we set all the blocks for the initial allocation
//// as used. If we dont we might have a problem while writing as it might overwrite on the the initial blocks.
////TODO : revisit bitmaps - Make them per bit instead of byte and save some space.
///UPDATE: the writes to the filesystem are working when the system is up.the updated values are correct wrt to the inodes
/// 
struct btree_node_t;
struct inode *rubix_iget(struct super_block *sb, unsigned long ino , int mode);
int rubix_getblock(struct inode *inode, sector_t block, struct buffer_head *out, int create);
int rubix_getblock_read(struct inode *inode, sector_t block, struct buffer_head *out, int create);
void dump_all_entry(struct inode *dir);
void reset_inode(struct inode *inode);
int read_page_from_disk(struct file *file, struct page *page);

// Get all the externs from files.
extern struct file_operations rubix_file_fops;
extern struct inode_operations rubix_file_iops;
extern struct file_operations rubix_dir_fops;
extern struct inode_operations rubix_dir_iops;

extern void *init_worker_threads(void);
extern void destroy_worker_threads(void *rbx);
extern int alloc_dblock_from_bitmap(struct super_block *sb) ;//buffer_head *bh);
extern btree_node_t *insert_btree_node(btree_node_t *root,loff_t pos, uint64_t pba);
extern btree_node_t *search_write_modification(btree_node_t *root,loff_t pos, uint64_t pba);
extern void print_btree(btree_node_t *root);
extern btree_node_t *search_node(btree_node_t *root, loff_t pos , uint64_t *);
int commit_page(struct page *page, struct writeback_control *wbc);

// Definitions of the structures- THis will be split into multiple files later based on files and directories.

//// FUNCTION POINTERS DEFINITION.
struct file_system_type rubix_fs_type = 
{
    .name = "rubix",
    .mount = rubix_mount,
    .kill_sb = rubix_kill_super,
    .fs_flags = FS_REQUIRES_DEV, 
    .owner = THIS_MODULE, 
};

// this will be the block number of the append point and the new data will always be written to this.

/*struct typedef __append_point_
{
   uint64_t current_pba;
   uint64_t offset;
}append_point;*/

struct super_operations rubix_sop = 
{
    .alloc_inode = rubix_sb_alloc_inode,//(same as create_inode) 
    .write_inode = rubix_sb_write_inode,
    .destroy_inode = rubix_sb_destroy_inode,
    .evict_inode = rubix_sb_evict_inode,
    .put_super = rubix_put_super, 
   // .lookup = rubix_sb_lookup_inode;
      //read_inode = rubix_sb_read_inode,
};


// These address operations are going to be called by the buffered reads 
// and writes which are going to use page cache.If we use DIRECT IO we dont
// call these. We do not support them for now.
struct address_space_operations rubix_asops =
{
    .readpage = rubix_read_asop_page, 
    .writepage = rubix_write_asop_page,  // This wont be doing the actual writing.
   .write_begin = rubix_prepare_write_asop_page, // This does the actual writing // prepare_write
    .write_end = generic_write_end,// rubix_write_end,//rubix_commit_asop_page,  // commitwrite -- we may not need the commit
   // as we already writing into the page cache from the user buffer after the write_begin.
   // we should already have the data in the mapped buffer bh.
};

// This returns the container for the vfs_inode 
rubix_inmem_t *rubix_create_inmem(struct inode *inode)
{
    return container_of(inode, rubix_inmem_t ,vfs_inode);
}

// Looks like we dont need this anymore. ??
void *get_block_data_from_bh(int blk)
{
    return NULL;
}

// return the idx for the bh array.
int get_buffer_head_idx(long int ino, uint64_t *mod)
{
    int base;

    *mod = ino % (NUM_INODES_PER_BLK);
    base = ino /(NUM_INODES_PER_BLK);
    return base;
}

//inode_table[NUM_INODES]

/*inode_entry *get_inode_entry_from_table(inode_table *i_table, struct inode *inode)
{
   struct buffer_head *bh;
   offset = inode->i_ino * sizeof(inode_entry);
   base = inode->i_ino / 64;
   mod =  inode->i_ino % 64;
 
   bh =  sb_bread(base);
   offset = mod * sizeof(table_entry);
   i_en  = (inode_entry *)(bh->b_data + offset);

   return i_en; 
}*/

// Returns the ondisk structure for the given inode.
rubix_inode_t *get_rubix_inode_from_vfs(struct inode *inode, struct buffer_head **bh)
{
    rubix_inode_t *rubix_inode;
    struct super_block *sb = inode->i_sb;
    rubix_sb_info_t *sbi = sb->s_fs_info;
    inode_entry *i_en;

    // Current location of the rubix_inode on the disk.
    i_en = sbi->i_table->i_ent[inode->i_ino];
    *bh = sb_bread(sb, i_en->blk);
   
    rubix_inode = (rubix_inode_t *)((*bh)->b_data + i_en->offset);
    /*offset = inode->i_ino * sizeof(rubix_inode_t);
    base = offset / (NUM_INODES_PER_BLK);
    mod = offset % (NUM_INODES_PER_BLK);

    *bh = sb_bread(sb, base);
    rubix_inode = (rubix_inode_t *)((*bh)->b_data + offset);
*/
    // check the size we are actually writing.
/*    blk =  (BBMAP_INDEX + 2 * sbi->map_arrays);
    BUG_ON(sb->s_bdev == NULL);
    base = get_buffer_head_idx(inode->i_ino, &mod);
    blk = blk + base;
    *bh = sb_bread(sb ,blk);

    offset = mod * sizeof(rubix_inode_t);
    rubix_inode = (rubix_inode_t *)((*bh)->b_data + offset);

printk("Rubix_inode :%p  :%lu\n",rubix_inode,inode->i_ino);
    */return rubix_inode;
}

void inode_init_function(void *rubix)
{
    rubix_inmem_t *rin = (rubix_inmem_t *)rubix;

    if(rin)
    {
        inode_init_once(&rin->vfs_inode);
    }
//#if CHECKPOINT
    rin->btree_root = NULL; //init the rin to make it usable later.
//#endif
}

// SLAB for the inodes.
struct kmem_cache *inode_cache= NULL;

int create_slab_cache(void)
{
    // Get a cache alligned slab for the inodes.
    inode_cache = (struct kmem_cache *)kmem_cache_create("RUBIX_INODE_CACHE",sizeof(rubix_inmem_t ),0,
                   SLAB_HWCACHE_ALIGN ,inode_init_function);

    if(inode_cache == NULL ) return -ENOMEM;

    return 0;
}

void destroy_slab_cache(void)
{
    kmem_cache_destroy(inode_cache);
}

rubix_inmem_t *allocate_from_slab(void)
{
    rubix_inmem_t *ptr = NULL;

    ptr = kmem_cache_alloc(inode_cache,GFP_KERNEL);
   
    return ptr;
}

void free_to_slab(rubix_inmem_t *in)
{
    memset(in, 0, sizeof(*in));
    // Reset the values to make this inode reusable.Guess VFS doesnt do that.
    inode_init_once(&in->vfs_inode);
    kmem_cache_free(inode_cache, in);
}

// Called inside transaction so no more FS used.
struct inode *rubix_sb_alloc_inode(struct super_block *sb)
{
    // Should allocate the inmemry structure completely from slab.
    rubix_inmem_t *inode_info;
    inode_info = allocate_from_slab();

    return &inode_info->vfs_inode;
}
 
void rubix_destroy_inode(struct inode *inode)
{
    // Free the whole structure.
    rubix_inmem_t *raw_inode = rubix_create_inmem(inode);
    free_to_slab(raw_inode);
}
 
struct inode* link_to_ondisk(struct inode *inode)
{
    return rubix_sb_read_inode(inode);
}

void rubix_sb_evict_inode(struct inode *inode)
{
    // Add a truncate function which removes all the blocks of the data
    // and then clears them. Need to add more logic when we expand the 
    // data block/s.
    if (inode->i_data.nrpages)
    {
        truncate_inode_pages(&inode->i_data, 0);
    }
    invalidate_inode_buffers(inode);
    clear_inode(inode);
}


// Just the basic requirements of an inode.
struct inode *rubix_iget(struct super_block *sb, unsigned long ino ,int mode)
{

    struct inode *inode;
    //int ret=0;
 
#if 0
    if((ret = search_hash(ino)))
    {
       printk("DUMPING ..\n");
       dump_hash();
       kassert(0);
    }
    build_hash(ino);
#endif
    inode = iget_locked(sb, ino);

    if(inode == NULL)
    { 
       return NULL;
    }
  
    if(S_ISREG(mode))
    {
        inode->i_op = &rubix_file_iops;
        inode->i_fop = &rubix_file_fops;
    }
    else if(S_ISDIR(mode))
    {
        inode->i_op = &rubix_dir_iops;
        inode->i_fop = &rubix_dir_fops;
    }
    inode->i_mapping->a_ops = &rubix_asops;
    inode->i_sb = sb;
    inode->i_ino = ino;

    // Link the ondisk and the inmemory.
    inode = link_to_ondisk(inode);

    // Set the imode as is from the VFS.
    inode->i_mode = inode->i_mode | mode;  // Making sure we set the right mode

    unlock_new_inode(inode);

    return inode;
}

// This creates the root inode and returns it back to the VFS.
int rubix_fill_super(struct super_block *sb, void *data , int silent)
{
    struct inode *root = NULL;
    rubix_sb_info_t *sbi;
    super_block_t *sbd= NULL;
    struct buffer_head *bh;
    uint64_t offset,base;
    inode_table *i_table = NULL;
    int i;

    sbi = kzalloc(sizeof(*sbi), GFP_KERNEL);
    sb->s_fs_info = sbi;
    sb->s_magic = MAGIC;
    // Read the super block from the disk .
    // we will be reading only super block in this case
    sbi->sb = sb_bread(sb,0);

    sbd = (super_block_t *)(sbi->sb->b_data);
    BUG_ON(sbd == NULL);

    i=0;
    base=1;
    bh = sb_bread(sb, base);
    offset = 0;
    i_table = sbi->i_table;

    if(!i_table)
    {
       //If i_table doesnt exist then allocate if first.
        i_table = kmalloc(sizeof(inode_table), GFP_KERNEL);
    }

    // fill up the table with entries from the disk. Then we use this inmem structure.
    while(1)
    {
        inode_entry *i_en;
        // ALlocate the entry for table entry
        i_table->i_ent[i] = kmalloc(sizeof(inode_entry) ,GFP_KERNEL);
        i_en =  (inode_entry *)(bh->b_data + offset);
        // Add it to the table of entries.
        i_table->i_ent[i]->blk = i_en->blk;
        i_table->i_ent[i]->offset = i_en->offset;
        offset+= sizeof(inode_entry);
        i++;
        // This will be used when we have more than 64 inodes in the FS.
        if((i % (SIZE_ONE_BLOCK /(sizeof(inode_entry)))) == 0)
        {
            bh = sb_bread(sb, base++);
        }
        if(i > NUM_INODES_FS) break;
    }

     // FIXME:We will leave this for now as we might need it for validity bitmaps.
     
    /* 
    i=0;
    // Read the inode bitmap block from the disk . Max number of inodes allowed on this filesystem are the    // total number of disk blocks as each inode atleast has one data block.
    // Pointing to an array of buffer head pointers.
    sbi->ibmap = kmalloc(num_blks * sizeof(uint64_t),GFP_KERNEL);
    while (i < num_blks)
    {
        // Read the i+1 block for the bbmap.
        *(sbi->ibmap + i) = sb_bread(sb, i + 1);
        bh = *(sbi->ibmap + i);
        test_byte = (uint8_t *)bh->b_data;
        mark_buffer_dirty(*(sbi->ibmap + i));  // Thats the buffer
        i++;
    }

    // Read all the block bitmaps next into memory.
    i=0;
 
    sbi->bbmap = kmalloc(num_blks * sizeof(uint64_t),GFP_KERNEL);
    while (i < num_blks)
    {
        // Read the i+1 block for the bbmap.
        *(sbi->bbmap + i) = sb_bread(sb, num_blks + i + 1 );
        mark_buffer_dirty(*(sbi->bbmap + i));
        i++;
    }

    sbi->map_arrays = num_blks;
    // For now FIXME:
    sbi->meta_data_blks = 2 *num_blks + 4;*/
    //## FIXME:We do not need to keep the bitmaps. We can just remove them from here.

    // Copy it into the inmem structrure so that you can just update this everytime.
    memcpy(sbi->append_point, sbd->append_point , sizeof(EB));
    
    // Then init the structures and create the root inode.
    sb->s_op = &rubix_sop;
    sb->s_type = &rubix_fs_type;
    sb_set_blocksize(sb, SIZE_ONE_BLOCK);
    // Its a directory.
    root =  rubix_iget(sb, ROOT ,S_IFDIR);
    // Root cannot be empty.
    BUG_ON(root == NULL);
    // Since this is the directory.
    sb->s_root = d_make_root(root);
    BUG_ON(sb->s_root == NULL);
    root->i_blkbits = 10;
    root->i_bytes = 1024;  // This is the size of the blocks

    // Just be sure we are fine till here with some BUG_ONs.
    BUG_ON(sb->s_root == NULL);
    BUG_ON(sb->s_root->d_inode == NULL);
    BUG_ON(!S_ISDIR(root->i_mode));
    BUG_ON(sb->s_root->d_inode != root); 
 
    mark_buffer_dirty(sbi->sb);

    return 0;
}


struct dentry *rubix_mount(struct file_system_type *fs_type, int flags, const char *dev_name, void *data)
{
    return mount_bdev(fs_type, flags , dev_name, data, rubix_fill_super);
}


// These are the address operatons for the filesystem.
// Need to implement this one for reading and writing into the page.
//TODO: check why the read needs to be reading a full page by mapping all the buffers.? 
// Why cant it do a simple copy into the page_addr ?
// getblock typically allocates a page and then maps the buffers in the page with the sector
// number we want it to map to. Then we either do a read or a write depending on the op.
int rubix_read_asop_page(struct file *file, struct page *page)
{
 static int count =0;
printk("Reading page :%lu %d \n",page->index,count++);
    //return block_read_full_page(page, rubix_getblock_read);  // read function for now.
    return read_page_from_disk(file, page);
}

int rubix_write_asop_page(struct page *page, struct writeback_control *wbc)
{

    int ret=0;
 static int count=0;
#if 0 
char *kaddr = kmap(page);
struct inode *inode = page->mapping->host;
int block,last_block,bbits;
int offset;
unsigned long t = inode->i_size >> PAGE_CACHE_SHIFT;
//printk("this is being caaled for the writes ..\n");
//msleep(1000);
offset = inode->i_size & (PAGE_CACHE_SIZE-1);

// Just read a page to see what its writing.
printk("data being read/wriiten ..i_size:%lld :%s end_index :%lu page_index:%lu offset:%d \n",inode->i_size,kaddr,t,page->index,offset);
msleep(1000);
kunmap(page);
    bbits = inode->i_blkbits;
    block = (sector_t)page->index << (PAGE_CACHE_SHIFT - bbits);
    last_block = (inode->i_size - 1) >> bbits;
printk("block :%d last_block :%d",block,last_block);

#endif
    printk("Writing page count :%d \n",count++);
    ret = commit_page(page, wbc);  ////block_write_full_page(page, rubix_getblock ,wbc);

    return ret;
}

void dump_inode(struct inode *inode)
{
    printk("inode->blocks :%ld \n",inode->i_blocks);
}

#define indirect_blocks_defined 1
//TODO: For now we just allocate the next block in the free list. The next step in our 
// Implementation is design is to free blocks and indirect blocks that we have.

//THis is very imp: Do not use the in disk structure everytime except for the last write
// and the initial read . Use the in memory structure in the other cases to store information.
//This will alloc the data blocks for the writes.

///$$$$$FIXME$$$$ This function will no longer be needed as we will just try to create a new snapshot tree
// and commit them only after the sync writes it to the media.
int alloc_disk_write(struct inode *inode, uint64_t blk_num, uint64_t *pba, int alloc)
{
    struct buffer_head *bh_indir,*bh_in,*bo;
    struct super_block *sb = inode->i_sb;
    uint16_t *blk_id,*bin,*boin;
    int offset;
    uint64_t i;
    long int idx,mod,div;

    rubix_inmem_t *in_mem = rubix_create_inmem(inode);
    i = blk_num;

    // Total blocks in the file. We have to start from the next one for this write.
    // TODO :This one just allocates one block at a time for every write or read irrespective
    // of space in the first one. THis has been fixed now.We allocate a block only when needed.
 
    // For all the blocks in this range needed for this IO, we need to check
    // if we need direct or indirect blocks.
    if (i < IDX_INDIRECT_BLK)
    {
  //      if (in_mem->dblocks[i] == 0)  // equal to -1
      if (alloc == 1)  // this will be used in case of snapshots btrees too.
        {
            // Allocate only one block at a time.
            if (!(in_mem->dblocks[i] = alloc_dblock_from_bitmap(sb)))
            {
                return -ENOMEM;
            }
            *pba = in_mem->dblocks[i];
            // After allocation just check.
            BUG_ON(in_mem->dblocks[i] == 0);
            inode->i_blocks++;
        }
        else
        {
            *pba = alloc_dblock_from_bitmap(sb);
            // After allocation just check.
            BUG_ON(in_mem->dblocks[i] == 0); // has the old value still.
            inode->i_blocks++;
         }
        // Else no need to allocate for this one.
    }
    else if (i < IDX_INDIRECT_BLK2)
    {
        // Only read then.
        if(!in_mem->dblocks[IDX_INDIRECT_BLK]) // INdirect blocks
        {
            in_mem->dblocks[IDX_INDIRECT_BLK] = alloc_dblock_from_bitmap(sb);
        }

        // Read the global block into the head and then parse through the block for the
        // data to map it into the indirect blocks.
        bh_indir = sb_bread(sb, in_mem->dblocks[IDX_INDIRECT_BLK]);
        blk_id =  (uint16_t *)(bh_indir->b_data + (i - IDX_INDIRECT_BLK) * 2);
 
        if(*blk_id == 0)
        {
            // Allocate the data block here.
            *blk_id = alloc_dblock_from_bitmap(sb);
            *pba = *blk_id;
        }

        mark_buffer_dirty(bh_indir);
        mark_inode_dirty(inode);
        // We are allocating a bh so we have to release it.
        brelse(bh_indir);
        BUG_ON(*blk_id == 0);
    }
    else if(i < IDX_INDIRECT_BLK3)
    {
        // Only read then.
 
        if(!in_mem->dblocks[IDX_INDIRECT_BLK + 1]) // INdirect blocks
        {
            in_mem->dblocks[IDX_INDIRECT_BLK + 1] = alloc_dblock_from_bitmap(sb);
        }

        // Read the global block into the head and then parse through the block for the
        // data to map it into the indirect blocks.
        // Read the first 2Indirect block.
        bh_indir = sb_bread(sb, in_mem->dblocks[IDX_INDIRECT_BLK + 1]);
        idx = i - IDX_INDIRECT_BLK2;
        div = idx / 512;
        mod = idx % 512;
        blk_id =  (uint16_t *)(bh_indir->b_data + div * 2);
        
        if (*blk_id == 0)
        {
            // Allocate this guy for the 2nd indirect reference.
            *blk_id = alloc_dblock_from_bitmap(sb); 
        }
        bh_in = sb_bread(sb, *blk_id);
        offset = mod * 2; // Should still modify this a little bit
        bin = (uint16_t *)(bh_in->b_data + offset);
 
        if(*bin == 0)
        {
            // Allocate only one block at a time.
            *bin = alloc_dblock_from_bitmap(sb);
            *pba = *bin;
        }
        mark_buffer_dirty(bh_indir);
        // We are allocating a bh so we have to release it.
        brelse(bh_indir);

        mark_buffer_dirty(bh_in);
        mark_inode_dirty(inode);
        // We are allocating a bh so we have to release it.
        brelse(bh_in);

        BUG_ON(*bin == 0);
    }
    else
    {
        // Have to fix only this .FIXME:   
        // Only read then.
        if(!in_mem->dblocks[IDX_INDIRECT_BLK + 2]) // INdirect blocks
        {
            in_mem->dblocks[IDX_INDIRECT_BLK + 2] = alloc_dblock_from_bitmap(sb);
        }

        bh_indir = sb_bread(sb, in_mem->dblocks[IDX_INDIRECT_BLK + 2]);
        idx = i - IDX_INDIRECT_BLK3;
        div = idx / (512 * 512);
        mod = idx % (512 * 512);
    
        blk_id =  (uint16_t *)(bh_indir->b_data + div * 2 ); // 2 bytes for each entry.

       // printk("Mapping :%d  indir_blk_id:%d\n",*blk_id, in_mem->dblocks[IDX_INDIRECT_BLK]);
        if (*blk_id == 0)
        {
            // Allocate this guy for the 2nd indirect reference.
            *blk_id = alloc_dblock_from_bitmap(sb); 
        }
 
        //blk_id is the 2nd level indirect reference. 
        bh_in = sb_bread(sb, *blk_id);
        div = mod / 512;
        mod =  mod % 512;

        bin = (uint16_t *)(bh_in->b_data + div * 2);
        //bin is the 3rd level indirect reference.
        if (*bin == 0)
        {
            // Allocate this guy for the 2nd indirect reference.
            *bin = alloc_dblock_from_bitmap(sb); 
        }
        bo = sb_bread(sb, *bin);
 
        boin = (uint16_t *)(bo->b_data + mod * 2 );

        if(*boin == 0)
        {
            // Allocate this guy for the 2nd indirect reference.
            *boin = alloc_dblock_from_bitmap(sb); 
            *pba = *boin;
        }

        mark_buffer_dirty(bh_indir);
        mark_buffer_dirty(bh_in);
        mark_buffer_dirty(bo);
        mark_inode_dirty(inode);
        // We are allocating a bh so we have to release it.
        brelse(bh_indir);
        brelse(bh_in);
        brelse(bo);

        BUG_ON(*boin == 0);
        // BLK 3 indirect reference.
    }
//    *pba = alloc_dblock_from_bitmap(sb);

    return 0;
}

// out is the putput buffer head of the mapped buffer into the physical location specified by  
// the sector number.

//btree_node_t *root_new;
/*int rubix_getblock(struct inode *inode, sector_t block, struct buffer_head *out, int create)
{
    // Never use rubix inode as its the disk structure. Use the inmem structure to map.
    uint16_t *blk_id,*bin,*boin;
    struct buffer_head *bh_indir,*bh_in,*bo;
    struct super_block *sb= inode->i_sb;
    rubix_inmem_t *in_mem = rubix_create_inmem(inode);
    long int idx,div,mod;
#if CHECKPOINT 
 
    btree_node_t *node,*node1;
#endif 
 
    // Map the buffers based on the index we need to map for this.
    if(block < IDX_INDIRECT_BLK) // Direct map
    {
        // Use the tree to map to the new data.
        BUG_ON(in_mem->dblocks[block] == 0);  // It should be allocated so this should be something else.
#if CHECKPOINT 
       node = search_node(in_mem->btree_root,block);
        node1 = search_node(root_new,block);
///printk("fkghflkghf has to be remapped ..\n");
        if(node1)
        {
            map_bh(out, inode->i_sb, node1->pba[block]);//in_mem->dblocks[block]);
            printk("atitude ..%llu\n",node1->pba[block]);
        }
        else
#endif
        {
           printk("i wanna check this one ..\n");
           map_bh(out, inode->i_sb, in_mem->dblocks[block]);
        }
 
 // BUG_ON only for direct blocks for now. We will extend it later for indirect blocks.
    }
    else if(block < IDX_INDIRECT_BLK2) // Indirect blocks
    {
        // has to exist.
        BUG_ON(in_mem->dblocks[IDX_INDIRECT_BLK] == 0);
        bh_indir = sb_bread(sb, in_mem->dblocks[IDX_INDIRECT_BLK]);
        blk_id =  (uint16_t *)(bh_indir->b_data + (block - IDX_INDIRECT_BLK) * 2 ); // 2 bytes for each entry.
        BUG_ON(*blk_id == 0);
        map_bh(out, inode->i_sb, *blk_id);
    }
    else if(block < IDX_INDIRECT_BLK3)
    {
        // has to exist.
        BUG_ON(in_mem->dblocks[IDX_INDIRECT_BLK + 1] == 0);
        bh_indir = sb_bread(sb, in_mem->dblocks[IDX_INDIRECT_BLK + 1]);
        idx = block - IDX_INDIRECT_BLK2;
        div = idx / 512;
        mod = idx % 512;
        blk_id =  (uint16_t *)(bh_indir->b_data + div * 2 ); // 2 bytes for each entry.
        //blk_id is the 2nd level indirect reference. 
        bh_in = sb_bread(sb, *blk_id);
        bin = (uint16_t *)(bh_in->b_data + mod * 2);
        BUG_ON(*bin == 0);
        map_bh(out, inode->i_sb, *bin);
    }
    else
    {
        // has to exist.
        BUG_ON(in_mem->dblocks[IDX_INDIRECT_BLK + 2] == 0);
        bh_indir = sb_bread(sb, in_mem->dblocks[IDX_INDIRECT_BLK + 2]);
 
        idx = block - IDX_INDIRECT_BLK3;
        div = idx / (512 * 512);
        mod = idx % (512 * 512);

        blk_id =  (uint16_t *)(bh_indir->b_data + div * 2 ); // 2 bytes for each entry.
        //blk_id is the 2nd level indirect reference.
        bh_in = sb_bread(sb, *blk_id);
        div = mod / 512;
        mod =  mod % 512;

        bin = (uint16_t *)(bh_in->b_data + div * 2);
        //bin is the 3rd level indirect reference.
        bo = sb_bread(sb, *bin);

        boin = (uint16_t *)(bo->b_data +  mod * 2);
        map_bh(out, inode->i_sb, *boin);
    }

    return 0;
}


btree_node_t *root_new;
int rubix_getblock_read(struct inode *inode, sector_t block, struct buffer_head *out, int create)
{
    // Never use rubix inode as its the disk structure. Use the inmem structure to map.
    uint16_t *blk_id,*bin,*boin;
    struct buffer_head *bh_indir,*bh_in,*bo;
    struct super_block *sb= inode->i_sb;
    rubix_inmem_t *in_mem = rubix_create_inmem(inode);
    long int idx,div,mod;
#if CHECKPOINT
    btree_node_t *node,*node1;
#endif
    // Map the buffers based on the index we need to map for this.
    if(block < IDX_INDIRECT_BLK) // Direct map
    {
        // Use the tree to map to the new data.
        BUG_ON(in_mem->dblocks[block] == 0);  // It should be allocated so this should be something else.
#if CHECKPOINT
        node = search_node(in_mem->btree_root,block);
        node1 = search_node(root_new,block);
///printk("fkghflkghf has to be remapped ..\n");
        if(node)
        {
            map_bh(out, inode->i_sb, node->pba[block]);//in_mem->dblocks[block]);
            printk("atitude ..%llu\n",node->pba[block]);
        }
        else
#endif
        {
           printk("i wanna check this one reader .. :%llu \n",in_mem->dblocks[block]);
           map_bh(out, inode->i_sb, in_mem->dblocks[block]);
        }
 
 // BUG_ON only for direct blocks for now. We will extend it later for indirect blocks.
    }
    else if(block < IDX_INDIRECT_BLK2) // Indirect blocks
    {
        // has to exist.
        BUG_ON(in_mem->dblocks[IDX_INDIRECT_BLK] == 0);
        bh_indir = sb_bread(sb, in_mem->dblocks[IDX_INDIRECT_BLK]);
        blk_id =  (uint16_t *)(bh_indir->b_data + (block - IDX_INDIRECT_BLK) * 2 ); // 2 bytes for each entry.
        BUG_ON(*blk_id == 0);
        map_bh(out, inode->i_sb, *blk_id);
    }
    else if(block < IDX_INDIRECT_BLK3)
    {
        // has to exist.
        BUG_ON(in_mem->dblocks[IDX_INDIRECT_BLK + 1] == 0);
        bh_indir = sb_bread(sb, in_mem->dblocks[IDX_INDIRECT_BLK + 1]);
        idx = block - IDX_INDIRECT_BLK2;
        div = idx / 512;
        mod = idx % 512;
        blk_id =  (uint16_t *)(bh_indir->b_data + div * 2 ); // 2 bytes for each entry.
        //blk_id is the 2nd level indirect reference. 
        bh_in = sb_bread(sb, *blk_id);
        bin = (uint16_t *)(bh_in->b_data + mod * 2);
        BUG_ON(*bin == 0);
        map_bh(out, inode->i_sb, *bin);
    }
    else
    {
        // has to exist.
        BUG_ON(in_mem->dblocks[IDX_INDIRECT_BLK + 2] == 0);
        bh_indir = sb_bread(sb, in_mem->dblocks[IDX_INDIRECT_BLK + 2]);
 
        idx = block - IDX_INDIRECT_BLK3;
        div = idx / (512 * 512);
        mod = idx % (512 * 512);

        blk_id =  (uint16_t *)(bh_indir->b_data + div * 2 ); // 2 bytes for each entry.
        //blk_id is the 2nd level indirect reference.
        bh_in = sb_bread(sb, *blk_id);
        div = mod / 512;
        mod =  mod % 512;

        bin = (uint16_t *)(bh_in->b_data + div * 2);
        //bin is the 3rd level indirect reference.
        bo = sb_bread(sb, *bin);

        boin = (uint16_t *)(bo->b_data +  mod * 2);
        map_bh(out, inode->i_sb, *boin);
    }

    return 0;
}




int my_block_write_begin(struct address_space *mapping, loff_t pos, unsigned len,
                unsigned flags, struct page **pagep, get_block_t *get_block)
{
        pgoff_t index = pos >> PAGE_CACHE_SHIFT;
        struct page *page;
        int status;

        page = grab_cache_page_write_begin(mapping, index, flags);
        if (!page)
                return -ENOMEM;

printk("it did grab the page ..\n");
        status = __block_write_begin(page, pos, len, get_block);
        if (unlikely(status)) {
                unlock_page(page);
                page_cache_release(page);
                page = NULL;
        }

        *pagep = page;
        return status;
}*/

// this will be used to remap the buffers into new device blocks to implement snapshots. The btree is used to keep track of the actual
// physical block numbers.

// We are using this to write as we have the file struct for the inodes.
int rubix_prepare_write_asop_page(struct file *file,struct address_space *mapping, loff_t pos, unsigned len, unsigned flags, struct page **page ,void **fsdata)
{
    /*struct dentry *de = file->f_dentry;
    struct inode *inode = de->d_inode;
    unsigned int bbits;
    //uint64_t pba;
    uint64_t i,start_block_index,end_block_index;*/
    pgoff_t index;
/*#if CHECKPOINT
    btree_node_t *node= NULL;
    uint64_t block_start, block_end,block;
    // Read from the ondisk inode into the VFS inode and return.
    rubix_inmem_t *rb_inode = rubix_create_inmem(inode);
    // We allocate the disk blocks based on a page size. This function is called
    // per page by the VFS so we allocate per page.
#endif 
    bbits = inode->i_blkbits;
    start_block_index = pos >> bbits;
    end_block_index =  (pos + len) >> bbits;

    for(i = start_block_index;i<=end_block_index;i++)
    {
#if CHECKPOINT
        // allocate and map the allocated blocks.
        node = search_node(rb_inode->btree_root,i);
        if(node == NULL)  // No entries in the tree so first node.
        {
            alloc_disk_write(inode, i, &pba , 1);
            rb_inode->cp_ver->btree_root[latest_commit] = insert_btree_node(rb_inode->cp_ver->btree_root[latest_commit], i, pba);
        }
        else
        {
           // this is the new pba for the same lba(i). This will be the new tree now.
           // Clear the already mapped block and then remap later.
           struct buffer_head *bh,*head;

           pgoff_t index = pos >> PAGE_CACHE_SHIFT;
           struct page *page;

           page = grab_cache_page_write_begin(mapping, index, flags);
           head = page_buffers(page);

           for(bh = head,block_start=0 ; bh != head || !block_start;
            block++,block_start=block_end, bh = bh->b_this_page) 
           {
                block_end = block_start+ (1 << bbits);
                // Clear all the mappings to the buffers so that we can get new mappings
                // again later on.
                if(bh->b_state & (1ULL << BH_Mapped))
                {
                     printk("Its mapped already..\n");
                     // clear the mapping
                     bh->b_state &= ~(1ULL << BH_Mapped);
                     mark_buffer_dirty(bh);
                }
           }
           // Have unlock it here to make sure others can use it later.
           unlock_page(page);
           page_cache_release(page);

           alloc_disk_write(inode, i, &pba, 2); // allocate one block but dont edit to check the snapshot
           printk("Allocating a new block for the same lba :%llu :%llu",i, pba);
           root_new = search_write_modification(rb_inode->btree_root, i, pba);
           print_btree(root_new);
       }
#else 
     //alloc_disk_write(inode, i, &pba, 1);  // For now we ll have both versions.
    }
#endif
//printk("pba:%llu \n",pba);

    // We have to allocate all blocks from pos to end inorder to map and write all data.
    // Block is the buffer in the page and we should should allocate a block only when we need this one.
    // This buffer has to be allocated if not allocated.
    // This will create a page to cache. We then call the callback to map the buffers
    // in the page with the device and the sector numbers allocated.
    // Note that this is the physical location of the block on the disk, we map to.
    //block_write_begin(mapping, pos, len, flags, page, rubix_getblock);*/
    //FIXME: Here we are just allocating a page if we need. We are allocating.
   
    index = pos >> PAGE_CACHE_SHIFT;

    *page = grab_cache_page_write_begin(mapping, index, flags);

    return 0;
}

// Same version to just get only a buffer. This is used for metadata.
uint64_t get_append_point_buffer(struct super_block *sb, uint64_t req_blks)
{
   rubix_sb_info_t *sbi = sb->s_fs_info;
   int64_t blk_num=-1;
   // This function has to increment the append point, set the bitmaps(i guess this can be eliminated
   // if its a log based write.)
   //alloc_from_append_point(sb);
   EB *eb = sbi->append_point;
 
   // Lets see if we have this variable already. ?? 
   while(1)
   {
       // For now we just have 
       // WE have enough in this EB.
       if(req_blks < eb->free_blks)
       {
           blk_num = (eb->eb_num * NUM_BLKS_PER_EB) + eb->cur_blk;
           eb->cur_blk = eb->cur_blk + req_blks;
           eb->free_blks = eb->free_blks - req_blks;
           // only one block at a time.
           // If the current EB is done then the append point Eb is the next one.
           break;
       }
       else
       {
           // Next eb.
           eb->eb_num++;
           // Reset all values for the current EB.
           eb->cur_blk = 0;
           eb->start_blk = 0;
           eb->end_blk = NUM_BLKS_PER_EB;
           eb->free_blks = NUM_BLKS_PER_EB;
           continue;
       }
   
       // LAST_EB is initally the last one. When grooming is done we update the LAST_EB as required.   
       if(eb->eb_num == eb->LAST_EB) break;
   }
   // Get the global sector number.
   return blk_num;
}

// The super block operations read inode, write_inode.
struct inode *rubix_sb_read_inode(struct inode *inode)
{
    rubix_inode_t *ino;
    struct buffer_head *bh;
    int i;
    struct super_block *sb = inode->i_sb;
    rubix_sb_info_t *sbi = sb->s_fs_info;
 
    // Read from the ondisk inode into the VFS inode and return.
    rubix_inmem_t *raw_inode = rubix_create_inmem(inode);
 
    // This will always get the latest copy of the rubix_inode.
    ino = get_rubix_inode_from_vfs(inode, &bh);
  
    inode->i_mode = ino->i_mode;
    // Fill up struct inode with rubix_inode fields and return it to VFS.
   
    inode->i_atime.tv_sec = ino->i_atime;
    inode->i_ctime.tv_sec = ino->i_ctime;
    inode->i_mtime.tv_sec = ino->i_mtime;
    // Make the nanos 0
    inode->i_atime.tv_nsec = 0;
    inode->i_ctime.tv_nsec = 0;
    inode->i_mtime.tv_nsec = 0;
    inode->i_atime= inode->i_mtime = inode->i_ctime = CURRENT_TIME;

    inode->i_blocks = ino->i_blocks; //NOt sure why this is not reading correct??
    inode->i_uid =  current_fsuid();
    inode->i_gid = current_fsgid();
    //inode->i_nlink = ino->i_nlinks;
    inode->i_size = ino->i_size;

    // Copy from the disk data into the memory
    // Copy only the direct blocks.
    for(i=0;i<=IDX_INDIRECT_BLK + 3;i++)
    {
        raw_inode->dblocks[i] = ino->dblocks[i];
        printk("raw_inode:%llu\n",raw_inode->dblocks[i]);
    }

  /// if(ino->dblocks[IDX_INDIRECT_BLK])
  // {
        // Just make sure we are actually commiting changes to the indirect blocks
  //      bh1 = sb_bread(inode->i_sb, ino->dblocks[IDX_INDIRECT_BLK]); 
      
  //  }
    printk("size:%llu \n",inode->i_size);
    // Maybe this is not so true later.
    if(inode->i_ino == 0 )
    {
       // THis is the root 
        // the general value maybe not be true later .??
       BUG_ON(raw_inode->dblocks[0] != 2 * sbi->map_arrays + 3 );
    }
 
    brelse(bh);
// Only verify if we need to.
#if TEST
    verify_mark_inode(inode);
#endif
    mark_inode_dirty(inode);

    return inode;
}

int rubix_sb_write_inode(struct inode *inode , struct writeback_control *wbc)
{
    rubix_inode_t *ino = NULL;
    rubix_inmem_t *in_mem = NULL;
    struct buffer_head *bh;
    uint16_t *blk_id;
    struct buffer_head *bh1;
    int i;
    uint64_t blk_num;
    struct super_block *sb = inode->i_sb;
    rubix_sb_info_t *sbi = sb->s_fs_info;
    inode_table *i_table = NULL;
 
    i_table = sbi->i_table;
    // There has to be a table.
    BUG_ON(i_table == NULL); 


    in_mem = rubix_create_inmem(inode);

    // Read from the ondisk inode into the VFS inode and return.
    // check the size we are actually writing.
    
    blk_num = get_append_point_buffer(sb, 1);
    bh = sb_bread(sb, blk_num);
  
    ino = (rubix_inode_t *)(bh->b_data);
    //ino = get_rubix_inode_from_vfs(inode ,&bh);
    // Fill up rubix_inode with struct inode fields and write to the disk.
    // Get the latest info from ur inode.
    ino->i_mode = inode->i_mode;
    ino->i_atime = inode->i_atime.tv_sec;
    ino->i_ctime = inode->i_ctime.tv_sec;
    ino->i_mtime = inode->i_mtime.tv_sec;
    ino->i_blocks = inode->i_blocks;
    ino->i_uid = inode->i_uid;
    ino->i_gid = inode->i_gid;
    //ino->i_nlinks = inode->i_nlink;
    ino->i_size = inode->i_size;
 
    // NOTE for now we are just mapping into the rubix_inode structure for that particular
    // inode and using that value to 
    // Copying the data into the data blocks of the on disk inode.
    // This will then be written to the disk. 
    // Write only the direct ones onto the disk.
    for(i=0;i<=IDX_INDIRECT_BLK + 3;i++)
    {
       ino->dblocks[i] = in_mem->dblocks[i];
       printk("in_mem:%llu\n",in_mem->dblocks[i]);
    }

    if(in_mem->dblocks[IDX_INDIRECT_BLK])
    {
       bh1 = sb_bread(inode->i_sb, in_mem->dblocks[IDX_INDIRECT_BLK]); 
       for(i=0;i<inode->i_blocks-IDX_INDIRECT_BLK;i++)
       { 
           blk_id =  (uint16_t *)(bh1->b_data + i * 2);
           printk("%d \n",*blk_id); 
       }
       brelse(bh1);
    }
    printk("size:%llu \n",inode->i_size);

    if(inode->i_ino == 0 )
    {
       // THis is the root 
        // the general value maybe not be true later .??
       //BUG_ON(ino->dblocks[0] != 2 * sbi->map_arrays + 3 );
    }
 

    mark_buffer_dirty(bh);
    brelse(bh);
    // then update the rubix_inode map.
    i_table->i_ent[inode->i_ino]->blk = blk_num; // This is the new blk.
    i_table->i_ent[inode->i_ino]->offset = 0; /// Will be zeroo until i figure out if we actually need this.
 
    return 0;
}

// Implement this one. We have to destroy the 2 inodes.
void rubix_sb_destroy_inode(struct inode *inode)
{
    rubix_destroy_inode(inode);
}

void init_inode_links(struct inode *dir, struct inode *inode, int mode)
{
    if(S_ISDIR(mode))
    {
        inc_nlink(inode);
    }
    // Also increment the dir links for every entry created.
    inc_nlink(dir);
    inode->i_atime= inode->i_mtime = inode->i_ctime = CURRENT_TIME;
}

int inode_dentry_to_dir( struct inode *dir, struct dentry *dentry, struct inode *inode)
{
    struct buffer_head *bh;
#if TEST
    struct buffer_head *bh1;
    rubix_dir_entry_t *dientry;
#endif
    rubix_dir_entry_t *dir_entry;
    rubix_inmem_t *in = rubix_create_inmem(dir);
    int  data_blk;
    struct super_block *sb = dir->i_sb;

    data_blk =  in->dblocks[0];
    BUG_ON(data_blk == 0);  // THis cannot be null
    bh = sb_bread(sb, data_blk);
    // Each directory entry is 64 bytes and we have to skip i_size entries to 
    // reach the current position where we write.
    dir_entry = (rubix_dir_entry_t *)(bh->b_data  + dir->i_size);

    // Fill up the the directory entry here.
    dir_entry->inode = inode->i_ino;
    dir_entry->rec_len = 64; // Always 64 bytes
    dir_entry->name_len = dentry->d_name.len;

    if(S_ISDIR(inode->i_mode))
    {
        dir_entry->file_type = 2;
    }
    else
    {
        dir_entry->file_type = 1; 
    }
    memcpy(dir_entry->name, dentry->d_name.name, dir_entry->name_len + 1);

    // Fill some other fields into the dentry. 
 
    dir->i_size +=64;
    dir->i_mtime = CURRENT_TIME;

    mark_buffer_dirty(bh);
    mark_inode_dirty(dir);
    brelse(bh);
#if TEST
    bh1 = sb_bread(sb, data_blk);
    dientry = (rubix_dir_entry_t *)(bh1->b_data  + (dir->i_size - 64 )); // Verifiying the recently written one.
    if(dir_entry->inode == dientry->inode && dir_entry->rec_len == dientry->rec_len && 
       !memcmp(dir_entry->name,dientry->name, dir_entry->name_len))
    {
        printk("All fields look ok ..\n");
        dump_all_entry(dir);
    }
    else
    {
        dump_dentry(dir_entry);
        dump_dentry(dientry);
        kassert(0);
    }
    brelse(bh1);

#endif 
 
    return 0;
}

// Empty the data on this data blk.
// Here we are clearing the inmem pages associated with that buffer(which is the same as that datablock)
// So before we allocate the same data block to another inode, we have to clear this page associated.
void reclaim_data_blocks(struct inode *inode, uint64_t blk)
{
    // Find the number of pages.
    struct buffer_head *bh;
    // Also clean up the in_mem data
    bh = sb_bread(inode->i_sb, blk);
    bforget(bh);
    // Make sure the pages are uptodate.
    mark_inode_dirty(inode);
}

#if DUMPCODE

int rubix_write_end(struct file *file, struct address_space *mapping,
                                 loff_t pos, unsigned len, unsigned copied,
                                 struct page *page, void *fsdata)
{
    struct buffer_head *bh;
    struct inode *inode = mapping->host;
    rubix_inmem_t *in_mem = rubix_create_inmem(inode);
    int data_blk;

   int  ret = generic_write_end(file, mapping, pos, len, copied, page, fsdata);

    // Verify that data has been written.
    data_blk = in_mem->dblocks[0];
    bh = sb_bread(inode->i_sb, data_blk);
    printk("bh->b_data :%s",bh->b_data);
msleep(1000);
    mark_buffer_dirty(bh);
    brelse(bh);
 
    return ret;
}
#endif

void rubix_put_super(struct super_block *sb)
{
     rubix_sb_info_t *sbi = (rubix_sb_info_t *)sb->s_fs_info; 
     int i;
     uint64_t base,offset;
     super_block_t *sbd; 
     struct buffer_head *bh;
     inode_table *i_table;
     // Free is this exists.
     if (sbi)
     {
         sbd = (super_block_t *)(sbi->sb->b_data);

         i=0;
         base =1; // First block
         bh = sb_bread(sb, base);
         offset = 0;
         i_table = sbi->i_table;
    
         while(i< NUM_INODES_FS)
         {
             inode_entry *i_en;
             // ALlocate the entry for table entry
             i_en =  (inode_entry *)(bh->b_data + offset);
             // Add it to the table of entries.
             i_en->blk = i_table->i_ent[i]->blk;
             i_en->offset = i_table->i_ent[i]->offset;
             offset+= sizeof(inode_entry);
             i++;
             kfree(i_table->i_ent[i]); // , sizeof(inode_entry));

             if((i % (SIZE_ONE_BLOCK / sizeof(inode_entry))) == 0)
             {
                 bh = sb_bread(sb, base++);
             }
             if(i > NUM_INODES_FS) break;
             mark_buffer_dirty(bh);
         }

         // Save the append point onto the disk.
         memcpy(sbd->append_point, sbi->append_point, sizeof(EB));
         mark_buffer_dirty(sbi->sb);
         brelse(sbi->sb);
         /*i=0;
         while (i < sbi->map_arrays)
         {
             mark_buffer_dirty(*(sbi->ibmap +i));
             mark_buffer_dirty(*(sbi->bbmap+ i));
             brelse(*(sbi->ibmap +i));
             brelse(*(sbi->bbmap+ i));
             i++;
         }*/
         kfree(sbi);
    }
}


void rubix_kill_super(struct super_block *sb)
{
    /* rubix_sb_info_t *sbi = (rubix_sb_info_t *)sb->s_fs_info; 
     int i;
     // Free is this exists.
     if (sbi)
     {
         mark_buffer_dirty(sbi->sb);
         brelse(sbi->sb);
         i=0;
         while (i < sbi->map_arrays)
         {
             mark_buffer_dirty(*(sbi->ibmap +i));
             mark_buffer_dirty(*(sbi->bbmap+ i));
             brelse(*(sbi->ibmap +i));
             brelse(*(sbi->bbmap+ i));
             i++;
         }
         kfree(sbi);
    }*/
    kill_block_super(sb);
}

//////Page related functions. These are begin added to make things a bit simpler. We do not want to many interactions with the vfs so call into the block directly.In that way we just update the 
///// pages for the respective inodes and then fsync all of them through a different thread either in the VFS or in our FS. THi si being done to increase performance and to develop logfs features with more simplicity.

// These functions will just get the index and the page in the index for that inode and then copy it.
// This is being done to remove the buffer head references completely and submit data using bios to increase performance. 
/*
int rubix_prepare_write_asop_page(struct file *file,struct address_space *mapping, loff_t pos, unsigned len, unsigned flags, struct page **page ,void **fsdata)
{
    struct dentry *de = file->f_dentry;
    struct inode *inode = de->d_inode;

    memcpy(ofc ,buf, len);

}
*/

// Get the log append point. This will be linear commit point.
uint64_t get_append_point(struct super_block *sb, uint64_t nr_pages)
{
   rubix_sb_info_t *sbi = sb->s_fs_info;
   uint64_t req_blks;
   uint64_t blk_num;
   // This function has to increment the append point, set the bitmaps(i guess this can be eliminated
   // if its a log based write.)
   //alloc_from_append_point(sb);
   EB *eb = sbi->append_point;
  
   // Lets see if we have this variable already. ?? 
   req_blks = nr_pages * 4; // For now.//NUM_BLKS_PAGE;
 
   while(1)
   {
 
       // For now we just have 
       // WE have enough in this EB.
       if(req_blks < eb->free_blks)
       {
           blk_num = (eb->eb_num * NUM_BLKS_PER_EB) + eb->cur_blk;
           eb->cur_blk = eb->cur_blk + req_blks;
           eb->free_blks = eb->free_blks - req_blks;
           // only one block at a time.
           // If the current EB is done then the append point Eb is the next one.
           break;
       }
       else
       {
           // Next eb.
           eb->eb_num++;
           // Reset all values for the current EB.
           eb->cur_blk = 0;
           eb->start_blk = 0;
           eb->end_blk = NUM_BLKS_PER_EB;
           eb->free_blks = NUM_BLKS_PER_EB;
           continue;
       }

       // LAST_EB is initally the last one. When grooming is done we update the LAST_EB as required.   
       if(eb->eb_num == eb->LAST_EB) break;
 
   }
   // Get the global sector number.
   return blk_num;
}

// This is the completor. We should probably update the tree here, Once disk has already written. 
// This is called by the underlaying driver.
void commit_pages_complete(struct bio *bio, int err)
{
    // either update the bio_sectors into the tree or the array which will be committed into the metadata
    // of the inode.
    struct inode *inode = (struct inode *)bio->bi_private;
    rubix_inmem_t *in_mem = rubix_create_inmem(inode);
    struct page *page;
    int i;
    btree_node_t *node=NULL,*root_new;
    uint64_t nr_pages,idx;
    
    nr_pages = inode->i_bytes >> PAGE_CACHE_SHIFT;
    if (inode->i_bytes % PAGE_CACHE_SHIFT) nr_pages++;

    for(i=0;i<nr_pages;i++)
    {
        page = bio->bi_io_vec[i].bv_page;
        i = page->index;
        i = i * 4;
        // search for the i and update the tree. 
        node = search_node(in_mem->btree_root, i, &idx);

        if(node == NULL)
        {
              in_mem->btree_root = insert_btree_node(in_mem->btree_root, i, bio->bi_sector);
        }
        else
        {
              // Lba that has been written. Here we are writing multiple entries into the tree.
              root_new = search_write_modification(in_mem->btree_root, i,bio->bi_sector);
              print_btree(root_new);
        }
    }
//#endif
}

// Writing only a page to the disk.
void commit_page_complete(struct bio *bio, int err)
{
     // Lba that has been written. Here we are writing multiple entries into the tree.
   // root_new = search_write_modification(rb_inode->btree_root, lba ,bio->bi_sector);
//#if CHECKPOINT
    struct page *page;
    struct inode *inode = (struct inode *)bio->bi_private;
    btree_node_t *node=NULL,*root_new;
    int i=0;
    uint64_t idx;
    rubix_inmem_t *in_mem = rubix_create_inmem(inode);

    page = bio->bi_io_vec[i].bv_page;
    i = page->index;
    i = i * 4 ;//NUM_BLKS_PER_PAGE;

    node = search_node(in_mem->btree_root,i, &idx);

    if(node == NULL)  // No entries in the tree so first node.
    {
         in_mem->btree_root = insert_btree_node(in_mem->btree_root, i, bio->bi_sector);
    }
    else
    {
         root_new = search_write_modification(in_mem->btree_root, i, bio->bi_sector);
         print_btree(root_new);
    }
    // level order traversal will be saved into the 
    // We dont want to commit in the write path.
    //commit_latest_tree(in_mem->btree_root, in_mem); 
//#endif
}

/// call this from the fsync function.
int commit_pages_of_inode(struct inode *inode)
{
    struct address_space *mapping = inode->i_mapping;
    uint64_t index = 0;
    int i=0;
    struct bio *bio;
    uint64_t pages_nr,max_pages;
    // Get the first page at the index 0. All pages should exist until here.
    struct page *page;
    struct super_block *sb = inode->i_sb;
    // Probably the first reference to the block layer.
    struct request_queue *q = bdev_get_queue(sb->s_bdev); 
    // how many pages are allowed by the underlying block.
    max_pages = queue_max_hw_sectors(q) >> (PAGE_SHIFT - 10); //Block layeer allows only certain number of pages. Make it 10 for now but lets have another define for that or should be user parameter ? 
 
    // The nr of pages for inode index. 
    pages_nr = inode->i_bytes >> PAGE_CACHE_SHIFT;
    if (inode->i_bytes % PAGE_CACHE_SHIFT) pages_nr++;

    bio = bio_alloc(GFP_NOFS, pages_nr);
    // Pages_nr are the total number needed.
    for(i=0;i<pages_nr;i++)
    {
        // go and submit and reset all the fields.
        if(i >= max_pages)
        {
            // adjust this guy later.
            bio->bi_vcnt = pages_nr;
            bio->bi_idx = 0;
            bio->bi_size = pages_nr * PAGE_SIZE;
            bio->bi_bdev = sb->s_bdev;
            bio->bi_sector = get_append_point(sb, pages_nr);  ///THis guy gets the latest append point sector number.
            bio->bi_private = inode;
            bio->bi_end_io = commit_pages_complete;
 
            submit_bio(WRITE, bio);
            index += i;
            // Reset the value.
            i=0;

            bio = bio_alloc(GFP_NOFS, max_pages);
            BUG_ON(!bio); 

        }
        // Get the page starting at index i which is 0.
        page   = find_get_page(mapping, i);
        bio->bi_io_vec[i].bv_page = page; // This is the page.
        bio->bi_io_vec[i].bv_len = PAGE_SIZE; // This is the page.
        bio->bi_io_vec[i].bv_offset = 0;
 
        unlock_page(page);
        page_cache_release(page);
    }
    bio->bi_vcnt = pages_nr;
    bio->bi_idx = 0;
    bio->bi_size = pages_nr * PAGE_SIZE;
    bio->bi_bdev = sb->s_bdev;
    // We need to update the append point with so many sectors.
    bio->bi_sector = get_append_point(sb, pages_nr);  ///THis gets the latest append point sector number.
    bio->bi_private = inode; // save this off we will need it.
    bio->bi_end_io = commit_pages_complete;
    submit_bio(WRITE, bio);

    return 0;
}

// Lets test this one.
int commit_page(struct page *page, struct writeback_control *wbc)
{
    // Commiting this page and update the tree.
    struct bio *bio;
    int i=0;
    struct inode *inode = page->mapping->host;
    struct super_block *sb = inode->i_sb;
    // Probably the first reference to the block layer.
    //struct request_queue *q = bdev_get_queue(sb->s_bdev); 
    // Only one page update. 
    bio = bio_alloc(GFP_NOFS, 1);
    // Get the page starting at index i which is 0.
    bio->bi_io_vec[i].bv_page = page; // This is the page.
    bio->bi_io_vec[i].bv_len = PAGE_SIZE; // This is the page.
    bio->bi_io_vec[i].bv_offset = 0;
    bio->bi_vcnt = 1;
    bio->bi_idx = 0;
    bio->bi_size = PAGE_SIZE;
    bio->bi_bdev = sb->s_bdev;
    // We need to update the append point with so many sectors.
    bio->bi_sector = get_append_point(sb, 1);  ///THis gets the latest append point sector number.
    bio->bi_private = (void *)inode; // save this off we will need it.
    bio->bi_end_io = commit_page_complete;
    submit_bio(WRITE, bio);
    return 0;
}

// Search for the lba and return the pba.
uint64_t get_physical_address(rubix_inmem_t *in_mem, uint64_t lba)
{
   btree_node_t *node;
   uint64_t idx;

   node = search_node(in_mem->btree_root, lba,&idx);
   // There has to be a node existing for this lba
   BUG_ON(node == NULL);
   return node->pba[idx];

}

// Lets Read the page.
int read_page_from_disk(struct file *file, struct page *page)
{
    // Commiting this page and update the tree.
    struct bio *bio;
    struct inode *inode = page->mapping->host;
    struct super_block *sb = inode->i_sb;
    // Probably the first reference to the block layer.
    rubix_inmem_t *in_mem = rubix_create_inmem(inode);
    uint64_t i;
    
    // Only one page update.
    bio = bio_alloc(GFP_NOFS, 1);
    // Get the page starting at index i which is 0.
    bio->bi_io_vec[0].bv_page = page; // This is the page.
    bio->bi_io_vec[0].bv_len = PAGE_SIZE; // This is the page.
    bio->bi_io_vec[0].bv_offset = 0;
    bio->bi_vcnt = 1;
    bio->bi_idx = 0;
    bio->bi_size = PAGE_SIZE;
    bio->bi_bdev = sb->s_bdev;
    // We need to update the append point with so many sectors.
    i = 4 * page->index;
    bio->bi_sector = get_physical_address(in_mem, i);  //   get_append_point(sb, 1);  ///THis gets the latest append point sector number.
    bio->bi_private = (void *)inode; // save this off we will need it.
    bio->bi_end_io = NULL; //commit_read_page_complete;
    submit_bio(READ, bio);
    return 0;
}

void *global_worker = NULL;
int __init init_filesystem(void)
{
    int ret=0;

    printk("RUBIX FS loaded  ..\n");
    // Before we register the filesystem we have to first create it.
    // We can either create it from the user space as a device and
    // then use that info to read into the flesystem structures and write
    // back to them when needed.

    ret = register_filesystem(&rubix_fs_type);
    // Creating the inode slab cache
    ret = create_slab_cache();
    // Testing workers here.
    global_worker = init_worker_threads();
    //create_worker();
    return ret;
}


void __exit exit_filesystem(void)
{
    printk("Removing RUBIX FS ..\n");
    destroy_worker_threads(global_worker);
    destroy_slab_cache();
    unregister_filesystem(&rubix_fs_type);
}

//MODULE_LICENSE("GPL");
module_init (init_filesystem);
module_exit (exit_filesystem);




///// TEmp putting in the linear indirect block allocation method. Will only try the other one later.
//// once i m clear about that.

      /* // First create a chain.
       if((ret = create_chain(chain, i, &len)))
       {
           return -ENOMEM;
       }
 
       j=0;
       flag=0;
       // Until chain exists. This should first work with the 2indirect blocks.
       while(j < len)
       {
           global_disk = alloc_from_bitmap();
           if(flag)
           {
               *blk_id = global_disk;
                if (j == len-1 ) break; // We are done.
           }
           if(j == len - 1)
           {
               in_mem->dblocks[chain[j]] = global_disk;
           }
           else
           {
               bh_indir = sb_bread(sb, global_disk);
               blk_id =  (uint16_t *)(bh_indir->b_data + chain[j] * 2);
               flag = 1; 
               j++;
           }
       }


#if TEST
   if( out->b_state & (1ULL << BH_Mapped))
    {
       printk("Its mapped already..\n");
    }
    else
    {
      printk("Not mapped \n");
    }
#endif
    // Use the new mapping scheme for this too.
 
    if((ret = create_chain(chain, block, len)))
    {
        printk("Cant fail here ..\n");
        return -ENOMEM;
    }
   
 
    j=0;
    flag=0;
    // Until chain exists. This should first work with the 2indirect blocks.
    while(j < len)
    {
           
           if(j == len - 1)
           {
               in_mem->dblocks[chain[j]] = global_disk;
           }
           else
           {
               bh_indir = sb_bread(sb, global_disk);
               blk_id =  (uint16_t *)(bh_indir->b_data + chain[j] * 2);
               flag = 1; 
               j++;
         
        }
    }

*/


