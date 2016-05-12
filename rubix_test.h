#ifndef __RUBIX_TEST__
#define __RUBIX_TEST__

struct inode;
// Forward declaration.
struct _dir_entry;
 void build_hash(unsigned long ino);
 void remove_hash(unsigned int ino);

 void dump_hash(void);
 int search_hash(unsigned int ino);
 void dump_dentry(struct _dir_entry *dir);

 void dump_all_entry(struct inode *dir);
 void verify_mark_inode(struct inode *inode);
#endif // __RUBIX_TEST__
