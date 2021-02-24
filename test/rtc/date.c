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

#include <stdio.h>
#include <stdlib.h>

#include <fcntl.h>      // open
#include <unistd.h>     // exit
#include <sys/ioctl.h>  // ioctl

#include "../../mpc/include/rtc.h"

// Day list
char *days[8] = { "",
        "Mon", "Tue", "Wed",
        "Thu", "Fri", "Sat", "Sun"};

// Month list
char *months[13] = { "",
        "Jan", "Feb", "Mar", "Apr",
        "May", "Jun", "Jul", "Aug",
        "Sep", "Oct", "Nov", "Dec"};

/**
 * Get time using mpc RTC device
 */
int main() {
    int fd = open("/dev/RTC", 0);

    if (fd < 0) {
        printf("Error opening /dev/RTC\n");
        exit(1);
    }

    unsigned char seconds   = ioctl(fd, RTC_READ_SECONDS);
    unsigned char minutes   = ioctl(fd, RTC_READ_MINUTES);
    unsigned char hour      = ioctl(fd, RTC_READ_HOUR);
    unsigned char weekday   = ioctl(fd, RTC_READ_WEEKDAY);
    unsigned char monthday  = ioctl(fd, RTC_READ_MONTHDAY);
    unsigned char month     = ioctl(fd, RTC_READ_MONTH);
    unsigned char year      = ioctl(fd, RTC_READ_YEAR);
    unsigned char century   = ioctl(fd, RTC_READ_CENTURY);

    printf("%s %s\t%d %d:%d:%d CET %d%d\n",
           days[weekday], months[month], monthday, hour, minutes, seconds, century, year);

    close(fd);
    return 0;
}

