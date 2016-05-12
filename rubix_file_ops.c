#include "rubix_file_system.h"

// FILE OPS- 
struct file_operations rubix_file_fops = 
{
  .read = do_sync_read , //rubix_file_read,
  //.llseek = generic_file_llseek,//rubix_file_llseek,
  //.open    = generic_file_open,//rubix_file_open,
  //.release = generic_file_close,//rubix_file_release, 
  .write = do_sync_write,
  .aio_read = generic_file_aio_read, 
  .aio_write = generic_file_aio_write,
  .fsync     = generic_file_fsync,
  //.readdir  = rubix_read_dir, // This should be implemented
};

struct inode_operations rubix_file_iops = 
{
   .getattr = rubix_getattr,
   .setattr = rubix_setattr,
   //.fsync   = rubix_sync_inodes, 
   //.create = rubix_create_inode,
   //.lookup =rubix_lookup_inode,
   //.readpage=rubix_readpage_inode,

   //.writepage=rubix_writepage_inode,
};

int rubix_getattr(struct vfsmount *mnt, struct dentry *dentry, struct kstat *stat)
{
    generic_fillattr(dentry->d_inode, stat);

    return 0;
}

int rubix_setattr(struct dentry *dentry, struct iattr *attr)
{
    int ret=0;
    struct inode *inode = dentry->d_inode;

    setattr_copy(inode ,attr);
    // copy the changes into the disk inode.
 
    rubix_sb_write_inode(inode , NULL);
    mark_inode_dirty(inode);

    return ret;
}

/*int rubix_sync_inodes(struct file *file, int datasync)
{
    // Commit all the inodes.
    // Get the Checkpoint region to find the inode map.
  

}*/
