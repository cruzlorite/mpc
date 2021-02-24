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
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/list.h>
#include <linux/tty.h>
#include <linux/sched/signal.h>

#include "mpc.h"

#define MD5_DEV_NAME "md5"  // device name
#define MD5_HASH_SIZE 16    // length on bytes

// *****************************************************************************
// *                            MD5 IMPL                                       *
// *****************************************************************************

// Per-round shift amounts
static const uint32_t r[] = {7, 12, 17, 22,  7, 12, 17, 22,  7, 12, 17, 22,  7, 12, 17, 22,
                             5,  9, 14, 20,  5,  9, 14, 20,  5,  9, 14, 20,  5,  9, 14, 20,
                             4, 11, 16, 23,  4, 11, 16, 23,  4, 11, 16, 23,  4, 11, 16, 23,
                             6, 10, 15, 21,  6, 10, 15, 21,  6, 10, 15, 21,  6, 10, 15, 21};

// Use binary integer part of the sines of integers (in radians) as constants
static const uint32_t k[] = {0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
                             0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
                             0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
                             0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
                             0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
                             0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
                             0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
                             0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
                             0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
                             0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
                             0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
                             0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
                             0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
                             0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
                             0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
                             0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391};

// left rotate function definition
#define LEFTROTATE(x, c) (((x) << (c)) | ((x) >> (32 - (c))))

/**
 * Original from https://gist.github.com/creationix/4710780
 */
ssize_t md5(uint32_t *h, const char __user *ubuff, size_t initial_len) {
    // Message (to prepare)
    uint8_t *msg = NULL;
    uint32_t temp, bits_len;
    int new_len, offset;

    h[0] = 0x67452301;
    h[1] = 0xefcdab89;
    h[2] = 0x98badcfe;
    h[3] = 0x10325476;

    // Pre-processing: adding a single 1 bit
    //append "1" bit to message
    /* Notice: the input bytes are considered as bits strings,
       where the first bit is the most significant bit of the byte.[37] */

    // Pre-processing: padding with zeros
    //append "0" bit until message length in bit ≡ 448 (mod 512)
    //append length mod (2 pow 64) to message
    new_len = ((((initial_len + 8) / 64) + 1) * 64) - 8;

    /* allocate memory */
    msg = kmalloc(new_len + 64, GFP_KERNEL); // also appends "0" bits (we alloc also 64 extra bytes...)
    if (!msg)
        return -ENOMEM;
    memset(msg, 0, new_len + 64);

    /* copy from user space to kernel space */
    if (copy_from_user(msg, ubuff, initial_len))
        return -EFAULT;
    //memcpy(msg, initial_msg, initial_len);

    msg[initial_len] = 128; // write the "1" bit

    bits_len = 8*initial_len; // note, we append the len
    memcpy(msg + new_len, &bits_len, 4); // in bits at the end of the buffer

    // Process the message in successive 512-bit chunks:
    //for each 512-bit chunk of message:
    for(offset=0; offset<new_len; offset += (512/8)) {

        // break chunk into sixteen 32-bit words w[j], 0 ≤ j ≤ 15
        uint32_t *w = (uint32_t *) (msg + offset);

        // Initialize hash value for this chunk:
        uint32_t a = h[0];
        uint32_t b = h[1];
        uint32_t c = h[2];
        uint32_t d = h[3];

        // Main loop:
        uint32_t i;
        for(i = 0; i<64; i++) {
            uint32_t f, g;

             if (i < 16) {
                f = (b & c) | ((~b) & d);
                g = i;
            } else if (i < 32) {
                f = (d & b) | ((~d) & c);
                g = (5*i + 1) % 16;
            } else if (i < 48) {
                f = b ^ c ^ d;
                g = (3*i + 5) % 16;
            } else {
                f = c ^ (b | (~d));
                g = (7*i) % 16;
            }
             
            temp = d;
            d = c;
            c = b;
            b = b + LEFTROTATE((a + f + k[i] + w[g]), r[i]);
            a = temp;
        }

        // Add this chunk's hash to result so far:
        h[0] += a;
        h[1] += b;
        h[2] += c;
        h[3] += d;
    }

    // cleanup
    kfree(msg);

    return 0;
}

// *****************************************************************************
// *                            VARIABLES                                      *
// *****************************************************************************

