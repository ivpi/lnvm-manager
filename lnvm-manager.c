/*  This software is based on 'lnvm' by Matias Bjørling
    Now, instead of direct ioctl calls, we manage lightNVM
    devices through 'liblightnvm'.

    The main improvement is the parallelism among LUNs.
    Now, it is possible to pick blocks from different LUNs,
    and read/write in parallel.

    Writes/reads can be performed in a full block, individual
    page or a range of pages sequentially. 
*/        

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <argp.h>
#include <string.h>
#include <linux/lightnvm.h>
#include <liblightnvm.h>
#include <linux/types.h>
#include "lnvm-manager.h"

static int get_dev_info(char * tgt_name, struct nvm_dev_info *info)
{
    static struct nvm_ioctl_dev_prop *dev_prop;
    static struct nvm_ioctl_tgt_info tgt_info; 
    static struct nvm_ioctl_dev_info dev_ioctl_info;
    int ret = 0;

    sprintf(tgt_info.target.tgtname, "%s", tgt_name);
    ret = nvm_get_target_info(&tgt_info);
    if (ret) {
        printf("nvm_get_target_info error. Failed to get target info.\n");
        goto out;
    }

    strncpy(dev_ioctl_info.dev, tgt_info.target.dev, DISK_NAME_LEN);

    ret = nvm_get_device_info(&dev_ioctl_info);
    if (ret)
        goto out;

    dev_prop = &dev_ioctl_info.prop;
    
    info->sec_size = dev_prop->sec_size;
    info->page_size = info->sec_size * dev_prop->sec_per_page;
    info->pln_pg_size = info->page_size * dev_prop->nr_planes;
    info->pg_sec_ratio = info->pln_pg_size / info->sec_size;

    info->pg_per_blk = PGS_PER_BLK;
out:
    return ret;
}

static void lnvm_show_info(struct arguments *args)
{
    struct nvm_ioctl_info c;
    int ret, i;

    memset(&c, 0, sizeof(struct nvm_ioctl_info));
    
    ret = nvm_get_info(&c);
    if(ret){
        printf("nvm_get_info error. Do you have root privilegies? "
                                        "If yes, look to 'dmesg'. %d\n",ret);
        return;
    }                     

    printf("\n### LNVM TARGET TYPES ###\n");
    printf(" LightNVM version (%u, %u, %u). %u target type(s) registered.\n",
                            c.version[0],c.version[1],c.version[2],c.tgtsize);

    for(i=0; i<c.tgtsize; i++){
        struct nvm_ioctl_tgt_info *tgt = &c.tgts[i];
        printf("  Type: %s (%u, %u, %u)\n", tgt->target.tgtname,
                        tgt->version[0],tgt->version[1],tgt->version[2]);
    }
    printf("\n");
}

static void lnvm_show_devices(struct arguments *args)
{
    struct nvm_ioctl_get_devices devs;
    int ret, i;

    ret = nvm_get_devices(&devs);
    if (ret) {
        printf("nvm_get_devices error. Do you have root privilegies? "
                                    "If yes, look to 'dmesg'. %d\n",ret);        
        return;
    }

    printf("\n### LNVM DEVICES ###\n");
    printf(" nr_devices: %u\n", devs.nr_devices);

    for (i = 0; i < devs.nr_devices && i < 31; i++) {
        struct nvm_ioctl_dev_info *dev = &devs.info[i];
        struct nvm_ioctl_dev_info info;
        uint32_t pg_size, pln_pg_size;
        
        strncpy(info.dev, dev->dev, DISK_NAME_LEN);
        ret = nvm_get_device_info(&info); 
        if(ret){
            printf("nvm_get_device_info error.\n");
        }        

        // Calculated values
        pg_size = info.prop.sec_size * info.prop.sec_per_page;
        pln_pg_size = pg_size * info.prop.nr_planes;

        printf("\n  Device: %s. Managed by %s (%u, %u, %u)\n", dev->dev, 
                dev->bmname, dev->bmversion[0], dev->bmversion[1],
                dev->bmversion[2]);
        printf("   Sector size: %d bytes\n",info.prop.sec_size);
        printf("   Sectors per page: %d\n",info.prop.sec_per_page);
        printf("   Page size: %d bytes\n",pg_size);
        printf("   Plane Page Size: %d bytes\n",pln_pg_size);
        printf("   Nr of planes: %d\n",info.prop.nr_planes);
        printf("   Nr of LUNs: %d\n",info.prop.nr_luns);
        printf("   Nr of Channels: %d\n",info.prop.nr_channels);
        printf("   Plane mode: %d\n",info.prop.plane_mode);
        printf("   max_sec_io: %d\n",info.prop.max_sec_io);
        printf("   oob_size: %d\n",info.prop.oob_size); 
    }
    printf("\n");
}

