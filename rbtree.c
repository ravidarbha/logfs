#include "btree.h"
#include <stdio.h>
#include <stdlib.h>
/// This is not currently being used
// THis file contains the implementation of the rbtree for the lba to pbs address mapping.
// When we are unmounting we just write the whole tree as a inorder and postorder traversals and recreate the
// tree when we are back during mounting.
 
//1. Lets do it in steps. First lets implement the rotation of the trees. Then we can figure out the cases and implement it back.
typedef struct rbtree_node_
{
   int key; /// Lba used as a key to the mapping.
   int block; // pba // retreived by the search.
   struct rbtree_node_ *parent;
   struct rbtree_node_ *right;
   struct rbtree_node_ *left;
   enum {RED,BLACK}color;
}rbtree_node_t;

/// RB TREE specific

// LDR
void print_inorder(rbtree_node_t *root)
{
  if(root == NULL) return;
  else
  {
     print_inorder(root->left);
     printf("%d :%d \n",root->key,root->color);
     print_inorder(root->right);
  }
}


int search_rbtree(rbtree_node_t *root, int key)
{
    if(root == NULL) return 0;

    if(key == root->key) return 1;
    else
    {
        if(root->key > key)
        {
            if(search_rbtree(root->left, key))
            {
                return 1;
            }
        }
        else
        {
            if(search_rbtree(root->right, key))
            {
               return 1;
            }
        }
    }
}


rbtree_node_t *get_grandparent(rbtree_node_t *node)
{
   return node->parent->parent;
}

rbtree_node_t *get_parent(rbtree_node_t *node)
{
   return node->parent;
}

rbtree_node_t *get_uncle(rbtree_node_t *node)
{
   rbtree_node_t *g = get_grandparent(node);
   if(g == NULL)
   return NULL;
   if(node->parent == g->left)
       return g->right;
 
   if(node->parent == g->right)
   return g->left;

   return 0;
}

//////////////////////// ROTATIONS
///
/* Example 

      1                          2
     / \                        / \
     3  2       ====>          1   4
       /  \                   / \
       5   4                 3   5

 Return the new root for the subtree.*/
rbtree_node_t *left_rotation(rbtree_node_t *new)
{
    rbtree_node_t *tmp,*left;

    tmp = new->right;
    left = tmp->left;
    new->right = left;
    if(left)
    left->parent = new;
    tmp->left = new;
    if(new)
    new->parent = tmp;

    return tmp;
}

//// Example 
///
/*
      1                    3 
     / \                  / \
    3  2       ====>     5   1
   / \                      / \
   5 4                      4  2
*/
rbtree_node_t *right_rotation(rbtree_node_t *new)
{
   rbtree_node_t *tmp,*right;

   tmp = new->left;
   right = tmp->right;
   new->left = right;
   if(right)
   right->parent =new;
   tmp->right = new;
   if(new)
   new->parent = tmp;

   return tmp;
}

rbtree_node_t *create_node(int key,int pba)
{
   rbtree_node_t *node = (rbtree_node_t *)malloc(sizeof(rbtree_node_t)); //,GFP_KERNEL);
   node->key = key; // logical
   node->block = pba; // physical
   node->parent= NULL; // For now
   node->right = NULL; // For now
   node->left = NULL;
 
   return node;
}

rbtree_node_t *insert_into_tree(rbtree_node_t *root,rbtree_node_t *node)
{
   int flag= -1;
   rbtree_node_t *cur,*parent=root;
   cur = root;
   while(cur)
   {
       if (cur->key < node->key)
       {
           parent = cur;
           cur = cur->right;
           flag=1;
       }
       else if(cur->key > node->key)
       {
           parent = cur;
           cur = cur->left; 
           flag=0;
       }
   }

   if(flag)
   {
      parent->right = node;
      node->parent = parent;
   }
   else
   {
      parent->left = node;
      node->parent = parent;
   }

   return root;
}


//root has to be black and its children can also be black
//if node ,parent and uncle are red , push blackness from grandfather and recursively call on parent.
//if node,parent are red and uncle is NULL ,also node if the left of parent and parent is the left of grandfather then do right rotation on parent and parent becomes black and recursively call on the parent/grandfather (just check again) 
//if node,parent are red and uncle is NULL ,also node if the right of parent and parent is the right of grandfather then do left rotation on parent , parent becomes black and recursively call on the parent.grandfather. (just check again) 
 
