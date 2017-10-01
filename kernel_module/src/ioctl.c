//////////////////////////////////////////////////////////////////////
//                             North Carolina State University
//
//
//
//                             Copyright 2016
//
////////////////////////////////////////////////////////////////////////
//
// This program is free software; you can redistribute it and/or modify it
// under the terms and conditions of the GNU General Public License,
// version 2, as published by the Free Software Foundation.
//
// This program is distributed in the hope it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
// more details.
//
// You should have received a copy of the GNU General Public License along with
// this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
//
////////////////////////////////////////////////////////////////////////
//
//   Author:  Hung-Wei Tseng
//
//   Description:
//     Skeleton of NPHeap Pseudo Device
//
////////////////////////////////////////////////////////////////////////

#include "npheap.h"

#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/poll.h>
#include <linux/mutex.h>
#include <linux/radix-tree.h>
#include <linux/sched.h>

/////////////////////////////////////// STUDENT CODE START ///////////////////////////
//Locking algo
//1. Lock tree
//2. find if object exists in tree
//3. if object exists
//4.    unlock tree
//5.    lock object
//6.    return 0
//7. else
//8.    create tree node
//9.    add node to tree
//10.   unlock tree
//11.   lock object
//12.   return 0
//Note: we unlock tree before locking object so as to not prevent other processes from accessing different objects.

//Unlocking algo
//1. Lock tree
//2. find if object exists in tree
//3. if object exists
//4.    unlock tree
//5.    unlock object
//6.    return 0
//7. else
//8.    unlock tree
//9 .   return error

//Getsize algo
//1. Lock tree
//2. find if object exists in tree
//3. if object exists
//4.    unlock tree
//5.    return object size
//7. else
//8.    unlock tree
//9.    return error

//Delete algo
//1. Lock tree
//2. find if object exists in tree
//3. if object exists
//4.    unlock tree
//5.    if data in object is not null
//6.        deallocate kernel data
//7.        set ibject size to 0
//8.        return 0
//9. else
//10.   unlock tree
//11.   return error



/**
 * @struct -    TreeNode
 *
 * @brief -     Structure of tree node
 *
 * @Modified by - Sweekrut and Vishal
 */
struct TreeNode {
    struct mutex object_lock;
    struct npheap_cmd object_data;
};

//Global data structure to store each allocated object
RADIX_TREE(Objects, GFP_ATOMIC);

//Global lock to handle tree concurrency
DEFINE_MUTEX(tree_lock);

/**
 * @brief - Initializes newly created tree object
 *
 * @Modified by - Sweekrut and Vishal
 */
void InitObjStruct (struct TreeNode *node, __u64 key)
{
    mutex_init(&(node->object_lock));
    node->object_data.op = 0;
    node->object_data.offset = key;
    node->object_data.size = 0;
    node->object_data.data = NULL;
}

/**
 * @brief -     Obtain lock on a single object
 *
 * @retval - 0 if successful, -EAGAIN or -EFAULT or -ENOMEM on failure
 *
 * @Modified by - Sweekrut and Vishal
 */
// If exist, return the data.
long npheap_lock (struct npheap_cmd __user *user_cmd)
{
    struct npheap_cmd user_data;
    __u64 key;
    struct TreeNode *tree_node = NULL;
    int retval = 0;
    int cpid = pid_nr(task_pid(current));

    if (copy_from_user(&user_data, user_cmd, sizeof(struct npheap_cmd)))
        return -EFAULT;

    key = user_data.offset/PAGE_SIZE;
    printk ("%d::npheap_lock: Enter, key = %llu\n", cpid, key);
    mutex_lock(&tree_lock);

    //Check if object exists
    tree_node = (struct TreeNode*)radix_tree_lookup(&Objects, key);

    //Object already exists
    if (tree_node) {
        //We dont need tree lock any more        
        mutex_unlock(&tree_lock);
        printk ("%d::npheap_lock: Locking found object for key = %llu\n", cpid, key);

        //Now we need to lock this object structure
        mutex_lock(&(tree_node->object_lock));
        
        //return once we have obtained lock on this object
        return retval;

    } else {
    //We need to allocate a new object holder and add to the tree

        printk ("%d::npheap_lock: No object for key = %llu\n", cpid, key);
        tree_node = kmalloc(sizeof(struct TreeNode), GFP_KERNEL);
        if (!tree_node) {
            mutex_unlock(&tree_lock);
            return -ENOMEM;
        }
       
        InitObjStruct (tree_node, key);

        //insert into tree
        retval = radix_tree_insert(&Objects, key, (void*)tree_node);

        //On success
        if (!retval){
            mutex_unlock(&tree_lock);
            printk ("%d::npheap_lock: Locking object key = %llu\n", cpid, key);
            //lock the object structure
            mutex_lock(&(tree_node->object_lock));
            return retval;
        } else {
        //This shouldnt happen, free allocated structure

            printk ("%d::npheap_lock: Insert failed for key = %llu\n", cpid, key);
            kfree(tree_node);
            retval = -EAGAIN;
        }
    }

    //Unlock tree in case of new object creation
    mutex_unlock(&tree_lock);

    return retval;
}   

