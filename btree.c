#include "btree.h"
#include "rubix_file_system.h"
////////////////////////////////////////// ROTATIONS for rebalancing the tree after removals.
///
/* Example

      1                          2
     / \                        / \
     3  2       ====>          1   4
       /  \                   / \
       5   4                 3   5

 Return the new root for the subtree.*/
/*btree_node_t *left_rotation(btree_node_t *new)
{
    btree_node_t *tmp,*left;

    tmp = new->right;
    left = tmp->left;
    new->right = left;
    if(left)
    left->parent = new;
    tmp->left = new;
    if(new)
    new->parent = tmp;

    return tmp;
}*/

//// Example
///
/*
      1                    3
     / \                  / \
    3  2       ====>     5   1
   / \                      / \
   5 4                      4  2
*/
/*btree_node_t *right_rotation(btree_node_t *new)
{
   btree_node_t *tmp,*right;

   tmp = new->left;
   right = tmp->right;
   new->left = right;
   if(right)
   right->parent =new;
   tmp->right = new;
   if(new)
   new->parent = tmp;

   return tmp;
}*/
///////////////////////////////////////////////////////////////////////////

btree_node_t *create_node(int key, uint64_t pba)
{
   int i;
   btree_node_t *node;
   node = (btree_node_t *)kmalloc(sizeof(btree_node_t),GFP_KERNEL);
   node->key[0] = key; // THis is the first one.
   node->pba[0]=  pba;
   node->free=1;  // first in the node as its just created.
   for(i=0;i<DEGREE;i++)
   node->child[i] = NULL;

   return node;
}

void print_btree(btree_node_t *root)
{
  int i;
  if(root == NULL) return;
  else
  {
     for(i=0;i<=root->free+1;i++)
     {
        printk("%llu %llu ",root->key[i],root->pba[i]);
     }
     printk("\n");
     for(i=0;i<=root->free +1;i++)
     {
        if(root->child[i])
        print_btree(root->child[i]);
     }
  }
}

int middle_element(btree_node_t *root, int *idx, uint64_t *pba)
{
   *idx = (root->free)/2;
   *pba = root->pba[*idx];
   return root->key[*idx];
}

btree_node_t *insert_into_tree(btree_node_t *parent, btree_node_t *node, uint64_t *key, uint64_t pba)
{
    btree_node_t *child,*left;
    int i,idx,j,mid;
    uint64_t pba_mid;

    if(node == NULL) return node;  // termination.
    if(*key == 0 )  return node; // Nothing to change.

    i = node->free - 1;  // the max index to start with.
    while (i >= 0 && node->key[i] > *key)  // found the location.
    {
        i--;
    }
    // So now i+1 is either the insertion point or the child pointer to examine.
    child = node->child[i+1]; // as we are one short of i.
    // this means we have ti insert into the node and not its children.
    // THere exists a child so we have to go till the end of the recusrion.
    // we just dont care about the return code for now.
    insert_into_tree(node,child,key, pba);
    // after the recursion we are at the node to insert for sure.

    if(*key != 0)
    {
       // First just add them and then decide on split.
       j = node->free - 1;  // the max index to start with.

printk("Adding ..:%llu %llu\n",*key, node->key[j+2]);
       while (j >= 0 && node->key[j] > *key)  // found the location.
       {
           node->key[j+1] = node->key[j];
           node->child[j+1] = node->child[j];
           j--;
       }
       node->free++;
       node->key[j+1] = *key;   // Added the key to the node.
       node->pba[j+1] = pba;
    } /// IF key is 0 dont care.

  ///-------------------------------------------
    // Split based on the need else return.
    if(node->free > DEGREE)
    {
         //split_and_insert(parent, node, i, key);
         // This is the expansion of the split and insert 
         mid = middle_element(node, &idx, &pba_mid);
         // Then you have to create a new parent and update the key.
         // This is only be the case when the node is the root.
         /// Midlle element of the root.
         //copy from 0 to idx into another new node.
         left = create_node(0,0);

         for(i=0;i<idx;i++)
         {
             left->key[i] = node->key[i];
             left->pba[i] = node->pba[i];
             left->child[i] = node->child[i];
         }

         left->child[i] = node->child[i]; 
         // then move the
         left->free = idx;

         for(i=idx+1;i<=node->free;i++)
         {
             node->key[i- idx -1] = node->key[i];
             node->pba[i- idx -1] = node->pba[i];
             node->child[i-idx -1] = node->child[i];
         }
       
         node->free = node->free - idx -1 ;
         // Reset the nodes old values.
         for(i=node->free+1;i<DEGREE+2;i++)
         { 
             node->key[i] = 0;
             node->pba[i] = 0;
             node->child[i]= NULL;
         }

         if (parent == NULL)
         {
            parent = create_node(mid, pba_mid);
            parent->key[0] = mid; // This is the newly created parent.
            parent->pba[0] = pba_mid;
            // We are done here this is new root;
            *key=0;

            parent->child[0] = left;
            parent->child[1] = node;
            parent->free=1;
            print_btree(parent);
 
            return parent; // ONly this case i ll return back the new parent.
         }
         else
         {
             j = parent->free-1;
 
             // Find the parent position for the mid
             while (j >= 0 && parent->key[j] > mid)  // found the location.
             {  
                 // Copy off all the pointers to the next
                 parent->child[j+2] = parent->child[j+1]; // Just to make sure we copy all. 
                 parent->child[j+1] = parent->child[j]; 
                 parent->key[j+1] = parent->key[j];
                 parent->pba[j+1] = parent->pba[j];
                 j--;
             }
 
             parent->child[j+1] = left;
             parent->child[j+2] = node;
             parent->key[j+1] = mid;
             parent->pba[j+1] = pba_mid;
             parent->free = parent->free + 1; 
         }
         *key = 0; // This shows we recusrively habe to insert
          // This guy into the parent when we return.
          return NULL;

    }
    else // dont split now. just make *key as 0;
    {
        *key = 0;  // No more changes required for the tree.
    }
    //do it similar to rbtree. First add into the right node and then decide wether to split the node or no.
    return node;
}

