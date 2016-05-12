#include "rubix_mkfs.h"
#include <linux/fcntl.h>
#include <linux/fs.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <linux/types.h>
#define C_ASSERT(expr)  typedef int __C_ASSERT___ [(expr)?1:-1]
char *filename;

long int num_blks;
typedef unsigned long long uint64_t;

//FIXME:This is the next version of the log based filesystem. We are going to write only the super block and the table of inode
// entries into the disk. Rest everything is going to be used 
// This file is going to create the ondisk formatting of the filesystem and then it will be used to to keep track of the 
// information. 

/// So the LAYout for the filesystem is changing  

//  0 -  SUPER BLOCK
//  1 -  INODE BITMAP(1 bit per inode and 1024 * 8 inodes per block)
//  2 - NUM_BLKS - INODE BITMAP BLOCKs 
//  NUM_BLKS - 2*NUM_BLKS +1 - BLOCK BITMAP
//  NUM_BLKS * 64 - INODEs
//  All Meta data Other Data blocks

// Write a super_block structure memsetting it to all 0s.
void write_super_block()
{
    int fd = open(filename,O_RDWR,0);
    super_block_t sb;

    memset(&sb,0,sizeof(super_block_t)); 
    // Determine the size of the device first.
    ioctl(fd, BLKGETSIZE, &sb.free_blocks);
    //super_block.free_inodes = NUM_INODES_FS; // All are fre for now.
    sb.num_arrays = (sb.free_blocks)/(8 * 1024);  
    if (sb.free_blocks % (8 * 1024))
    {  // The extra blks.
        sb.num_arrays++;
    }
    num_blks = sb.num_arrays;
    // Starting from here. IN log based we do not have bitmaps at the beginning so dont care. Just 
    // start writing from the first block.
    super_block.append_point.cur_blk = 1 ;  // All are free after this.
    super_block.append_point.eb_num = 0; //the first eb to write too.
    super_block.append_point.start_blk = 0; 
    super_block.append_point.end_blk = NUM_BLKS_PER_EB; 
    super_block.append_point.free_blks = NUM_BLKS_PER_EB;
    super_block.append_point.LAST_EB = sb.free_blocks / NUM_BLKS_PER_EB;  
 
    write(fd, &sb,sizeof(super_block_t));
    close(fd);
}

void write_root_inode()
{
    rubix_inode_t inode;
    int i;
    int fd = open(filename,O_RDWR,0);

    C_ASSERT(sizeof(inode) == SIZE_ON_DSK_INODE);
    // 3 rd block to read
    lseek(fd, (BBMAP_INDEX + 2 * num_blks) * SIZE_ONE_BLOCK , SEEK_SET);
    memset(&inode,0,sizeof(rubix_inode_t)); 

    // NO init the root inode.
    inode.i_mode =     S_IFDIR | S_IRWXU; // (S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
    inode.i_atime =    time(NULL);
    inode.i_ctime =    time(NULL);
    inode.i_mtime =    time(NULL);
    inode.i_uid   =    2;
    inode.i_gid   =    0;
    inode.i_size =     0;
    inode.i_nlinks =   2;
    // Just check if its read correctly.
    inode.dblocks[0] =  2 * num_blks + 3;
    for(i=1;i<NUM_BLKS_PER_INODE;i++)
    inode.dblocks[i] = 0;
    inode.i_blocks =   1 ;//NUM_BLKS_PER_INODE;

    write(fd, &inode,sizeof(rubix_inode_t));
    close(fd);

#if TEST
    rubix_inode_t buf;
    fd = open(filename,O_RDWR,0);
    lseek(fd, (BBMAP_INDEX + 2 * num_blks) * SIZE_ONE_BLOCK , SEEK_SET);
  
    read(fd,&buf,sizeof(rubix_inode_t));
    if(buf.i_mode == inode.i_mode && buf.dblocks[0] == inode.dblocks[0])  
    {
        printf("Done with root.. :%d :%llu \n",buf.i_mode, buf.dblocks[0]);
    }
    else // Something went wrong in writing 
    {
        printf("Something is wrong .. check code ..\n");
        exit(0);
    }
    close(fd);
#endif 
 
}

