#if !defined __BTREE_H__
#define __BTREE_H__

//#include "rubix_file_system.h"
#define DEGREE 3 
#define MIN 2  // If we just have a node with one node we merge it with its parent.
#define uint64_t long int 
#define true 1
#define false 0
#define boolean int 
/// FIXME:2 things now 1. to extend the write_nodifcation COW tree to inserts. Currently its only done for certain edits. 2.also complete the removal for trees.
// Remove works fine -  except for balancing the tree.

typedef struct __btree_node_
{
   uint64_t key[DEGREE +1]; // if the number of keys in the node increase then we need to split the node into 2 and update the nodes accordingly.
   uint64_t pba[DEGREE+1];
   struct __btree_node_ *child[DEGREE + 2];
   int idx;   //inx for the number of free entries in the node
   boolean leaf;
   int ref_count;
   int dirty;
}btree_node_t;


#endif // __BTREE_H__
