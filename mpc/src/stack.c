// Copyright 2020 José María Cruz Lorite
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

#include <linux/module.h>
#include <linux/moduleparam.h>

#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/semaphore.h>
#include <linux/uaccess.h>
#include <linux/log2.h>

#include "mpc.h"

#define STACK_MIN_LOAD 3        // stack load factor
#define STACK_N_DEVS   3        // by default stack0 through stack2
#define STACK_MIN_SIZE 512      // minimum stack size
#define STACK_DEV_NAME "stack"  // stack device name

// Forward definition
struct stack;

// *****************************************************************************
// *                                VARIABLES                                  *
// *****************************************************************************

static int      nstacks     = STACK_N_DEVS;	// number of stack devices
static dev_t    stack_devno = -1;           // our first device number

static struct stack *stacks;                // list of stack devices allocated on initialization

/* register module parameter */
module_param_named(stacks, nstacks, int, S_IRUGO);
MODULE_PARM_DESC(stacks, "Number of stack devices");

// *****************************************************************************
// *                    STACK STRUCT AND RELATED FUNCTIONS                     *
// *****************************************************************************

/**
 * The device is a stack of memory where the user can push and pop data.
 */
struct stack {
    int minor;              ///< Device minor
    char *buffer;           ///< Device data buffer
    size_t psize;           ///< Buffer physical size
    size_t lsize;           ///< Buffer logical size
    struct semaphore sem;   ///< Mutual exclusion semaphore
    struct cdev cdev;	    ///< cdev struct identifying this device
};

/**
 * Reallocate memory for 'size' bytes and copy data to the new buffer.
 * If memory can't be allocated '-ENOMEN' is returned, 0 otherwise.
 */
int realloc_buffer(struct stack *dev, size_t size) {
    // allocate memory for new buffer
    void *new_buffer = kzalloc(size, GFP_KERNEL);
    if (!new_buffer)
        return -ENOMEM;

    // copy data on new buffer
    if (dev->buffer) {
        memcpy(new_buffer, dev->buffer, dev->lsize);
        kfree(dev->buffer);
    }

    dev->buffer = new_buffer;
    dev->psize = size;

    return 0;
}

/**
 * Grow up the buffer to the next power of two.
 */
int increase_buffer(struct stack *dev, size_t size) {
    size_t new_size = roundup_pow_of_two(size);
    return realloc_buffer(dev, new_size);
}

/**
 * Cut the buffer in half. If buffer size is equal to MPC_MIN_STACK_SIZE
 * the buffer is not modified.
 * If memory can't be allocated '-ENOMEN' is returned, 0 otherwise.
 */
int decrease_buffer(struct stack *dev) {
    if (dev->psize == STACK_MIN_SIZE)
        return 0;

    return realloc_buffer(dev, dev->psize / 2);
}

// *****************************************************************************
// *                            FILE OPERATIONS                                *
// *****************************************************************************

/**
 * Open stack device.
 */
static int stack_open(struct inode *inode, struct file *filp) {
    struct stack *dev;

    dev = container_of(inode->i_cdev, struct stack, cdev);
    filp->private_data = dev;

    if (down_interruptible(&dev->sem))
        return -ERESTARTSYS;

    if (!dev->buffer) { // allocate the buffer if needed
        if (increase_buffer(dev, STACK_MIN_SIZE)) {
            pr_info("mpc: stack%d: open: unable to allocate memory\n", dev->minor);
            up(&dev->sem);
            return -ENOMEM;
        } else {
            pr_info("mpc: stack%d: open: buffer initialized with %d bytes\n", dev->minor, STACK_MIN_SIZE);
        }
    }

    pr_info("mpc: stack%d: open: process %i(%s) successfully opened the device\n", dev->minor, current->pid,
            current->comm);
    up(&dev->sem);
    return nonseekable_open(inode, filp);
}

/**
 * Read 'count' bytes form top of stack
 */