void write_inode(int i)
{ 
    rubix_inode_t inode;
    int offset,ret,j;
    int fd = open(filename,O_RDWR,0);

    offset = sizeof(rubix_inode_t) * i;
    lseek(fd, ((BBMAP_INDEX + 2 * num_blks) * SIZE_ONE_BLOCK) + offset , SEEK_SET);

    memset(&inode,0,sizeof(rubix_inode_t)); 
 
    // NO init the root inode.
    inode.i_mode =     0777;
    inode.i_atime =    time(NULL);
    inode.i_ctime =    time(NULL);
    inode.i_mtime =    time(NULL);
    inode.i_uid   =    2;
    inode.i_gid   =    0;
    inode.i_nlinks =   1;
    inode.i_size =     0;
   
    for(j=0;j<NUM_BLKS_PER_INODE;j++)
    inode.dblocks[j] = 0;
    inode.i_blocks = 0;//   NUM_BLKS_PER_INODE;  // Max only 2 blocks per ino  // Max only 2 blocks per inodee
    ret = write(fd, &inode,sizeof(rubix_inode_t));
printf("len:%d",ret);
    close(fd);

#if TEST
    rubix_inode_t buf;
    fd = open(filename,O_RDWR,0);
    lseek(fd, ((BBMAP_INDEX + 2 * num_blks) * SIZE_ONE_BLOCK) + offset , SEEK_SET);
  
    read(fd,&buf,sizeof(rubix_inode_t));
    if(buf.i_mode == inode.i_mode && buf.dblocks[0] == inode.dblocks[0] && buf.i_size == inode.i_size &&
       buf.i_blocks == inode.i_blocks)  
    {
        printf("Writing other nodes.. :%d :%llu %d %lu %d \n",buf.i_mode, buf.dblocks[0],buf.i_blocks ,offset/sizeof(rubix_inode_t),offset);
    }
    else // Something went wrong in writing 
    {
        printf("Something is wrong .. check code ..\n");
        exit(0);
    }  
    close(fd);
#endif 
}

void write_bmap_inode()
{
    uint8_t bmap_inode[1024]; 
    int fd = open(filename,O_RDWR,0);
    int i;

    // 1st block to read
    lseek(fd,IBMAP_INDEX * SIZE_ONE_BLOCK , SEEK_SET);

    memset(&bmap_inode,0,1024);
    bmap_inode[0] |= (1 << 0);
    write(fd, &bmap_inode, 1024);
 
    // We need to the reset all the blocks.
    for(i=1;i<num_blks;i++)
    {
        memset(&bmap_inode,0,1024);
        write(fd, &bmap_inode, 1024);
    }

    close(fd);
#if TEST
    uint8_t test_byte;
   
    fd = open(filename,O_RDWR,0);
    lseek(fd, IBMAP_INDEX * SIZE_ONE_BLOCK , SEEK_SET);
  
    // Read only the first byte in the disk.
    read(fd,&test_byte,1);
   
   if(test_byte != 1)
   {
       printf("exiting inode bmap :%u",test_byte);
       exit(0);
   }
   else
   {
       printf("inode bmap :%u",test_byte);
   }
#endif 

}


// make it just one block for now.
void write_bmap_blocks()
{
    // A block full of pointers to allocation bitmaps of blocks
    uint8_t bmap_blocks[1024]; 
    int i;
    int  fd = open(filename,O_RDWR,0);
 
    lseek(fd, (BBMAP_INDEX + num_blks) * SIZE_ONE_BLOCK , SEEK_SET);

    // We need to the reset all the blocks.
    for(i=0;i<num_blks;i++)
    {
        memset(&bmap_blocks,0,1024);
        write(fd, &bmap_blocks, 1024);
    }

    // all bits are zeros. 
#if TEST 
    if(bmap_blocks[0] != 0)
    {
        printf("exiting block bitmap :%u",bmap_blocks[0]);
        exit(0);
    }
    else // check if the left most is set to 0;
    {
        if((bmap_blocks[0]) & (1 << 7))
        {
            printf("exiting wdith :%ld",sizeof(bmap_blocks[0]));
            exit(0); 
        }
    }
#endif
    // written 8 blocks so lets seek to the new position on the block.
    close(fd);
}

// The argv should receive the block interface device which should create our filesytem.
// This sb from the mounted filesystem is going to be used to write into the block.
int main(int argc, char *argv[])
{
    int i;
    filename = argv[1];

    if (filename == NULL) 
    {
        printf("Enter a file name which will be used to keep track.\n");
        exit(0);
    }

    // Block 0
    // This will create the superblock for the filesystem.
    write_super_block();

printf("numblks :%ld \n",num_blks);
    // Block 1to num_blocks.
    write_bmap_inode();

printf("done.2..\n");    
    // Data blocks
    write_bmap_blocks();

printf("done.3..\n");    
    // This will create the root inode.
    write_root_inode();
 
printf("done.4..\n");    
    for(i=1;i<NUM_INODES_FS;i++)
    // write all other inodes.
    { 
        write_inode(i);
    }
#if TEST
    uint8_t test_byte;
   
    int fd = open(filename,O_RDWR,0);
    lseek(fd, IBMAP_INDEX * SIZE_ONE_BLOCK , SEEK_SET);
  
    // Read only the first byte in the disk.
    read(fd,&test_byte,1);
   
   if(test_byte != 1)
   {
       printf("exiting inode bmap :%u",test_byte);
       exit(0);
   }
   else
   {
       printf("tail inode bmap :%u",test_byte);
   }
#endif 

}
