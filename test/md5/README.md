# MD5 device

The **/dev/md5** device allows you to compute the message digest of given data. Underground it actually creates a virtual device for every single terminal that opens the device, so every terminal's **/dev/md5** device is different.

```sh
$ echo "The quick brown fox jumps over the lazy dog" > /dev/md5
```

Read from device using **dd**:

```sh
$ dd if=/dev/md5 bs=16 count=1 | xxd
> 9e107d9d3...
```
