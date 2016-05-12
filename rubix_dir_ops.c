#include "rubix_file_system.h"

// Directory 
struct file_operations rubix_dir_fops = 
{
  .read = generic_read_dir, //generic_file_aio_read , //rubix_file_read,
  .readdir  = rubix_read_dir,
  //.write  = generic_file_aio_write,  //rubix_file_write,
  .llseek = generic_file_llseek,
  //.open    = generic_file_open,
  //.release = generic_file_release, 
};

struct inode_operations rubix_dir_iops = 
{
   //.setattr = rubix_setattr,
     .getattr = rubix_getattr,
     .create = rubix_create_inode,
     .mkdir = rubix_mkdir_inode,
     .rmdir = rubix_rmdir_inode,
     .lookup  = rubix_lookup, 
     .unlink  = rubix_unlink,
};

extern struct buffer_head *read_ibmap(struct super_block *sb);
extern struct inode *rubix_iget(struct super_block *sb, unsigned long ino ,int mode);
extern void init_inode_links(struct inode *dir, struct inode *inode, int mode);
extern int alloc_disk_write(struct inode *inode, int blkcnt, uint64_t *pba, int alloc);
extern int inode_dentry_to_dir( struct inode *dir, struct dentry *dentry, struct inode *inode);
extern struct buffer_head *read_bbmap(struct super_block *sb);
extern void reclaim_data_blocks(struct inode *inode, int blk);
extern rubix_inode_t *get_rubix_inode_from_vfs(struct inode *inode, struct buffer_head **bh);
extern int alloc_dblock_from_bitmap(struct super_block *sb);
extern int alloc_inode_from_bitmap(struct super_block *sb);
extern void clear_dblock_bitmap(struct super_block *sb, int block);
extern void clear_inode_bitmap(struct super_block *sb, int block);
extern void clear_indirect_blocks(struct super_block *sb, int block);

// We traverse through the entries created in insert_dir_entry and then fill them
// using filldir function.
int rubix_read_dir(struct file *file, void *dirent, filldir_t filldir)
{
    struct dentry *de;
    struct inode *inode;
    struct super_block *sb;
    int offset; 
    rubix_inmem_t *raw_inode;
    rubix_dir_entry_t *dir_entry;
    struct buffer_head *bh;
    uint64_t blk;
    // Access the data .

    de = file->f_dentry;
    inode = de->d_inode;
    raw_inode = rubix_create_inmem(inode);
   
    sb = inode->i_sb;

    // First data block which contains the dir info 
    blk = raw_inode->dblocks[0];

    // Init it to 0 then we traverse through all the entries in the 
    // data block - move it by 64 bytes -rec_len of each entry.  
 
    if(file->f_pos >= 2)
    return 1;
    
    if(filldir(dirent, ".",1 , file->f_pos++, inode->i_ino, DT_DIR))
    {
printk("1..\n");
        return 0;
    }
   
    if(filldir(dirent, "..",2 , file->f_pos++, de->d_parent->d_inode->i_ino, DT_DIR))
    {
printk("2..\n");
        return 0;
    }

    offset = 0;
    bh = sb_bread(sb,blk);

    while(offset < inode->i_size)
    {
        dir_entry = (rubix_dir_entry_t *)(bh->b_data + offset); 
        if(dir_entry)
        {
            if(filldir(dirent,dir_entry->name,dir_entry->name_len,file->f_pos++, dir_entry->inode,dir_entry->file_type))
            {
                // Error
                return 0;
            }
        }
        offset+= 64;
    }
    brelse(bh);
 
    return 1;
}

