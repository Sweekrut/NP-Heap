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

extern struct miscdevice npheap_dev;

///////////////////////////////////////// STUDENT CODE START /////////////////////////////////

//mmap algo
//1. Lock tree
//2. find if object exists in tree
//3. if object exists
//4.    unlock tree
//5.    if data is not null
//6.        allocate kernel memory
//7.        map this to process virtual memory
//8.        return 0
//7.    else
//8.        map existing data pointer to process virtual memory
//9.        return 0
//10. else
//11.   unlock tree
//12.   return error


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

//extern variables from ioctl.c
extern struct mutex tree_lock;
extern struct radix_tree_root Objects;
 
/**
 * @brief - Function to allocate kernel memory and map it to user space
 *
 * @retval - 0 if successful, -EAGAIN or -ENOMEM on failure
 * @Modified by - Sweekrut and Vishal
 */
int npheap_mmap(struct file *filp, struct vm_area_struct *vma)
{
    __u64 key;
    struct TreeNode *tree_node = NULL;
    int *area;

    key = vma->vm_pgoff;

    printk ("npheap_mmap: key = %llu\n", key);

    mutex_lock(&tree_lock);

    //Check if object exists
    tree_node = (struct TreeNode*)radix_tree_lookup(&Objects, key);

    mutex_unlock(&tree_lock);

    //Object exists
    if (tree_node) {
        printk ("npheap_mmap: Got tree node for key = %llu\n", key);

        //No object exist
        if (!tree_node->object_data.size) {
            printk ("npheap_mmap: No object in tree node for key = %llu\n", key);
            tree_node->object_data.data = kmalloc(vma->vm_end - vma->vm_start, GFP_KERNEL);
            printk ("npheap_mmap: After kmalloc of size %lu, for key = %llu\n", vma->vm_end-vma->vm_start, key);
            
            if (!tree_node->object_data.data) {
                printk ("npheap_mmap: kmalloc for size %lu  for key = %llu failed\n", vma->vm_end - vma->vm_start, key);

                return -ENOMEM;
            }

            printk ("npheap_mmap: Address %lu and shifted address %lu for key = %llu\n", (unsigned long)area, (unsigned long)virt_to_phys((void*)((unsigned long)area)), key);

            if (remap_pfn_range(vma, vma->vm_start, virt_to_phys((void*)((unsigned long)tree_node->object_data.data)) >> PAGE_SHIFT, vma->vm_end - vma->vm_start, vma->vm_page_prot)) {
                printk ("npheap_mmap: 1st remap for key = %llu\n failed", key);
                kfree(tree_node->object_data.data);
                tree_node->object_data.data = NULL;
                return -EAGAIN;
            }

            tree_node->object_data.size = vma->vm_end - vma->vm_start;

            return 0;

        } else {
        //Object already exists
            
            if (!tree_node->object_data.data) {
                //this shouldnt happen
                printk("npheap_mmap: Object size is present but pointer is NULL for key %llu\n", key); 
                return -EAGAIN;
            }

            if (remap_pfn_range(vma, vma->vm_start, virt_to_phys((void*)((unsigned long)tree_node->object_data.data)) >> PAGE_SHIFT, vma->vm_end - vma->vm_start, vma->vm_page_prot)) {
                printk ("npheap_mmap: 2nd remap for key = %llu\n failed", key);
                return -EAGAIN;
            }
        }
    
    } else {
    //This shouldnt happen
        printk ("npheap_mmap: No object structure for key %llu found\n", key);
        return -EAGAIN;
    }

    return 0;
}


///////////////////////////////////////// STUDENT CODE END /////////////////////////////////

int npheap_init(void)
{
    int ret;
    if ((ret = misc_register(&npheap_dev)))
        printk(KERN_ERR "Unable to register \"npheap\" misc device\n");
    else
        printk(KERN_ERR "\"npheap\" misc device installed\n");
    return ret;
}

void npheap_exit(void)
{
    misc_deregister(&npheap_dev);
}