static void lnvm_create_tgt(struct arguments *args)
{
    struct nvm_ioctl_tgt_create c;
    int ret;

    sprintf(c.target.dev,"%s", args->new_dev);
    sprintf(c.target.tgttype,"%s", args->new_tgt);
    sprintf(c.target.tgtname,"%s", args->new_name);
    c.flags = 0;
    c.conf.type = 0;
    c.conf.s.lun_begin = args->lun_begin;
    c.conf.s.lun_end = args->lun_end;

    printf("\n### LNVM CREATE TARGET ###\n");

    ret = nvm_create_target(&c);

    if(ret) {
        printf(" nvm_create_target ERROR. Do you have root privilegies? "
                                            "If yes, look to 'dmesg'.\n");
        return;
    }

    printf(" LNVM Target created succesfully. Device: %s, target: %s, file: "
                    "/dev/%s\n",args->new_dev,args->new_tgt,args->new_name);
    printf("\n");
}

static void lnvm_remove_tgt(struct arguments *args)
{
    struct nvm_ioctl_tgt_remove c;
    int ret;

    sprintf(c.tgtname, "%s", args->rm_name);
    c.flags = 0;

    printf("\n### LNVM REMOVE TARGET ###\n");

    ret = nvm_remove_target(&c);

    if(ret){
        printf("nvm_remove_target error. Do you have root privilegies? "
                                                "If yes, look to 'dmesg'.\n");
        return;
    }

    printf(" LNVM Target removed succesfully. file: /dev/%s\n",args->rm_name);
    printf("\n");
}

static void lnvm_show_tgt_info(struct arguments *args)
{
    struct nvm_ioctl_tgt_info tgt;
    int ret;

    sprintf(tgt.target.tgtname, "%s", args->tgt_name);
    
    printf("\n### LNVM TARGET INFO ###\n");

    ret = nvm_get_target_info(&tgt);

    if (ret) {
        printf("nvm_get_target_info error. Do you have root privilegies? "
                                                "If yes, look 'dmesg'.\n");
        return;
    }
    
    printf("\n Target File: /dev/%s\n",tgt.target.tgtname);
    printf(" Target Type: %s (%u, %u, %u)\n", tgt.target.tgttype, 
                            tgt.version[0],tgt.version[1],tgt.version[2]);
    printf(" Device: %s\n",tgt.target.dev);
    printf("\n");
    
}

static void lnvm_put_blk(struct arguments *args)
{
    NVM_VBLOCK *vblk;
    int tgt_fd;
    int ret;

    vblk = &args->putblk_vblk;

    printf("\n### LNVM PUT BLOCK ###\n");

    tgt_fd = nvm_target_open(args->putblk_tgt, 0x0);
    if (tgt_fd < 0) {
        printf("nvm_target_open error. Failed to open LightNVM target %s.\n",
                                                            args->getblk_tgt);
        return;
    }


    ret = nvm_put_block(tgt_fd, vblk);
    if (ret) {
        printf("nvm_put_block error. Could not put block %llu to LUN %u.\n",
                                                    vblk->id, vblk->vlun_id);
        return;
    }

    nvm_target_close(tgt_fd);

    printf("\n Block %llu from LUN %u has been succesfully freed.\n",
                                                    vblk->id, vblk->vlun_id);
    printf("\n");
}

static void lnvm_get_blk(struct arguments *args)
{
    NVM_VBLOCK *vblk;
    int tgt_fd;
    int ret;

    vblk = &args->getblk_vblk;
    
    printf("\n### LNVM GET BLOCK ###\n");

    tgt_fd = nvm_target_open(args->getblk_tgt, 0x0);
    if (tgt_fd < 0) {
        printf("nvm_target_open error. Failed to open LightNVM target %s.\n",
                                                            args->getblk_tgt);
        return;
    }

    ret = nvm_get_block(tgt_fd, vblk->vlun_id, vblk);
    if (ret) {
        printf("nvm_get_block error. 'dmesg' for further info.\n");
        return;
    }

    nvm_target_close(tgt_fd);

    printf("\n A block has been succesfully allocated.\n");
    printf(" LUN: %d\n", vblk->vlun_id);
    printf(" Block ID: %llu\n", vblk->id);
    printf(" Block initial addr (bppa): %#018llx\n", vblk->bppa);
    printf(" Nr of ppas (pages): %d\n", vblk->nppas);
    printf("\n");
}

