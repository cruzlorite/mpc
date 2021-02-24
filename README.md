# Mi Primer Controlador (My First Linux Driver)

Simple Linux Driver that creates various devices:

* [/dev/md5](../test/md5)
* [/dev/RTC](../test/rtc)
* [/dev/stack](../test/stack)

## Build

Go to download folder and execute this:

```sh
$ cd mpc
$ make
```

# Test

You can install/remove the driver like this:

```sh
$ sudo insmod build/mpc.ko nstacks=0
$ sudo rmmod mpc.ko
```

With the parameter **nstacks** you can specify the amount of stacks devices.

Check debug messages with:

```sh
$ sudo dmesg
```

## License

This project is licensed under the GNU General Public License v3.0 - see the LICENSE file for details.

