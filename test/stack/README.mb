# Stack device

The **/dev/stack** devices allows you to use them as stacks where you can push/pop data. Installing the driver you can specify how many stacks you want with the parameter **nstacks**. Every stack device is named following this syntax: /dev/stack{0,1,2,...}. For instance, **/dev/stack0**, **/dev/stack1**, etc...

Push data to the stack:

```sh
$ cat /proc/vmstat > /dev/stack0
```

Pop all data from the stack:

```sh
$ cat /dev/stack0
```

Pop certain amount of data from the stack:

```sh
$ dd if=/dev/stack8 bs=1024 count=1 | xxd
```
