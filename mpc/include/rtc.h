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

#define RTC_DEV_NAME "RTC"

#define RTC_IOCTL_MAGIC  0xFF

#define RTC_READ_SECONDS    _IOR(RTC_IOCTL_MAGIC, 0, unsigned char)
#define RTC_READ_MINUTES    _IOR(RTC_IOCTL_MAGIC, 1, unsigned char)
#define RTC_READ_HOUR       _IOR(RTC_IOCTL_MAGIC, 2, unsigned char)
#define RTC_READ_WEEKDAY    _IOR(RTC_IOCTL_MAGIC, 3, unsigned char)
#define RTC_READ_MONTHDAY   _IOR(RTC_IOCTL_MAGIC, 4, unsigned char)
#define RTC_READ_MONTH      _IOR(RTC_IOCTL_MAGIC, 5, unsigned char)
#define RTC_READ_YEAR       _IOR(RTC_IOCTL_MAGIC, 6, unsigned char)
#define RTC_READ_CENTURY    _IOR(RTC_IOCTL_MAGIC, 7, unsigned char)

#define RTC_IOCTL_MAXNR 7