/// INODE FILE OPERATIONS
// Here we get the inode from ur disk and the in memory inode structure. Link both and return
// VFS inode back to it to keep track.
int rubix_create_inode(struct inode *dir,struct dentry *dentry, int mode, bool ) //struct nameidata *ni)
{
    struct inode *inode;
    uint64_t pba;
    struct super_block *sb = dir->i_sb;
    int cnt =-1;

    cnt = alloc_inode_from_bitmap(sb);

    if(cnt == -1)
    {
       // cnt hasnt changed so it means we havnt found an inode
       // so we just return ERROR.
       return -ENOMEM;
    }
 
    // We are allocating the cnt number.
    // This already initializes the function callbacks.
    // We have to do +1 as we are statting the root from 1
    // in the inode and from 0 on the disk.
    inode = rubix_iget(sb, cnt ,mode);
    init_inode_links(dir, inode, mode);

    // If its a directory then allocate a block for writing entries into it.
    if(S_ISDIR(mode))
    {
        alloc_disk_write(inode,0, &pba ,1);
    }

    // Create the dir entry in the first data block in the parent as it becomes a directory.
    inode_dentry_to_dir(dir, dentry, inode);
    // Insert into the hash using insert_inode_hash(inode) is not required as we already use iget_locked
    // which inserts into the hash.
    // Get the dentry for this inode.
    d_instantiate(dentry, inode);
    mark_inode_dirty(inode);
    // mark the parent as dirty too.
    mark_inode_dirty(dir);

    return 0;
}

// This is a directory so just call create_inode with the DIR flag set.
// Create_inode function takes care of the DIR with the ifs for DIR.
int rubix_mkdir_inode(struct inode *inode, struct dentry *dentry, int mode)
{
    return rubix_create_inode(inode, dentry, mode | S_IFDIR, 0); //NULL);
}

// reset the in mem inode and then commit it.
void reset_inode(struct inode *inode)
{
    struct buffer_head *bh;
    int j;
    rubix_inode_t *rin = get_rubix_inode_from_vfs(inode, &bh);
    // reset the inode structure and then write to the disk.
    // Never mess up with the ondisk structure directly.
    rin->i_mode  =    0777;
    rin->i_uid   =    2;
    rin->i_gid   =    0;
    rin->i_size  =    0;

    rin->i_nlinks =    1;
    rin->i_blocks =   0;

    for(j=0;j<inode->i_blocks;j++)
    {
        struct buffer_head *bh1;
        uint64_t data_blk = rin->dblocks[j];

        if (data_blk != 0)
        {
           bh1 = sb_bread(inode->i_sb, data_blk);
           memset(bh1->b_data, 0, bh1->b_size);
           rin->dblocks[j] = 0;
           mark_buffer_dirty(bh1);
           brelse(bh1);
        }
    }
    mark_buffer_dirty(bh);
    brelse(bh);
}

// IN short this function just deletes the dir_entry of the inode to be deleted from the 
// directory entry.
int remove_entry_from_directory(struct inode *dir, struct dentry  *dentry)
{
    struct buffer_head *bh;
    rubix_dir_entry_t *dir_entry, *dir_entry1; 
    rubix_inmem_t *in = rubix_create_inmem(dir); 
    int offset,cnt=-1;
    struct super_block *sb = dir->i_sb; 
    // Dir entry should be removed.
    uint64_t data_blk = in->dblocks[0];
   
    BUG_ON(data_blk == 0);
   
    bh = sb_bread(sb, data_blk);
    offset = 0;
    dump_all_entry(dir);
    while(offset < dir->i_size)
    {
        // Search for the inode to remove
        dir_entry = (rubix_dir_entry_t *)(bh->b_data + offset);
        if(dir_entry)
        {
            // Found the match to be searched.
            if(!(memcmp(dentry->d_name.name, dir_entry->name, strlen(dentry->d_name.name))))
            { 
                cnt = offset;
                break;   
            }
        }
        offset+=64;
    }

    if(cnt != -1)   // If we find an entry only then do this .
    {
        offset = cnt;
        while(offset  < dir->i_size)
        {
            dir_entry = (rubix_dir_entry_t *)(bh->b_data + offset);   // write to here
            dir_entry1 = (rubix_dir_entry_t *)(bh->b_data + offset + 64);  // write from here 

            memcpy(dir_entry->name, dir_entry1->name,dir_entry1->name_len);
            dir_entry->inode = dir_entry1->inode;
            dir_entry->rec_len = dir_entry1->rec_len;
            dir_entry->name_len = dir_entry1->name_len;
            dir_entry->file_type = dir_entry1->file_type;

            offset+=64;
        }
    }
    dir->i_size -= 64; // Removing an entry so reduce the size.
    mark_buffer_dirty(bh);
    mark_inode_dirty(dir);
    brelse(bh);
  
    return 0;
}

