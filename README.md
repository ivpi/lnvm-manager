# lnvm-manager (LightNVM Administrator)
A tool to manage LightNVM-enable devices, such as OpenChannel SSDs.

This software is based on 'lnvm' by Matias Bjørling:

<https://github.com/OpenChannelSSD/lnvm>

The previous version of 'lnvm' has been developed to manage the devices using ioctl directly. The linux kernel has been modified in order to work with 'liblightnvm', a user-space library to access OpenChannel SSDs:

<https://github.com/OpenChannelSSD/liblightnvm>

This new version of 'lnvm' uses 'liblightnvm' and the new version of the kernel. The main difference in the new LightNVM is that now is possible to access device LUNs in parallel (get and put blocks from different Logical Units).

# How to use

You need to use the new LightNVM version, available in the kernel (install this kernel):

<https://github.com/OpenChannelSSD/linux/tree/liblnvm>

Then, install 'liblightnvm':
<https://github.com/OpenChannelSSD/liblightnvm>

Now, clone the repository:
$ git clone git@github.com:ivpi/lnvm-manager.git
$ make

You need root privilegies in order to manage LightNVM devices.

```
Available commands:
   tgt     Show registered lightNVM targets
   dev     Show registered lightNVM devices
   new     Init a target on top of a device
   rm      Remove a target from a device
```

# lnvm tgt

Example:
```
  $ sudo ./lnvm tgt

  ### LNVM TARGET TYPES ###
    LightNVM version (1, 0, 0). 2 target type(s) registered.
      Type: dflash (0, 0, 0)
      Type: rrpc (1, 0, 0)
```

# lnvm dev

Example:
```
  $ sudo ./lnvm dev
  
  ### LNVM DEVICES ###
  nr_devices: 1

  Device: nvme0n1. Managed by gennvm (0, 1, 0)
    Sector size: 4096
    Sectors per page: 1
    Nr of planes: 4
    LUNs per channel: 32
    Nr of Channels: 8
    Plane mode: 2
    max_sec_io: 64
    oob_size: 0
```

# lnvm new

```
 Options:
  -d, --device=DEVICE        LightNVM device e.g. nvme0n1
  -l, --luns=LUNS            LUNs e.g. 0:3
  -n, --tgtname=TGTNAME      Target name e.g. mydev
  -t, --tgttype=TGTTYPE      Target type e.g. dflash
```

Example:
```
  $ sudo ./lnvm new -d nvme0n1 -t dflash -n mydev -l 0:3

  ### LNVM CREATE TARGET ###
    LNVM Target created succesfully. Device: nvme0n1, target: dflash, file: /dev/mydev
```

Look to /dev and you can see /dev/nvme0n1 device file.

# lnvm rm
```
 Options:
  -n, --tgtname=TGTNAME      Target name e.g. mydev
  
Examples:
  $ lnvm rm -n tgt0
  $ lnvm rm tgt0
  
  ### LNVM REMOVE TARGET ###
    LNVM Target removed succesfully. file: /dev/mydev
```
