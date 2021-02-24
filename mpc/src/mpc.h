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

#ifndef _MPC_H_
#define _MPC_H_

#define MPC_DRIVER_NAME "mpc"       // driver name
#define MPC_CLASS_NAME  "mpc_class"

#define MPC_DEV_MODE    0666        // device permissions
#define MPC_FIRST_MINOR 0           // first mpc minor

// variables used on main.c
extern int mpc_major;
extern int mpc_minor;
extern int mpc_ndevs;
extern struct class* mpc_class;

/**
 * Initialize stack devices.
 * @return Amount of stack devices created
 */
int mpc_stack_init(dev_t firstdev, struct class *cl);

/**
 * Cleanup stack devices.
 */
void mpc_stack_cleanup(struct class *cl);

/**
 * @return Number of stack devices created
 */
int mpc_nstacks(void);

/**
 * Initialize md5 devices.
 * @return Amount of devices created
 */
int mpc_md5_init(dev_t firstdev, struct class *cl);

/**
 * Cleanup md5 devices.
 */
void mpc_md5_cleanup(struct class *cl);

/**
 * Initialize Real Time Clock device.
 * @return Amount of devices created
 */
int mpc_rtc_init(dev_t firstdev, struct class *cl);

/**
 * Cleanup Real Time Clock device.
 */
void mpc_rtc_cleanup(struct class *cl);


#endif //_MPC_H_