static void write_prepare(struct nvm_io_info *io, struct nvm_dev_info *info,
                                                            uint8_t verbose)
{
    char *current;
    unsigned int written;
    int total_bytes;
    int pg_lines;
    int i, pg;
    char aux[9];
    char input_char;

    current = io->buf_data;
    written = 0;
    total_bytes = info->pln_pg_size * io->nr_pages;
    pg_lines = info->pln_pg_size / 64;
    
    if(verbose)
        printf("\n Total to be written: %d bytes\n Nr of pages: %d\n "
         "Page size: %u bytes\n", total_bytes, io->nr_pages, info->pln_pg_size);
    
    for(pg = io->start_pg; pg < io->start_pg + io->nr_pages; pg++){
        srand(time(NULL)+pg);
        input_char = (rand() % 93) + 33;
        for(i = 0; i < pg_lines-1; i++) {
            switch (i) {
                case 0:
                    memset(current, '#', 63);
                    memset(current+63, '\n', 1);
                    current += 64;
                    written += 64;                
                    break;
                case 1:
                case 3:
                case 5:
                    memset(current, '|', 1);
                    memset(current+1, ' ', 61);
                    memset(current+62, '|', 1); 
                    memset(current+63, '\n', 1);
                    current += 64;
                    written += 64; 
                    break;
                case 2:
                    memset(current, '|', 1);
                    memset(current+1, ' ', 23);
                    sprintf(current+24, "BLOCK ");
                    sprintf(aux, "%d", io->blk_id);
                    memcpy(current+30, aux, strlen(aux));
                    memset(current+30+strlen(aux), ' ', 23+9-strlen(aux));
                    memset(current+62, '|', 1);
                    memset(current+63, '\n', 1);
                    current += 64;
                    written += 64;
                    break;
                case 4:
                    memset(current, '|', 1);
                    memset(current+1, ' ', 23);
                    sprintf(current+24, "PAGE ");
                    sprintf(aux, "%d", pg);
                    memcpy(current+29, aux, strlen(aux));
                    memset(current+29+strlen(aux), ' ', 28+5-strlen(aux));
                    memset(current+62, '|', 1);
                    memset(current+63, '\n', 1);
                    current += 64;
                    written += 64;
                    break;
                default:
                    memset(current, '|', 1);
                    memset(current+1, (char) input_char, 61);
                    memset(current+62, '|', 1); 
                    memset(current+63, '\n', 1);
                    current += 64;
                    written += 64; 
                    break;
            }
        }
        memset(current, '#', 63);
        memset(current+63, '\n', 1);
        current += 64;
        written += 64; 
    }
}

static int io_prepare(struct nvm_io_info *io, struct nvm_dev_info *info)
{
    int ret;

    io->tgt_fd = nvm_target_open(io->tgt_name, 0x0);
    if (io->tgt_fd < 0) {
        printf("nvm_target_open error. Failed to open LightNVM target %s.\n",
                                                                io->tgt_name);
        return -1;
    } 

    ret = posix_memalign((void **)&io->buf_data, info->sec_size, 
                                          info->pln_pg_size * io->nr_pages + 1);
    if (ret) {
        printf("Could not allocate write/read aligned memory (%d,%d)\n",
                    info->sec_size, info->pln_pg_size);
        return -1;
    }
    
    io->bytes_trans = 0;
    io->current_ppa = io->blk_id * info->pg_per_blk + io->start_pg;  
    io->left_pages = io->nr_pages;

    return 0;
}

static int lnvm_io (struct nvm_io_info *io, struct nvm_dev_info *info, 
                                uint8_t direction, struct arguments *args)
{   
    int ret;

    io->blk_id = args->io_blkid;
    io->tgt_name = args->io_tgt;

    ret = get_dev_info(io->tgt_name, info);
    if (ret) {
        printf("nvm_dev_info error. Failed to get device info.\n");
        goto clean;
    }
 
    io->start_pg = (args->io_flag & IOARGS) ? args->io_pgstart : 0;
    io->nr_pages = (args->io_flag & IOARGP) ?
                   args->io_nrpages : 
                   (args->io_flag & IOARGS) ? 
                            1 : info->pg_per_blk;

    if ( io->nr_pages + io->start_pg > info->pg_per_blk )
    {
        printf(" IO out of bounds (last page in the block %d: %d, erroneous "
                "page: %d)\n",io->blk_id, info->pg_per_blk-1, io->nr_pages-1
                                                            + io->start_pg);
        ret = 1;
        goto clean;
    }

    ret = io_prepare(io, info);
    if (ret)
        goto clean;
  
    if(direction == WRITE)
        write_prepare(io, info, args->io_flag & IOARGV);

    while(io->left_pages > 0){
        //sleep(1);

        io->buf_offset = io->buf_data + ( ( io->nr_pages - io->left_pages ) 
                                                    * info->pln_pg_size );

        ret = (direction)?pwrite(io->tgt_fd, io->buf_offset, info->pln_pg_size,
                                io->current_ppa * info->sec_size):
                          pread(io->tgt_fd, io->buf_offset, info->pln_pg_size,
                                io->current_ppa * info->sec_size);
        //printf("  buf: %#018llx, current_ppa: %lu, offset: %lu\n", 
        //    (long long unsigned int) io->buf_offset, io->current_ppa, 
        //                                    io->current_ppa * info->sec_size);
        if (ret != info->pln_pg_size) {
            printf("  Could not perform IO on page %d.\n", 
                                io->nr_pages - io->left_pages + io->start_pg);
            ret = 1;
            goto clean;
        }
        io->bytes_trans += ret;
    
        //printf("  Page %d succesfull.\n",info->pg_per_blk - io->left_pages);

        io->current_ppa += info->pg_sec_ratio;
        io->left_pages--; 
    }
    ret = 0;

clean:
    nvm_target_close(io->tgt_fd);    
    return ret;
}

