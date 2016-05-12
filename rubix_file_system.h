#if !defined __FILESYSTEM__
#define __FILESYSTEM__
#include "rubix_mkfs.h"
#include "rubix_test.h"
#include "btree.h"
#include <linux/kernel.h> 
#include <linux/buffer_head.h>                                                                            
#include <linux/fs.h>
#include <linux/mount.h>
#include <linux/vfs.h>
#include <linux/blkdev.h>    
#include <linux/delay.h>
#include <linux/namei.h>
#include <linux/idr.h>

#define MAGIC 0xABC432
#define MAX_VERSIONS 5 //for checkpointing trees for datablks

typedef struct __indirect_blocks__
{   
   uint16_t dblocks[NUM_BLKS_PER_INODE];
   struct __indirect_blocks__ *level2; 

}indirect_blocks_t;

// If checkpointing is 
#if CHECKPOINT   
typedef struct _cp_version__
{
    struct __btree_node_ *btree_root[MAX_VERSIONS];
}checkpoint_version_t;

#endif

struct __btree_node_;
typedef struct __minix_inode
{
    //This contains the data blocks in the inmem inode.
    uint64_t dblocks[NUM_BLKS_PER_INODE];
    // Each node will have this entry. This inode is the VFS node
    // which have thave the data stored in here.
    struct inode vfs_inode;
/*#if CHECKPOINT   
    checkpoint_version_t *cp_ver;
#endif*/
    struct __btree_node_ *btree_root;
}rubix_inmem_t;

typedef struct t_rubix_super_block_
{
   // add some fields here 
}rubix_super_block_t;

// This is the least block which will be used to clean up for the groomer. 
/*typedef struct _erase_block_
{
   uint64_t eb_num;  /// This will distinctly tell the number on the drive. We then calculate the page_num and blk based on that. 
   uint64_t free_blks;
   uint64_t start_blk;
   uint64_t end_blk;
   uint64_t cur_blk; /// We are still writing only blk wise.
}EB;*/

typedef struct _rubix_info_sb
{
   struct buffer_head  *sb;
   struct buffer_head **ibmap;  // you dont know how many you need so just create a double pointer
   struct buffer_head **bbmap;  // same ditto.
   uint64_t map_arrays;
   uint64_t meta_data_blks;
   EB *append_point;
   inode_table *i_table; // Inmem for the inode table which is the key data structure to store things.
}rubix_sb_info_t;


// FUNCTION PROTOTYPES 
// 1. Filesystem functions
struct dentry *rubix_mount(struct file_system_type *fs_type, int flags, const char *dev_name, void *data);
int rubix_fill_super(struct super_block *sb, void *data , int silent);
void rubix_kill_super(struct super_block *sb);
void rubix_put_super(struct super_block *sb);

// 2. Super block functions
int rubix_sb_write_inode(struct inode *inode, struct writeback_control *wbc);
struct inode *rubix_sb_alloc_inode(struct super_block *sb);
void rubix_sb_destroy_inode(struct inode *inode);
void rubix_sb_lookup_inode(struct inode *inode);
void rubix_sb_evict_inode(struct inode *inode);

// 3. Inode functions - FILE
int rubix_getattr(struct vfsmount *mnt, struct dentry *dentry, struct kstat *stat);
int rubix_setattr(struct dentry *dentry, struct iattr *attr);

// 4. File functions
void rubix_file_open(struct inode *inode, struct file *file);
void rubix_file_release(struct inode *inode, struct file *file);

// 5. Inode functions - DIR 
int rubix_create_inode(struct inode *dir, struct dentry *dentry, int mode,bool ); //  struct nameidata *ni);
int rubix_mkdir_inode( struct inode *dir, struct dentry *dentry, int mode);

int rubix_rmdir_inode(struct inode *inode, struct dentry *dentry);
struct dentry *rubix_lookup(struct inode *dir, struct dentry *dentry, struct nameidata *nd);
int rubix_unlink(struct inode *inode, struct dentry *dentry);

// 6. Dir functions
int rubix_read_dir(struct file *file, void *dirent, filldir_t filldir);
  
// 7. Address space functions
int rubix_read_asop_page( struct file *file, struct page *page);
int rubix_write_asop_page( struct page *page, struct writeback_control *wbc);   // This wont be doing the actual writing.
int rubix_prepare_write_asop_page( struct file *file,struct address_space *mapping, loff_t pos, unsigned len, unsigned flags, struct page **page ,void **fsdata); // This does the actual writing // prepare_write
int rubix_write_end( struct file *file,struct address_space *mapping, loff_t pos, unsigned len, unsigned flags, struct page *page ,void *fsdata); // This does the actual writing // prepare_write


struct inode *rubix_sb_read_inode(struct inode *inode);

rubix_inmem_t *rubix_create_inmem(struct inode *inode);

#endif