btree_node_t *insertion(btree_node_t *root, uint64_t key, uint64_t pba)
{
   btree_node_t *node;

   if(root == NULL)
   {
       // If there are no nodes yet first create one with the key
       node = create_node(key,pba);

       return node;
   }
   else
   {
       root = insert_into_tree(NULL, root, &key, pba);
   }
   return root;
}
 
// This searches for the node in the node and return the node.
btree_node_t *search(btree_node_t *root, uint64_t key, uint64_t *idx, btree_node_t **parent,uint64_t *pidx)
{
    int i;
    btree_node_t *node=NULL,*child;
    // This is the number of free elements.
    i = root->free - 1;

    while (i >= 0 && root->key[i] >= key)  // found the location.
    {
        i--;
    }
    // i+1 is either the key to check or we have to go to the i+1 child.
    if (root->key[i+1] == key)
    {
        *idx = i+1;
        return root; 
    }
    else
    {
        child = root->child[i+1];
        if(child == NULL)
        {
            *idx =-1;
             return NULL;
        }
        else
        {
            node = search(child, key, idx, parent,pidx);
            // Used to clear nodes.
            if (node && !*parent)
            { 
               *parent = root;
               *pidx = i+1;
            }
            return node;
        }
    }
}

btree_node_t *search_node(btree_node_t *root, uint64_t key, uint64_t *idx)
{
   int64_t pidx; // the idx of the key in the node.
   btree_node_t *node=NULL,*parent=NULL;
    
   if(root ==  NULL) return NULL; 
   
   node = search(root, key, idx, &parent ,&pidx);
   //if(node)
    //printk("Seach :%llu :%llu\n ",node->key[idx],*idx);
   return node;
}
/*
btree_node_t * rebalance_tree(btree_node_t* parent, btree_node_t *root)
{
   if(node == NULL) return;

   if(node->free < MIN)
   {
       recursion(node, node->child[node->free-1]);
       recursion(node, node->child[node->free]);
   }
 
   //copy from left
   if(node->free < MIN)
   {
       merge node  

   }     
}
*/
// This is called only to clear the node when the free count goes to zero.
void clear_node(btree_node_t *parent, btree_node_t *node, int idx)
{
    kfree(node);
    parent->child[idx] = NULL;
}

// Keep updating idx everytime in the recursion.
// this once just removes the node and then updates the tree to keep it consistent.
void recursion(btree_node_t *parent, btree_node_t *node, int idx, btree_node_t *stop)
{
   if(node == NULL) return;

   recursion(node, node->child[node->free-1], idx, stop);
   recursion(node, node->child[node->free],idx, stop);
   //copy from left
   // ONly in this case we will just copy the right node element.
   if((node->child[node->free-1] == NULL) && (node->child[node->free] != NULL))
   {
       // COpy from the right.
       parent->key[parent->free-1] = node->key[node->free];
       node->free--;
   }
   else if(stop == parent)
   {
       // Copy from the left.
       parent->key[idx] = node->key[node->free-1];
       node->key[node->free-1] = 0;
       node->free--;
       if(node->free == 0 && parent)
       {
          clear_node(parent,node,idx);
       }
  //     rebalance(parent, node);
   }
   else
   {
       // for all the nodes copy.
       parent->key[parent->free-1] = node->key[node->free-1];
       node->key[node->free-1] = 0;
       node->free--;
   }
   return;
}