static void lnvm_write(struct arguments *args)
{
    int ret;
    static struct nvm_io_info io;
    static struct nvm_dev_info info;
    
    if (args->io_flag & IOARGV) {
        printf("\n ### LNVM BLOCK WRITE ###\n");
        printf("\n WRITING TO DEVICE...\n");
    }
    
    ret = lnvm_io(&io, &info, WRITE, args);
    if (ret)
        goto clean;
    
    if (args->io_flag & IOARGV) {
        printf(" Write of %d pages (%d:%d) in block %d performed succesfully.\n",
            io.nr_pages, io.start_pg, io.start_pg + io.nr_pages-1, io.blk_id);
        printf(" Total bytes written: %u bytes\n", io.bytes_trans);
        printf("\n");
    }

clean:
    free(io.buf_data);
}

void static lnvm_read(struct arguments *args) 
{     
    int ret;
    static struct nvm_io_info io;
    static struct nvm_dev_info info;
    
    if (args->io_flag & IOARGV) {
        printf("\n ### LNVM BLOCK READ ###\n");
        printf("\n READING FROM DEVICE...\n");
    }
    
    ret = lnvm_io(&io, &info, READ, args);
    if (ret)
        goto clean;

    if (args->io_flag & IOARGV) {
        printf(" Read of %d pages (%d:%d) in block %d performed succesfully.\n",
            io.nr_pages, io.start_pg, io.start_pg + io.nr_pages-1, io.blk_id);
        printf(" Total bytes read: %u bytes\n", io.bytes_trans);
        memset(io.buf_data + info.pln_pg_size * io.nr_pages,'\0',1);
        printf("%s",io.buf_data);
        printf("\n");
    }

clean:
    free(io.buf_data);
}

char doc_global[] = "\n*** LNVM MANAGER ***\n"
      " \nlnvm-manager is a tool to manage LightNVM-enabled devices\n"
      " such as OpenChannel SSDs.\n\n"
      "  Available commands:\n"
      "   info            Show available lightNVM target types\n"
      "   dev             Show registered lightNVM devices\n"
      "   new             Init a target on top of a device\n"
      "   rm              Remove a target from a device\n"
      "   tgt             Show info about an online target "
                                            "(created by 'new' command)\n"
      "   getblock        Get a block from a specific LUN (mark as in-use)\n"
      "   putblock        Free a block (mark as free, it can be erased)\n"
      "   write           Write data to a block\n"
      "   read            Read data from a block\n";

struct argp argp = {NULL, parse_opt, "lnvm [<cmd> [cmd-options]]",
                                                            doc_global};
int main(int argc, char **argv)
{
    struct arguments args = { 0 };

    args.lun_begin=0;
    args.lun_end=0;

    argp_parse(&argp, argc, argv, ARGP_IN_ORDER, NULL, &args);

    switch (args.cmdtype)
    {
        case LNVM_INFO:
            lnvm_show_info(&args);
            break;
        case LNVM_DEV:
            lnvm_show_devices(&args);
            break;
        case LNVM_NEW:
            lnvm_create_tgt(&args);
            break;
        case LNVM_RM:
            lnvm_remove_tgt(&args);
            break;
        case LNVM_TGT:
            lnvm_show_tgt_info(&args);
            break;
        case LNVM_GETBLK:
            lnvm_get_blk(&args);
            break;
        case LNVM_PUTBLK:
            lnvm_put_blk(&args);
            break;
        case LNVM_WRITE:
            lnvm_write(&args);
            break;
        case LNVM_READ:
            lnvm_read(&args);
            break;
        default:
            printf("Invalid command.\n");            
    }

    return 0;
}
