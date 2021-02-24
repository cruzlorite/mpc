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
#include <linux/ioctl.h>

#include "mpc.h"
#include "../include/rtc.h"

// *****************************************************************************
// *                            CMOS MEMORY                                    *
// *****************************************************************************

/** Read CMOS address. */
#define CMOS_READ(addr) ({ \
    outb_p((addr), CMOS_SEL_PORT); \
    inb_p(CMOS_REG_PORT); \
})

/** BCD to binary. */
#define BCD2BIN(x) ((x & 0x0F) + (x >> 4) * 10)

// CMOS layout
// @see https://wiki.osdev.org/CMOS
//
//  Register  Contents            Range
//  -----------------------------------
//  0x00      Seconds             0–59
//  0x02      Minutes             0–59
//  0x04      Hours               0–23 in 24-hour mode,
//                                1–12 in 12-hour mode, highest bit set if pm
//  0x06      Weekday             1–7, Sunday = 1
//  0x07      Day of Month        1–31
//  0x08      Month               1–12
//  0x09      Year                0–99
//  0x32      Century (maybe)     19–20?

#define CMOS_SEL_PORT   0x70   // CMOS select register port
#define CMOS_REG_PORT   0x71   // CMOS read/write register port

#define CMOS_SECONDS    0x00   // CMOS seconds register
#define CMOS_MINUTES    0x02   // CMOS minutes register
#define CMOS_HOUR       0x04   // CMOS hours register
#define CMOS_WEEKDAY    0x06   // CMOS weekday register
#define CMOS_MONTHDAY   0x07   // CMOS day register
#define CMOS_MONTH      0x08   // CMOS month register
#define CMOS_YEAR       0x09   // CMOS year register
#define CMOS_CENTURY    0x32   // CMOS century register

// *****************************************************************************
// *                            VARIABLES                                      *
// *****************************************************************************

static struct cdev  rtc_cdev;
static        dev_t rtc_devno;

// *****************************************************************************
// *                            FILE OPERATIONS                                *
// *****************************************************************************

/**
 * Open clock device.
 */
static int rtc_open(struct inode *inode, struct file *filp) {
    return nonseekable_open(inode, filp);
}

/**
 * Control RTC.
 */
long rtc_ioctl(struct file *filp, unsigned int cmd, unsigned long arg) {
    unsigned char retval = 0;

    // check the command exist
    if (_IOC_TYPE(cmd) != RTC_IOCTL_MAGIC)
        return -ENOTTY;
    if (_IOC_NR(cmd) > RTC_IOCTL_MAXNR)
        return -ENOTTY;

    switch (cmd) {
        case RTC_READ_SECONDS:
            retval = CMOS_READ(CMOS_SECONDS);   break;
        case RTC_READ_MINUTES:
            retval = CMOS_READ(CMOS_MINUTES);   break;
        case RTC_READ_HOUR:
            retval = CMOS_READ(CMOS_HOUR);      break;
        case RTC_READ_WEEKDAY:
            retval = CMOS_READ(CMOS_WEEKDAY);   break;
        case RTC_READ_MONTHDAY:
            retval = CMOS_READ(CMOS_MONTHDAY);  break;
        case RTC_READ_MONTH:
            retval = CMOS_READ(CMOS_MONTH);     break;
        case RTC_READ_YEAR:
            retval = CMOS_READ(CMOS_YEAR);      break;
        case RTC_READ_CENTURY:
            retval = CMOS_READ(CMOS_CENTURY);   break;
        default:
            return -ENOTTY;
    }

    return BCD2BIN(retval);
}

/**
 * RTC file operations struct.
 */
static const struct file_operations rtc_fops = {
        .owner          = THIS_MODULE,
        .llseek         = no_llseek,
        .open           = rtc_open,
        .unlocked_ioctl = rtc_ioctl
};

// *****************************************************************************
// *                            INIT/CLEANUP IMPLEMENTATION                    *
// *****************************************************************************

int mpc_rtc_init(dev_t firstdev, struct class *cl) {
    int err;

    rtc_devno = firstdev;

    /* setup rtc cdev */
    cdev_init(&rtc_cdev, &rtc_fops);
    if ((err = cdev_add(&rtc_cdev, rtc_devno, 1))) {
        pr_err("mpc: error %d adding clock device\n", err);
        return 0;
    }

    if(device_create(cl, NULL, rtc_devno, NULL, RTC_DEV_NAME) == NULL) {
        pr_err("mpc: clock device node creation failed\n");
        cdev_del(&rtc_cdev);
        return 0;
    }

    return 1;
}

void mpc_rtc_cleanup(struct class *cl) {
    device_destroy(cl, rtc_devno);
    cdev_del(&rtc_cdev);
}