// TODO:Should get this one too.
// Implementation in progress.  The main idea here is to remove the inode entry
// in the disk structures. dir is the inode of the directory and dentry is for the
// inode of the file needed to be removed.
//TODO:PROB:The inode inmem structures need the changes too. We need to update the
// inmem structures and some how make sure the data blocks of the removed inode can be
// reused later on.
int rubix_rmdir_inode(struct inode *dir, struct dentry *dentry)
{
#if TEST
    struct buffer_head *bh2;
    rubix_inode_t *ino;
#endif
    struct super_block *sb = dir->i_sb;
    struct inode* inode = dentry->d_inode;
    int i,io;
    // Free the in mem inode and the in disk inode
    rubix_inmem_t *in = rubix_create_inmem(inode);

printk("WHY is this being called ..\n");
    // Make this inode reusable by setting it to free again.
    clear_inode_bitmap(sb, inode->i_ino);
    for(i=0;i<inode->i_blocks;i++)
    {
        clear_dblock_bitmap(sb, in->dblocks[i]);
printk("clearing the blocks ..\n");
        reclaim_data_blocks(inode, in->dblocks[i]);
        in->dblocks[i] = 0; // resetting this inmem data blocks to 0.
    }
    remove_entry_from_directory(dir, dentry);

    // just decrease the count on the links.
    // these functions will mark the inodes dirty.
    inode_dec_link_count(dir);

    inode_dec_link_count(inode);
    // If its a dir then do it twice.
    if(S_ISDIR(inode->i_mode))
    {
        inode_dec_link_count(inode);
    }

    // This only resets the on disk structure.
    reset_inode(inode);

    io = inode->i_ino;
#if TEST // Do some testing here again. // This rubix inode should have been reset by now.

    ino = get_rubix_inode_from_vfs(inode, &bh2);
    if(ino->i_blocks != 0)
    {
       printk("Reset failed ..Check code\n");
       kassert(0);
    }
    else
    {
       printk("iblocks :%d \n",ino->i_blocks);
    }

    brelse(bh2);
    remove_hash(io);
#endif
    iput(inode);

    return 0;
}

// Here we read the 1st data block for the parent and then 
// search for the one with the name given. We then create 
// an inode and a dentry for that entry and add the dentry.
// Here the parameters inode is the direcroty you are looking in 
// and the dentry for the filename that has to be looked up.
struct dentry *rubix_lookup(struct inode *dir, struct dentry *dentry, struct nameidata *nd)
{
    struct buffer_head *bh;
    rubix_dir_entry_t *dir_entry = NULL;
    int offset,ino;
    struct super_block *sb = dir->i_sb;
    // The inode for the lookup.
    struct inode *inode = NULL;

    rubix_inmem_t *in = rubix_create_inmem(dir);
    uint64_t data_blk =  in->dblocks[0]; 
   
    // Get the data block. 
    bh = sb_bread(sb, data_blk);

    offset = 0;
 
    while(offset < dir->i_size)
    {
 
      dir_entry = (rubix_dir_entry_t *)(bh->b_data + offset);
      if(dir_entry)
      {
              if(!(memcmp(dentry->d_name.name, dir_entry->name, strlen(dentry->d_name.name))))
              {
                  //dir_entry is the match for the dentry specified in the parameters.
                  ino = dir_entry->inode;
                  // Then create the inode for this number.
                  if (dir_entry->file_type == 1)
                  {
                     inode = rubix_iget(sb, ino, S_IFREG);
                  }
                  else
                  {
                     inode = rubix_iget(sb, ino, S_IFDIR);
                  }
                  d_add(dentry, inode);
                  mark_buffer_dirty(bh);
                  brelse(bh);
                  return NULL;
              }
      }
      offset+=64; // This is going to be standard for every entry.
   }

   // at this point we have not found anything so just add NULL;
   d_add(dentry, NULL);
   brelse(bh);

   return NULL;
}


// THis is called for the rm function for all the files.
int rubix_unlink(struct inode *dir, struct dentry *dentry)
{
    return rubix_rmdir_inode(dir, dentry);
}