/**
 * Every tty has its own device.
 */
struct tty_listitem {
    dev_t key;                      ///< tty key
    uint8_t hash[MD5_HASH_SIZE];    ///< Message-Digest buffer
    size_t index;                   ///< Displacement on hash buffer (maybe the buffer is not read wholy)
    struct list_head list;          ///< Pointer to the list head    
};


// the list of devices and a lock to protect it
static LIST_HEAD(tty_list);
static DEFINE_SPINLOCK(tty_list_lock);

// md5 device
static struct cdev  md5_cdev;
static        dev_t md5_devno;

/**
 * Look for a device or create one if missing.
 * @param key, Terminal key
 * @return The correspondent tty_listitem
 */
static struct tty_listitem *md5_lookfor_tty(dev_t key) {
    struct tty_listitem *lptr;

    list_for_each_entry(lptr, &tty_list, list) {
        if (lptr->key == key)
            return lptr;
    }

    lptr = kmalloc(sizeof(struct tty_listitem), GFP_KERNEL);
    if (!lptr) /* no memory */
        return NULL;

    /* init tty */
    memset(lptr, 0, sizeof(struct tty_listitem));
    lptr->key = key;
    lptr->index = 0;

    list_add(&lptr->list, &tty_list);

    return lptr;
}

// *****************************************************************************
// *                            DEVICE OPERATIONS                              *
// *****************************************************************************

/**
 * Open md5 device.
 * The device must be opened from a tty.
 */
static int md5_open(struct inode *inode, struct file *filp) {
    struct tty_listitem *tty_item;
    dev_t key;

    if (!current->signal->tty) {
        pr_err("mpc: md5: process \"%s\" has no ctl tty\n", current->comm);
        return -EINVAL;
    }
    key = tty_devnum(current->signal->tty);

    /* look for tty in the list */
    spin_lock(&tty_list_lock);
    tty_item = md5_lookfor_tty(key);
    spin_unlock(&tty_list_lock);

    if (!tty_item) /* no tty_item because kmalloc error */
        return -ENOMEM;

    filp->private_data = tty_item;

    return nonseekable_open(inode, filp);
}

/**
 * Read data from user buffer and compute message digest.
 */
static ssize_t md5_write(struct file *filp, const char __user *ubuff, size_t count, loff_t *f_pos) {
    struct tty_listitem *tty_item = filp->private_data;
    ssize_t err = md5((uint32_t*) tty_item->hash, ubuff, count);
    /* don't reset buff index when err */
    tty_item->index = err ? tty_item->index : 0;
    return err ? err : count;
}

/**
 * Read (if available) message digest.
 */
static ssize_t md5_read(struct file *filp, char __user *ubuff, size_t count, loff_t *f_pos) {
    struct tty_listitem *tty_item = filp->private_data;

    count = min(count, MD5_HASH_SIZE - tty_item->index);

    if (copy_to_user(ubuff, tty_item->hash + tty_item->index, count))
        return -EFAULT;

    tty_item->index += count;

    return count;
}

/**
 * MD5 file operations.
 */
static const struct file_operations md5_fops = {
        .owner      = THIS_MODULE,
        .llseek     = no_llseek,
        .open       = md5_open,
        .read       = md5_read,
        .write      = md5_write,
};

// *****************************************************************************
// *                            INIT/CLEANUP IMPLEMENTATION                    *
// *****************************************************************************

int mpc_md5_init(dev_t firstdev, struct class *cl) {
    int err;

    md5_devno = firstdev;

    /* setup md5 cdev */
    cdev_init(&md5_cdev, &md5_fops);
    if ((err = cdev_add(&md5_cdev, md5_devno, 1))) {
        pr_err("mpc: error %d adding md5\n", err);
        return 0;
    }

    if(device_create(cl, NULL, md5_devno, NULL, MD5_DEV_NAME) == NULL) {
        pr_err("mpc: md5 device node creation failed\n");
        cdev_del(&md5_cdev);
        return 0;
    }

    return 1;
}

void mpc_md5_cleanup(struct class *cl) {
    struct tty_listitem *lptr, *next;

    device_destroy(cl, md5_devno);
    cdev_del(&md5_cdev);

    list_for_each_entry_safe(lptr, next, &tty_list, list) {
        list_del(&lptr->list);
        kfree(lptr);
    }
}
