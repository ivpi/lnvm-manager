# lnvm-manager (LightNVM Administrator and Tester)
A tool to manage and test LightNVM-enable devices, such as OpenChannel SSDs.

```
Features:
   See information about LightNVM devices;
   See available LightNVM target types in the kernel;
   Create a target on top of a device and expose the namespace in /dev;
   See info about the created target;
   Delete a target and remove namespace from /dev;
   Provisioning:
      Get a block from a specific LUN and mark it as in-use;
      Free a block (put) and mark it as free (it can be erased at any time);
   Write/Read a full block in a specific LUN;
   Write/Read an individual page within a block and specific LUN;
   Write/Read a range of sequential pages within a block and specific LUN;
   During IO operations (read/write) there is no output (use '-v' to see output)
```

The previous version of this software is 'lnvm' by Matias Bjørling:

<https://github.com/OpenChannelSSD/lnvm>

The previous version of 'lnvm' has been developed to manage the devices using ioctl directly and does not perform IO operations. Now, the linux kernel has been modified in order to work with 'liblightnvm', an user-space library to access OpenChannel SSDs:

<https://github.com/OpenChannelSSD/liblightnvm>

lnvm-manager now uses 'liblightnvm' and the new version of the kernel. The main difference in the kernel is that now is possible to access LUNs in parallel (get/put and read/write blocks from different Logical Units).

# How to use

You need to use the new LightNVM version, available in the kernel (install this kernel):

<https://github.com/OpenChannelSSD/linux/tree/liblnvm>

Then, install 'liblightnvm':

<https://github.com/OpenChannelSSD/liblightnvm>

Now, clone the repository:
```
$ git clone git@github.com:ivpi/lnvm-manager.git
$ make
```

You need root privilegies in order to manage LightNVM devices.

```
 Available commands:
   info            Show available lightNVM target types
   dev             Show registered lightNVM devices
   new             Init a target on top of a device
   rm              Remove a target from a device
   tgt             Show info about an online target (created by 'new' command)
   getblock        Get a block from a specific LUN (mark as in-use)
   putblock        Free a block (mark as free, it can be erased)
   write           Write data to a block
   read            Read data from a block
```

# lnvm info

Example:
```
  $ sudo ./lnvm info

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
  $ lnvm rm -n mydev
  $ lnvm rm mydev
  
  ### LNVM REMOVE TARGET ###
    LNVM Target removed succesfully. file: /dev/mydev
```

# lnvm tgt
```
 Options:
  -n, --tgtname=TGTNAME      Target name e.g. mydev

 Examples:
  $ lnvm tgt -n mydev
  $ lnvm tgt mydev

  ### LNVM TARGET INFO ###
   Target File: /dev/mydev
   Target Type: dflash (0,0,0)
   Device: nvme0n1
```

# lnvm getblock
```
   Options:
    -l, --lun=LUN              LUN id. <int>
    -n, --target=TARGET_NAME   Target name. e.g. 'mydev'
   
   Examples:
    lnvm getblock -l 1 -n mydev
    lnvm getblock -n mydev (without 'l' argument to pick a random LUN)
    
   ### LNVM GET BLOCK ###
    A block has been succesfully allocated.
    LUN: 1
    Block ID: 1030
    Block initial addr (bppa): 0x0000000000040600
    Nr of ppas (pages): 256
```

# lnvm putblock
```
   Options:
    -b, --blockid=BLOCK_ID     Block ID. <int>
    -l, --lunid=LUN_ID         LUN ID. <int>
    
   Examples:
    lnvm putblock -l 1 -b 1022 -n mydev
    lnvm putblock -l 0 -b 5 -n mydev
    
   ### LNVM PUT BLOCK ###
    Block 1022 from LUN 1 has been succesfully freed.
```

# lnvm write
```
We consider 256 pages per block for now (This info should come from kernel).
Use this command to overwrite the whole block, to write an indidual page or a
range of pages.

Each page written is filled with a human-readable byte sequency you can 
see in the 'read' command.

Use 'v' to see information and output during read/write.

 Full block: use only 'b' and 'n' keys
 Individual page: use only 'b', 'n' and 's' keys or 'p' = 1
 A range of pages: use all keys ('b','n','s' and 'p')

 Options:
  -b, --blockid=BLOCK_ID     Block ID. <int>
  -n, --target=TARGET_NAME   Target name. e.g. 'mydev'
  -p, --nr_pages=NUMBER_OF_PAGES   Number of pages to read
  -s, --page_start=PAGE_START   Page start ID within the block
  -v, --verbose              Print info and output to the screen
  
  Examples:
   lnvm write -b 1022 -n mydev (full block write)
   lnvm write -b 100 -s 10 -n mydev (individual page write)
   lnvm write -b 75 -n mydev -s 5 -p 10 (range page write. From page 5 to 14)
   lnvm write -b 1000 -n mydev -p 8 (range page write. From page 0 to 7)

   lnvm write -n volt -b 1000 -s 10 -p 2 -v
   
   ### LNVM BLOCK WRITE ###
   WRITING TO DEVICE...
   Total to be written: 8192 bytes
   Nr of pages: 2
   Page Size: 4096 bytes
   Write of 2 pages (10:11) in block 1000 performed succesfully.
   Total written: 8192 bytes
```

# lnvm read
```
We consider 256 pages per block for now (This info should come from kernel).
Use this command to read a full block, an invidual or a range of page.
Use 'v' to see information and output during read/write.

 Full block: use only 'b' and 'n' keys
 Individual page: use only 'b', 'n' and 's' keys or 'p' = 1
 A range of pages: use all keys ('b','n','s' and 'p')

 Options:
  -b, --blockid=BLOCK_ID     Block ID. <int>
  -n, --target=TARGET_NAME   Target name. e.g. 'mydev'
  -p, --nr_pages=NUMBER_OF_PAGES   Number of pages to read
  -s, --page_start=PAGE_START   Page start ID within the block
  -v, --verbose              Print info and output to the screen
  
  Examples:
   lnvm read -b 50 -n mydev (full block read)
   lnvm read -b 50 -s 10 -n mydev (individual page read)
   lnvm read -b 50 -n mydev > output.file (full block read with output file)
   lnvm read -b 50 -n mydev -s 5 -p 10 (range page read. From page 5 to 14)
   lnvm read -b 50 -n mydev -p 8 (range page read. From page 0 to 7)
   
   lnvm read -n volt -b 1000 -s 10 -p 2 -v
   
   ### LNVM BLOCK READ ###
    READING FROM DEVICE...
    Read of 2 pages (10:11) in block 1000 performed succesfully.
    Total read: 8192 bytes
    
    ############## READ OUTPUT ############### (use e.g  '> output.file' to write in a file)
    If you read a page written by this tool you will see an human-readable array of bytes.
    If you read an erased or empty page, the bytes will be transfered but no output will appear.
```