btree_node_t *remove_node(btree_node_t *root, uint64_t key)
{
   int i;
   uint64_t idx,pidx; // the idx of the key in the node.
   btree_node_t *node=NULL,*left,*parent=NULL;

   node = search(root, key, &idx, &parent, &pidx);

   if (node)
   {
      // if the children are NULL then we just overwrite.
      if(node->child[idx] == NULL && node->child[idx+1] == NULL)
      {
          // Overwrite the elements and the key from  idx+1;
          for(i=idx;i<DEGREE+1;i++)
          {
             node->key[i] = node->key[i+1];
             node->pba[i] = node->pba[i+1];
             node->child[i]= node->child[i+1];
          }
          node->free--;
          // FIXME: 
          //node->child[i]= node->child[i+1];

          if(node->free < MIN)
          {
              //rebalance_tree(node);
          }

          if(node->free == 0 && parent)
          {
             clear_node(parent,node,pidx);
          }

          return root;
      }
      //Now node is the node to be removed.
      if (node->child[idx])
      {
         // left child for this node now.
         left = node->child[idx];
         // copy this new
         recursion(node, left, idx, node);
      }
      // Rebalance the tree to decrease the depth.
  //    rebalance_tree(node);
   }
   else
   {
         printk("Couldnt find ..\n");
   }
   return root;
}

void copy_elements(btree_node_t *node, btree_node_t *new , int key, btree_node_t *nt, uint64_t pba)
{ 
   int i;
   for(i=0;i<key;i++)
   {
      new->key[i] = node->key[i];
      new->pba[i] = node->pba[i];
      new->child[i] = node->child[i];
   }
   new->key[i] = node->key[i];
   new->pba[i] = pba;
   new->child[i] = nt;

   for(i=key + 1; i <= node->free;i++)
   {
      new->key[i] = node->key[i];
      new->pba[i] = node->pba[i];
      new->child[i] = node->child[i];
   }
   // Have to use this instead of modifying the keys.
   new->dirty=1;
   node->dirty=1;
}

// ALgorithm here is to search for node first.  btree and sync it into the FS semantics.
// Btree modification of the update tree.
btree_node_t *search_write_modification(btree_node_t *root, int key, uint64_t pba)
{
    int i;
    btree_node_t *node=NULL,*child,*new_node;
    // This is the number of free elements.
    i = root->free - 1;

    while (i >= 0 && root->key[i] >= key)  // found the location.
    {
        i--;
    }
    // i+1 is either the key to check or we have to go to the i+1 child.
    if (root->key[i+1] == key)
    {
        // we have found the node so now create a new version for this node.
        new_node = create_node(0,0);
        //copy_nodes_into
        copy_elements(root,new_node, i+1,NULL, pba); 
        return new_node;
    }
    else
    {
        child = root->child[i+1];
        if(child == NULL)
        {
             return NULL;
        }
        else
        {
            node = search_write_modification(child, key ,pba);
            // Copy the elements into the new creation and the link it to the parent.
            if(node)
            {
                new_node = create_node(0,0);
                //copy_nodes_into
                copy_elements(root,new_node, i+1,node,pba);
                return new_node;
            }
            else
            {
                return node;
            }
        }
    }
}

/// pos is going to converted into a global logical address 
btree_node_t *insert_btree_node(btree_node_t *root, uint64_t lba , uint64_t pba)
{
    // THe algo here is to first search in the tree. If present then we need to modify the tree.
    // Else we need to  
    root = insertion(root, lba, pba);
print_btree(root);
    return root;
}

////////////////// LEVEL ORDER traversal for the btree and this will be saved. ////////////////////////////////////
int max_depth(int *depth)
{
  int max = depth[0],i;
 
  for(i=1;i<DEGREE;i++)
  {
     if(max < depth[i])
     max = depth[i];
  }
  return max;
}

int calculate_depth(btree_node_t *node)
{
    int depth[DEGREE];
    int i;

    if(node == NULL ) return 0;
 
    for(i=0;i<DEGREE;i++)
    {
        depth[i] = 1 + calculate_depth(node->child[i]);
    }
    max_depth(depth);
    return 0;
}

int level_order(btree_node_t *node, int d)
{
    int i;
    if(node == NULL)
    return 0;

    if(d == 0)
    {
        printk("%llu \n",node->key[0]);
        //commit_latest_tree(node);
    }
 
    for(i=0;i<DEGREE;i++)
    {
        level_order(node->child[i], d-1);
    }
    return 0;
}

void write_level_order_disk(btree_node_t *root)
{
    int i;
   int depth = calculate_depth(root);

   printk("depth:%d\n",depth);

   for(i=0;i<depth;i++)
   {
      level_order(root, i);
   }
   
}

/*
int main(int argc, char argv[])
{
    btree_node_t *root=NULL,*root_new=NULL;
    root = insertion(root, 2);
    root = insertion(root, 7);
    root = insertion(root, 4);
    root = insertion(root, 8);
    root = insertion(root, 6);
    root = insertion(root, 9);
    root = insertion(root, 1);
    root = insertion(root, 5);
    root = insertion(root, 11);
 
    root = insertion(root, 12);
    print_btree(root);
    root = remove_node(root, 9);
    print_btree(root);
    root = remove_node(root, 1);
    print_btree(root);
    remove_node(root, 7);
    print_btree(root);
    remove_node(root, 12);
    print_btree(root);
 
//    root_new = search_write_modification(root, 8);
  //  print_btree(root_new);
 
    return 0;
}*/
