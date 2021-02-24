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
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/fs.h>

#include "mpc.h"

int             mpc_major = -1;
int             mpc_minor = MPC_FIRST_MINOR;
int             mpc_ndevs = 0;
struct class   *mpc_class = NULL;

/* Metadata */
MODULE_AUTHOR("José María Cruz Lorite");
MODULE_LICENSE("GPL");

/**
 * Configure permissions
 */
static int mpc_dev_uevent(struct device *dev, struct kobj_uevent_env *env) {
    add_uevent_var(env, "DEVMODE=%#o", MPC_DEV_MODE);
    return 0;
}

/**
 * Driver cleanup
 */
static void mpc_cleanup(void) {
    if (mpc_class) {
        mpc_stack_cleanup(mpc_class);
        mpc_md5_cleanup(mpc_class);
        mpc_rtc_cleanup(mpc_class);
        class_destroy(mpc_class);
    }

    if (mpc_major != -1) {
        unregister_chrdev_region(MKDEV(mpc_major, mpc_minor), mpc_ndevs);
        pr_info("mpc: released major %d \n", mpc_major);
    }
}

/**
 * Load the driver
 */
static int __init mpc_init_driver(void) {
    dev_t firstdev = 0, dev;

    // n (stacks) + 1 (md5) + 1 (rtc)
    mpc_ndevs = mpc_nstacks() + 1 + 1;

    /* allocate driver region */
    if (alloc_chrdev_region(&firstdev, mpc_minor, mpc_ndevs, MPC_DRIVER_NAME) < 0) {
        pr_err("mpc: unable to allocate '%s' region\n", MPC_DRIVER_NAME);
        goto error;
    }

    mpc_major = MAJOR(firstdev);
    dev = firstdev;
    pr_info("mpc: %d major assigned\n", mpc_major);

    /* create driver class */
    if((mpc_class = class_create(THIS_MODULE, MPC_CLASS_NAME)) == NULL) {
        pr_err("mpc: unable to create '%s' class\n", MPC_CLASS_NAME);
        goto error;
    } else
        mpc_class->dev_uevent = mpc_dev_uevent;

    /* initialize devices */
    dev += mpc_stack_init(dev, mpc_class);
    dev += mpc_md5_init(dev, mpc_class);
    dev += mpc_rtc_init(dev, mpc_class);

    pr_info("mpc: driver loaded\n");
    return 0;

    error:
    mpc_cleanup();
    return -1;
}

/**
 * Unload the driver
 */
static void __exit mpc_exit_driver(void) {
    mpc_cleanup();
    pr_info("mpc: driver unloaded\n");
}

module_init(mpc_init_driver)
module_exit(mpc_exit_driver)
