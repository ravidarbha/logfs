//This file has all the functions related to test
#include "rubix_file_system.h"

//Building my hash which is easier.
int global_hash[20]={0}; // Only 20 inodes anyways. 

void build_hash(unsigned long ino)
{
   global_hash[ino] = 1;
}

void remove_hash(unsigned int ino)
{
   global_hash[ino]  = 0;
}

void dump_hash(void)
{
   int i;
   for(i=0;i<20;i++)
   {
      if(global_hash[i])
      printk("ino:%d",i);
   }
   printk("\n");
}

int search_hash(unsigned int ino)
{
    return (global_hash[ino]);
}

void dump_dentry(rubix_dir_entry_t *dir)
{
    BUG_ON(dir == NULL);

    printk(" Name: %s",dir->name);
    printk(" size:%d",dir->rec_len);
    printk(" inode:%d \n",dir->inode);
}

void dump_all_entry(struct inode *dir)
{
    int offset;
    struct buffer_head *bh;
    rubix_dir_entry_t *dientry = NULL;  
    rubix_inmem_t *in = rubix_create_inmem(dir);
    struct super_block *sb = dir->i_sb;

    offset = 0;
    bh = sb_bread(sb,in->dblocks[0]);
    while(offset < dir->i_size)
    {
        dientry = (rubix_dir_entry_t *)(bh->b_data  + offset); // Verifiying the recently written one.
        dump_dentry(dientry);
        offset+=64;
    }
}

// Keep this for now.
void verify_mark_inode(struct inode *inode)
{
    int was_dirty;
    BUG_ON(inode == NULL);
    was_dirty = inode->i_state & I_DIRTY;
 
    if (!was_dirty)
    {
         struct bdi_writeback *wb  = &inode->i_mapping->backing_dev_info->wb;
         struct backing_dev_info *bdi;
         BUG_ON(wb == NULL);
         bdi = wb->bdi;
 
         BUG_ON(bdi == NULL);
         if (bdi_cap_writeback_dirty(bdi) && !test_bit(BDI_registered, &bdi->state)) 
         {
            printk("bdi-%s not registered\n",bdi->name);
         }
         inode->dirtied_when = jiffies;
         list_move(&inode->i_wb_list, &bdi->wb.b_dirty);
    }
    else
    {
       printk("was dirty :%d",was_dirty);
    }
}