/**
 * @brief - Function to unlock a locked object
 *
 * @retval - 0 if successful, -EAGAIN or -EFAULT on failure
 *
 * @Modified by - Sweekrut and Vishal
 */
long npheap_unlock(struct npheap_cmd __user *user_cmd)
{
    struct npheap_cmd user_data;
    __u64 key;
    struct TreeNode *tree_node = NULL;
    int cpid = pid_nr(task_pid(current));

    if (copy_from_user(&user_data, user_cmd, sizeof(struct npheap_cmd)))
        return -EFAULT;

    key = user_data.offset/PAGE_SIZE;
    printk ("%d::npheap_unlock: Enter, key = %llu\n", cpid, key);
    mutex_lock(&tree_lock);

    //Check if object exists
    tree_node = (struct TreeNode*)radix_tree_lookup(&Objects, key);

    //Object exists
    if (tree_node) {
        //We dont need tree lock any more 
        mutex_unlock(&tree_lock);
        printk ("%d::npheap_unlock: Found obj for key = %llu, unlocking\n", cpid, key);

        //Now we need to unlock this object structure
        mutex_unlock(&(tree_node->object_lock));
    
        //return once we have obtained lock on this object
        return 0;

    } else {
    //This should not happen
        
        printk("%d::npheap_unlock: Object not found at offset %llu\n", cpid, key);
        mutex_unlock(&tree_lock);

        return -EAGAIN;
    }

}

/**
 * @brief - Function to get size of an object
 *
 * @retval - 0 if successful, -EAGAIN or -EFAULT on failure
 *
 * @Modified by - Sweekrut and Vishal
 */
long npheap_getsize(struct npheap_cmd __user *user_cmd)
{
    struct npheap_cmd user_data;
    __u64 key;
    struct TreeNode *tree_node = NULL;
    __u64 size = 0;

    if (copy_from_user(&user_data, user_cmd, sizeof(struct npheap_cmd)))
        return -EFAULT;

    key = user_data.offset/PAGE_SIZE;
    printk ("npheap_lock: Enter, key = %llu\n", key);
    mutex_lock(&tree_lock);

    //Check if object exists
    tree_node = (struct TreeNode*)radix_tree_lookup(&Objects, key);

    //Object already exists
    if (tree_node) {
        //We dont need tree lock any more        
        mutex_unlock(&tree_lock);
        //Now we need to lock this object structure
        size = tree_node->object_data.size;
        return size;

    } else {
        mutex_unlock(&tree_lock);
        printk ("npheap_getsize: Object ofr key %llu not found\n", key);
        return -EAGAIN;
    }
}

/**
 * @brief - Function to delete an allocaetd object
 *
 * @retval - 0 if successful, -EAGAIN or -EFAULT on failure
 *
 * @Modified by - Sweekrut and Vishal
 */
long npheap_delete(struct npheap_cmd __user *user_cmd)
{
    struct npheap_cmd user_data;
    __u64 key;
    struct TreeNode *tree_node = NULL;

    if (copy_from_user(&user_data, user_cmd, sizeof(struct npheap_cmd)))
        return -EFAULT;

    key = user_data.offset/PAGE_SIZE;
    printk ("npheap_lock: Enter, key = %llu\n", key);
    mutex_lock(&tree_lock);

    //Check if object exists
    tree_node = (struct TreeNode*)radix_tree_lookup(&Objects, key);

    //Object exists
    if (tree_node) {
        //We dont need tree lock any more        
        mutex_unlock(&tree_lock);

        if (tree_node->object_data.data) {
            kfree(tree_node->object_data.data);
            tree_node->object_data.data = NULL;
            tree_node->object_data.size = 0;
        }

        //return once we have obtained lock on this object
        return 0;

    } else {
    //This shouldnt happen
        mutex_unlock(&tree_lock);
        printk ("npheap_delete: Object ofr key %llu not found\n", key);
        return -EAGAIN;
    }
}

/////////////////////////////////////// STUDENT CODE END ///////////////////////////

long npheap_ioctl(struct file *filp, unsigned int cmd,
                                unsigned long arg)
{
    switch (cmd) {
    case NPHEAP_IOCTL_LOCK:
        return npheap_lock((void __user *) arg);
    case NPHEAP_IOCTL_UNLOCK:
        return npheap_unlock((void __user *) arg);
    case NPHEAP_IOCTL_GETSIZE:
        return npheap_getsize((void __user *) arg);
    case NPHEAP_IOCTL_DELETE:
        return npheap_delete((void __user *) arg);
    default:
        return -ENOTTY;
    }
}