static ssize_t stack_read(struct file *filp, char __user *ubuff, size_t count, loff_t *f_pos) {
    struct stack *dev = filp->private_data;
    char *read;
    int load;

    if (down_interruptible(&dev->sem))
        return -ERESTARTSYS;

    // check if there is something to read
    if (dev->lsize == 0) {
        up(&dev->sem);
        pr_info("mpc: stack%d: read: 0 bytes read\n", dev->minor);
        return 0; // read nothing
    }

    // where start reading
    count = min(count, dev->lsize);
    read = dev->buffer + (dev->lsize - count);

    if (copy_to_user(ubuff, read, count)) {
        up (&dev->sem);
        return -EFAULT;
    }
    dev->lsize -= count;

    // check min load
    if (dev->lsize != 0) {
        load = dev->psize / dev->lsize;
        if (load >= STACK_MIN_LOAD) {
            if (decrease_buffer(dev)) {
                pr_info("mpc: stack%d: read: unable to reduce buffer size\n", dev->minor);
            } else {
                pr_info("mpc: stack%d: read: buffer reduced to %zu bytes\n", dev->minor, dev->psize);
            }
        }
    }

    up(&dev->sem);
    pr_info("mpc: stack%d: read: %zu bytes read\n", dev->minor, count);
    return count;
}

/**
 * Push data on top of the stack.
 */
static ssize_t stack_write(struct file *filp, const char __user *ubuff, size_t count, loff_t *f_pos) {
    struct stack *dev = filp->private_data;
    char *write;
    size_t req_size; // required size

    if (down_interruptible(&dev->sem))
        return -ERESTARTSYS;

    // if we don't have sufficient memory
    req_size = dev->lsize + count;
    if (req_size > dev->psize) {
        if (increase_buffer(dev, req_size)) {
            up(&dev->sem);
            pr_err("mpc: stack%d: write: unable to grow up the buffer\n", dev->minor);
            pr_info("mpc: stack%d: write: 0 bytes written\n", dev->minor);
            return 0;
        } else {
            pr_info("mpc: stack%d: write: buffer resized to %zu bytes\n", dev->minor, dev->psize);
        }
    }

    write = dev->buffer + dev->lsize;
    if (copy_from_user(write, ubuff, count)) {
        up(&dev->sem);
        return -EFAULT;
    }
    dev->lsize += count;

    up(&dev->sem);
    pr_info("mpc: stack%d: write: %zu bytes written\n", dev->minor, count);
    return count;
}

/**
 * Release the device.
 */
static int stack_release(struct inode *inode, struct file *filp) {
    struct stack *dev = filp->private_data;
    pr_info("mpc: stack%d: release: process %i(%s) released the device\n", dev->minor, current->pid,
            current->comm);
    return 0;
}

/**
 * Stack file operations structure
 */
static const struct file_operations stack_fops = {
    .owner      = THIS_MODULE,
    .llseek     = no_llseek,
    .open       = stack_open,
    .read       = stack_read,
    .write      = stack_write,
    .release    = stack_release,
};

// *****************************************************************************
// *                            INIT/CLEANUP IMPLEMENTATION                    *
// *****************************************************************************

/**
 * Initialize one stack
 */
static void stack_init(struct stack *dev, struct class *cl, int index) {
    int err, devno = stack_devno + index;

    // init_MUTEX
    sema_init(&dev->sem, 1);

    dev->minor =    MINOR(devno);
    dev->buffer =   NULL;
    dev->psize =    0;
    dev->lsize =    0;

    /* setup cdev */
    cdev_init(&dev->cdev, &stack_fops);
    if ((err = cdev_add(&dev->cdev, devno, 1)))
        pr_err("mpc: error %d adding stack%d\n", err, dev->minor);

    if(device_create(cl, NULL, devno, NULL, STACK_DEV_NAME "%d", index) == NULL)
        pr_err("mpc: device node creation failed\n");
}

/**
 * Initialize stack devices.
 * Return the amount of stack devices created.
 */
int mpc_stack_init(dev_t firstdev, struct class *cl) {
    int i;

    // check nstacks is greater than zero
    if (nstacks < 1) {
        pr_info("mpc: 0 stack devices created\n");
        return 0;
    }

    stack_devno = firstdev;

    // allocate memory for stack devices
    stacks = kmalloc(nstacks * sizeof(struct stack), GFP_KERNEL);
    if (stacks == NULL) {
        pr_err("mpc: unable to allocate memory for stack devices\n");
        return 0;
    }

    // initialize stack devices
    for (i = 0; i < nstacks; i++)
        stack_init(&stacks[i], cl, i);

    return nstacks;
}

/**
 * Cleanup stack devices.
 */
void mpc_stack_cleanup(struct class *cl) {
    int i;

    if (stacks) {
        for (i = 0; i < nstacks; i++) {
            device_destroy(cl, stack_devno + i);
            cdev_del(&stacks[i].cdev);

            if (stacks[i].buffer)
                kfree(stacks[i].buffer);
        }

        kfree(stacks);
    }

    stacks = NULL;
}

int mpc_nstacks(void) {
    return nstacks;
}