rbtree_node_t *adjust_rbtree(rbtree_node_t *root, rbtree_node_t *node)
{
    rbtree_node_t *u,*g,*tmp;
    int left=0;

    if(node->parent == NULL)
    {
        node->color = BLACK;
        return node;
    }
    else
    {
        if (node->parent->color == BLACK) // NOthing to adjust
        {
            return root;
        }
        else
        {
            u = get_uncle(node);
            if ((u != NULL) && (u->color == RED))
            {
                node->parent->color = BLACK;
                u->color = BLACK;
                g  = get_grandparent(node);
                g->color = RED;
                return adjust_rbtree(root,g); // call recursively
            }
            else
            {
               g = get_grandparent(node);
 
               if ((node == node->parent->right) && (node->parent == g->left))
               {
                    left=0;
                    tmp = node->parent->parent;
                    if(tmp && tmp->left == node->parent)
                    {
                        left =1;
                    }
                    node = left_rotation(node->parent);
                    if(tmp)
                    {
                       if(left)
                       tmp->left = node;
                       else
                       tmp->right = node;
                    }
                    node->parent = tmp;
                    node = node->left;
               }
               else if ((node == node->parent->left) && (node->parent == g->right))
               {
                    left=0;
                    tmp = node->parent->parent;
                    if(tmp && tmp->left == node->parent)
                    {
                        left =1;
                    }

                    node = right_rotation(node->parent);
                    if(tmp)
                    {
                        if(!left)
                        tmp->right = node;
                        else
                        tmp->left = node;
                    }
                    node->parent = tmp;
                    node = node->right;
               }

               g = get_grandparent(node);
               node->parent->color = BLACK;
               g->color = RED;
               left=0;
               if (node == node->parent->left)
               {
                   tmp = g->parent;
                   if(tmp && tmp->left == g)  // left tree
                   left=1;

                   g = right_rotation(g);
 
                   if (tmp)
                   {
                      if(left)
                      tmp->left = g;
                      else
                      tmp->right = g;
                   }
                   else
                   {
                       root = g;  ///If tmp is NULL then we might have updated root
                   }
                   g->parent = tmp;
               }
               else
               {
                   tmp = g->parent;
                   if(tmp && tmp->left == g)  // left tree
                   left=1;
                   g = left_rotation(g);

                   if (tmp)
                   {
                      if(left)
                      tmp->left = g;
                      else
                      tmp->right = g;
                   }
                   else
                   {
                      root = g;
                   }
                   g->parent = tmp;
               }
               return root;
            }
        }
    }
}

// Will be called for every allocate block.
// key: is the lba for the file. We need a mapping into the pba
rbtree_node_t *insert(rbtree_node_t *root,int key, int pba)
{
    rbtree_node_t *node;
 
    // First create the node.
    node = create_node(key,pba);

    // First node in the tree.
    if(root == NULL)
    {
        // Init to a rb tree
        node->color = BLACK; /// root is always black.
        return node;
    }
    else
    {
        node->color = RED; // Always the added one is red.
        // First insert into a binary search tree and then do the adjustments
        root = insert_into_tree(root,node);
        // Now adjust based on our notes.
        root = adjust_rbtree(root, node); // call function so that we can recursively call later
    }
    return root;
}

// This will be implemented last as we need it only for file deletions.
rbtree_node_t *delete(void)
{

 return NULL;
}


// This function is invoked when we are unmounting. This writes the whole tree of the 
int write_tree_to_disk(void)
{
   return 0;
}

int main(int argc, char *argv[])
{
    rbtree_node_t *root=NULL;
 
    root = insert(root,7,8);
    root = insert(root,9,8);
    root = insert(root,5,8);
    root = insert(root,4,8);
    root = insert(root,2,8);
   
    // Test with printing inorder
     print_inorder(root);
    if(search_rbtree(root,9))
    {
       printf("Found");
    }
    else
     printf("Not found.\n");
    
}

