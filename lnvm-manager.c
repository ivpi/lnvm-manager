/*  This software is based on 'lnvm' by Matias Bjørling
    Now, instead of direct ioctl calls, we manage lightNVM
    devices through 'liblightnvm'.

    The main improvement is the parallelism among LUNs.
    Now, it is possible to pick blocks from different LUNs,
    and read/write in parallel. 
*/        

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <argp.h>
#include <string.h>
#include <linux/lightnvm.h>
#include <liblightnvm.h>
#include <linux/types.h>

struct nvm_dev_info {
    uint32_t sec_size;
    uint32_t page_size;
    uint32_t pln_pg_size;
    uint32_t max_sec_io;
    uint16_t pg_per_blk;
    uint16_t pg_sec_ratio;
};

struct nvm_io_info {
    int tgt_fd;
    char * tgt_name;
    uint32_t blk_id;
    size_t current_ppa;
    char * buf_offset;
    char * buf_data;
    int left_pages;
    uint32_t bytes_trans;
};

enum io_dir {
    READ = 0,
    WRITE
};

enum cmdtypes {
    LNVM_INFO = 1,
    LNVM_DEV,
    LNVM_NEW,
    LNVM_RM,
    LNVM_TGT,
    LNVM_GETBLK,
    LNVM_PUTBLK,
    LNVM_WRITE,
    LNVM_READ
};

struct arguments
{
    /* GLOBAL */
    int cmdtype;
    int arg_num;   
    /* CMD NEW */
    char *new_tgt;
    char *new_dev;
    char *new_name;
    int lun_begin;
    int lun_end;
    /* CMD RM */
    char *rm_name;
    /* CMD TGT */
    char *tgt_name;
    /* CMD GETBLK */
    int getblk_argn;
    NVM_VBLOCK getblk_vblk;
    char getblk_tgt[DISK_NAME_LEN];
    /* CMD PUTBLK */
    int putblk_argn;
    NVM_VBLOCK putblk_vblk;
    char putblk_tgt[DISK_NAME_LEN];
    /* CMD WRITE */
    char write_tgt[DISK_NAME_LEN];
    uint32_t write_blkid;
    /* CMD READ */
    char read_tgt[DISK_NAME_LEN];
    uint32_t read_blkid;
    uint32_t read_pgid;
    int read_argb;
    int read_argn;
};

const char *argp_program_version = "lnvm-manager 1.0";
const char *argp_program_bug_address = "Ivan Picoli <ivpi@itu.dk>";

/* CMD NEW */

static struct argp_option opt_new[] = {
    {"device", 'd', "DEVICE", 0, "LightNVM device e.g. nvme0n1"},
    {"tgtname", 'n', "TGTNAME", 0, "Target name e.g. tgt0"},
    {"tgttype", 't', "TGTTYPE", 0, "Target type e.g. dflash"},
    {"luns", 'l', "LUNS", 0, "LUNs e.g. 0:3"},
    {0}
};

static char doc_new[] =
   "\n\vExamples:\n"
   " Init target (tgt0) with (nvme0n1) device using dflash type on lun 0.\n"
   "  lnvm new -d nvme0n1 -n tgt0 -t dflash\n"
   " Init target (tgt0) with (nvme0n1) device using rrpc type on luns " 
                                                                "[0,1,2,3].\n"
   "  lnvm new -d nvme0n1 -n tgt0 -t rrpc -l 0:3\n";

static error_t parse_opt_new(int key, char *arg, struct argp_state *state)
{
    struct arguments *args = state->input;
    int vars;

    switch (key) {
    case 'd':
        args->new_dev=arg;
        args->arg_num++;
        break;
    case 'n':
        args->new_name=arg;
        args->arg_num++;
        break;
    case 't':
        args->new_tgt=arg;
        args->arg_num++;
        break;
    case 'l':
        if (!arg)
            argp_usage(state);
        vars = sscanf(arg, "%u:%u", &args->lun_begin, &args->lun_end);
        if (vars != 2)
            argp_usage(state);
        args->arg_num++;
        break;
    case ARGP_KEY_ARG:
        if (args->arg_num > 4)
            argp_usage(state);
        break;
    case ARGP_KEY_END:
        if (args->arg_num < 3)
            argp_usage(state);
        break;
    default:
        return ARGP_ERR_UNKNOWN;
    }

    return 0;
}

static struct argp argp_new = { opt_new, parse_opt_new, 0, doc_new};

/* END CMD NEW */

/* CMD RM */

static struct argp_option opt_rm[] = {
    {"tgtname", 'n', "TGTNAME", 0, "Target name e.g. tgt0"},
    {0}
};

static char doc_rm[] =
        "\n\vExamples:\n"
        "  lnvm rm -n tgt0\n"
        "  lnvm rm tgt0\n";

static error_t parse_opt_rm(int key, char *arg, struct argp_state *state)
{
    struct arguments *args = state->input;

    switch (key) {
        case 'n':
            if (!arg || args->rm_name)
                argp_usage(state);
            if (strlen(arg) > DISK_NAME_LEN) {
                printf("Argument too long\n");
                argp_usage(state);
            }
            args->rm_name = arg;
            args->arg_num++;
            break;
        case ARGP_KEY_ARG:
            if (args->arg_num > 1)
                argp_usage(state);
            if (arg) {
                args->rm_name = arg;
                args->arg_num++;
            }
            break;
        case ARGP_KEY_END:
            if (args->arg_num < 1)
                argp_usage(state);
            break;
        default:
            return ARGP_ERR_UNKNOWN;
    }

    return 0;
}

static struct argp argp_rm = { opt_rm, parse_opt_rm, 0, doc_rm};

/* END CMD RM */

/* CMD TGT INFO */

static struct argp_option opt_tgt[] = {
    {"tgtname", 'n', "TGTNAME", 0, "Target name e.g. tgt0"},
    {0}
};

static char doc_tgt[] =
        "\n\vExamples:\n"
        "  lnvm tgt -n tgt0\n"
        "  lnvm tgt tgt0\n";

static error_t parse_opt_tgt(int key, char *arg, struct argp_state *state)
{
    struct arguments *args = state->input;

    switch (key) {
        case 'n':
            if (!arg || args->tgt_name)
                argp_usage(state);
            if (strlen(arg) > DISK_NAME_LEN) {
                printf("Argument too long\n");
                argp_usage(state);
            }
            args->tgt_name = arg;
            args->arg_num++;
            break;
        case ARGP_KEY_ARG:
            if (args->arg_num > 1)
                argp_usage(state);
            if (arg) {
                args->tgt_name = arg;
                args->arg_num++;
            }
            break;
        case ARGP_KEY_END:
            if (args->arg_num < 1)
                argp_usage(state);
            break;
        default:
            return ARGP_ERR_UNKNOWN;
    }

    return 0;
}

static struct argp argp_tgt = { opt_tgt, parse_opt_tgt, 0, doc_tgt};

/* END CMD TGT INFO */

/* CMD GET BLOCK */

static struct argp_option opt_getblk[] = {
    {"lun", 'l', "LUN", 0, "LUN id. <int>"},
    {"target", 'n', "TARGET_NAME", 0, "Target name. e.g. 'mydev'"},
    {0}
};

static char doc_getblk[] =
   "\n\vExamples:\n"
   "  lnvm getblock -l 2 -n mydev\n"
   "  lnvm getblock -n mydev (without 'l' argument to pick a random LUN)\n";

static error_t parse_opt_getblk(int key, char *arg, struct argp_state *state)
{
    struct arguments *args = state->input;

    switch (key) {
        case 'l':
            args->getblk_vblk.vlun_id = atoi(arg);
            if (args->getblk_vblk.vlun_id < 0)
                argp_usage(state);
            args->getblk_vblk.flags |= NVM_PROV_SPEC_LUN;
            args->getblk_vblk.owner_id = 101;
            args->arg_num++;
            break;
        case 'n':
            strcpy(args->getblk_tgt,arg);
            args->arg_num++;
            args->getblk_argn++; 
            break;
        case ARGP_KEY_ARG:
            if (args->arg_num > 2)
                argp_usage(state);
            break;
        case ARGP_KEY_END:
            if (args->arg_num < 1 || !args->getblk_argn)
                argp_usage(state);
            if (args->arg_num == 1){
                /* wait for NVM_PROV_RAND_LUN until the feature has been 
                                                    developed in the kernel */
                /* for now, random LUN we pick LUN 0 */
                //args->getblk_vblk.flags |= NVM_PROV_RAND_LUN;
                args->getblk_vblk.flags |= NVM_PROV_SPEC_LUN;
                args->getblk_vblk.vlun_id = 0;
                args->getblk_vblk.owner_id = 101;
            }
            break;
        default:
            return ARGP_ERR_UNKNOWN;
    }

    return 0;
}

static struct argp argp_getblk = { opt_getblk, parse_opt_getblk, 0,
                                                            doc_getblk};

/* END CMD GET BLOCK */

/* CMD PUT BLOCK */

static struct argp_option opt_putblk[] = {
    {"lunid", 'l', "LUN_ID", 0, "LUN ID. <int>"},
    {"blockid", 'b', "BLOCK_ID", 0, "Block ID. <int>"},
    {"target", 'n', "TARGET_NAME", 0, "Target name. e.g. 'mydev'"},
    {0}
};

static char doc_putblk[] =
        "\nYou must use the same LUN and block you allocated by 'getblock'.\n"
        "\n\vExamples:\n"
        "  lnvm putblock -l 1 -b 1022 -n mydev\n"
        "  lnvm putblock -l 0 -b 5 -n mydev\n";

static error_t parse_opt_putblk(int key, char *arg, struct argp_state *state)
{
    struct arguments *args = state->input;

    switch (key) {
        case 'b':
            args->putblk_vblk.id = atoi(arg);
            if (args->putblk_vblk.id < 0)
                argp_usage(state);
            args->arg_num++;
            break;
        case 'l':
            args->putblk_vblk.vlun_id = atoi(arg);
            if (args->putblk_vblk.vlun_id < 0)
                argp_usage(state);
            args->arg_num++;
            break;
        case 'n':
            strcpy(args->putblk_tgt,arg);
            args->arg_num++;
            args->putblk_argn++; 
            break;
        case ARGP_KEY_ARG:
            if (args->arg_num > 3 || !args->putblk_argn)
                argp_usage(state);
            break;
        case ARGP_KEY_END:
            if (args->arg_num < 3)
                argp_usage(state);
            break;
        default:
            return ARGP_ERR_UNKNOWN;
    }

    return 0;
}

static struct argp argp_putblk = { opt_putblk, parse_opt_putblk, 0,
                                                            doc_putblk};

/* END CMD PUT BLOCK */

/* CMD WRITE */

static struct argp_option opt_write[] = {
    {"blockid", 'b', "BLOCK_ID", 0, "Block ID. <int>"},
    {"target", 'n', "TARGET_NAME", 0, "Target name. e.g. 'mydev'"},    
    {0}
};

static char doc_write[] =
   "\nWe consider 256 pages per block for now (This info should come from kernel).\n"
   "This command will overwrite the whole block, use 'read' to see the data.\n"
   "\n\vExamples:\n"
   "  lnvm write -b 1022 -n mydev\n"
   "  lnvm write -b 5 -n mydev\n";

static error_t parse_opt_write(int key, char *arg, struct argp_state *state)
{
    struct arguments *args = state->input;

    switch (key) {
        case 'b':
            args->write_blkid = atoi(arg);
            if (args->write_blkid < 0)
                argp_usage(state);
            args->arg_num++;
            break;
        case 'n':
            strcpy(args->write_tgt,arg);
            args->arg_num++;
            break;
        case ARGP_KEY_ARG:
            if (args->arg_num > 2)
                argp_usage(state);
            break;
        case ARGP_KEY_END:
            if (args->arg_num < 2)
                argp_usage(state);
            break;
        default:
            return ARGP_ERR_UNKNOWN;
    }

    return 0;
}

static struct argp argp_write = { opt_write, parse_opt_write, 0, doc_write};

/* END CMD WRITE */

/* CMD READ */

static struct argp_option opt_read[] = {
    {"blockid", 'b', "BLOCK_ID", 0, "Block ID. <int>"},
    {"pageid", 'p', "PAGE_ID", 0, "Page ID within the block"}, 
    {"target", 'n', "TARGET_NAME", 0, "Target name. e.g. 'mydev'"},    
    {0}
};

static char doc_read[] =
   "\nWe consider 256 pages per block for now (This info should come from kernel).\n"
   "Use this command to read a full block or an invidual page.\n" 
   "\n\vExamples:\n"
   "  lnvm read -b 50 -n mydev (without 'p' to read the whole block)\n"
   "  lnvm read -b 50 -p 10 -n mydev\n"
   "  lnvm read -b 50 -n mydev > output.file";

static error_t parse_opt_read(int key, char *arg, struct argp_state *state)
{
    struct arguments *args = state->input;

    switch (key) {
        case 'b':
            args->read_blkid = atoi(arg);
            if (args->read_blkid < 0)
                argp_usage(state);
            args->arg_num++;
            args->read_argb++;
            break;
        case 'n':
            strcpy(args->read_tgt,arg);
            args->arg_num++;
            args->read_argn++;
            break;
        case 'p':
            args->read_pgid = atoi(arg);
            if (args->read_pgid < 0)
                argp_usage(state);
            args->arg_num++;
            break;
        case ARGP_KEY_ARG:
            if (args->arg_num > 3)
                argp_usage(state);
            break;
        case ARGP_KEY_END:
            if (args->arg_num < 2 || !args->read_argb || !args->read_argn)
                argp_usage(state);
            break;
        default:
            return ARGP_ERR_UNKNOWN;
    }

    return 0;
}

static struct argp argp_read = { opt_read, parse_opt_read, 0, doc_read};

/* END CMD READ */

static char doc_global[] = "\n*** LNVM MANAGER ***\n"
      " \nlnvm-manager is a tool to manage LightNVM-enabled devices\n"
      " such as OpenChannel SSDs.\n\n"
      "  Available commands:\n"
      "   info            Show available lightNVM target types\n"
      "   dev             Show registered lightNVM devices\n"
      "   new             Init a target on top of a device\n"
      "   rm              Remove a target from a device\n"
      "   tgt             Show info about an online target "
                                            "(created by 'new' command)\n"
      "   getblock        Get a block from a specific LUN\n"
      "   putblock        Free a block\n"
      "   write           Write data to a block (use 'getblock' before)\n"
      "   read            Read data from a block\n";

static void cmd_prepare(struct argp_state *state, struct arguments *args,
                                        char *cmd, struct argp *argp_cmd)
{
    int argc = state->argc - state->next + 1;
    char** argv = &state->argv[state->next - 1];
    char* argv0 = argv[0];

    argv[0] = malloc(strlen(state->name) + strlen(cmd) + 2);
    if(!argv[0])
        argp_failure(state, 1, ENOMEM, 0);

    sprintf(argv[0], "%s %s", state->name, cmd);

    argp_parse(argp_cmd, argc, argv, ARGP_IN_ORDER, &argc, args);

    free(argv[0]);
    argv[0] = argv0;
    state->next += argc - 1;
}

static error_t parse_opt (int key, char *arg, struct argp_state *state)
{
    struct arguments *args = state->input;    

    switch(key)
    {
        case ARGP_KEY_ARG:
            if (strcmp(arg, "info") == 0)
                args->cmdtype = LNVM_INFO;
            else if (strcmp(arg, "dev") == 0)
                args->cmdtype = LNVM_DEV;
            else if (strcmp(arg, "new") == 0){
                args->cmdtype = LNVM_NEW;
                cmd_prepare(state, args, "new", &argp_new);
            }
            else if (strcmp(arg, "rm") == 0){
                args->cmdtype = LNVM_RM;   
                cmd_prepare(state, args, "rm", &argp_rm);
            }
            else if (strcmp(arg, "tgt") == 0){
                args->cmdtype = LNVM_TGT;
                cmd_prepare(state, args, "tgt", &argp_tgt);
            }
            else if (strcmp(arg, "getblock") == 0){
                args->cmdtype = LNVM_GETBLK;
                cmd_prepare(state, args, "getblock", &argp_getblk);
            }
            else if (strcmp(arg, "putblock") == 0){
                args->cmdtype = LNVM_PUTBLK;
                cmd_prepare(state, args, "putblock", &argp_putblk);
            }
            else if (strcmp(arg, "write") == 0){
                args->cmdtype = LNVM_WRITE;
                cmd_prepare(state, args, "write", &argp_write);
            }
            else if (strcmp(arg, "read") == 0){
                args->cmdtype = LNVM_READ;
                cmd_prepare(state, args, "read", &argp_read);
            }
            break;
        default:
            return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

static struct argp argp = {NULL, parse_opt, "lnvm [<cmd> [cmd-options]]",
                                                            doc_global};

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

    /* pg_per_blk should come from the kernel, we wait for this */
    info->pg_per_blk = 256;
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

static void write_prepare(char *buf, uint32_t pg_size, uint32_t n_pages, uint32_t blk_id)
{
    char *current;
    unsigned int written;
    int total_bytes;
    int pg_lines;
    int i, pg;
    char aux[9];

    current = buf;
    written = 0;
    total_bytes = pg_size * n_pages;
    pg_lines = pg_size / 64;
    
    printf("\n Total to be written: %d bytes\n Nr of pages: %d\n Page size: %u bytes\n",
                                    total_bytes, n_pages, pg_size);
    printf("\n PROCESSING INPUT...\n");
    
    for(pg = 0; pg < n_pages; pg++){
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
                    sprintf(aux, "%d", blk_id);
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
                    memset(current+1, '-', 61);
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
    memset(current-1, '\0', 1);

    printf("\n Bytes processed: %u bytes\n",written);
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
                                          info->pln_pg_size * info->pg_per_blk);
    if (ret) {
        printf("Could not allocate write/read aligned memory (%d,%d)\n",
                    info->sec_size, info->pln_pg_size);
        return -1;
    }
    
    io->bytes_trans = 0;
    io->current_ppa = io->blk_id * info->pg_per_blk;  
    io->left_pages = info->pg_per_blk;

    return 0;
}

static int lnvm_io (struct nvm_io_info *io, struct nvm_dev_info *info, uint8_t direction)
{   
    int ret;
   
    ret = get_dev_info(io->tgt_name, info);
    if (ret) {
        printf("nvm_dev_info error. Failed to get device info.\n");
        goto clean;
    }

    ret = io_prepare(io, info);
    if (ret)
        goto clean;
  
    if(direction == WRITE)
        write_prepare(io->buf_data, info->pln_pg_size, info->pg_per_blk, io->blk_id);

    while(io->left_pages > 0){
        //sleep(1);

        io->buf_offset = io->buf_data + ( (info->pg_per_blk-io->left_pages) * info->pln_pg_size );

        ret = (direction)?pwrite(io->tgt_fd, io->buf_offset, info->pln_pg_size,
                                io->current_ppa * info->sec_size):
                          pread(io->tgt_fd, io->buf_offset, info->pln_pg_size,
                                io->current_ppa * info->sec_size);
       // printf("  buf: %#018llx, current_ppa: %lu, offset: %lu\n", 
       //     (long long unsigned int) io->buf_offset, io->current_ppa, io->current_ppa * info->sec_size);
        if (ret != info->pln_pg_size) {
            printf("  Could not perform IO on page %d.\n", info->pg_per_blk-io->left_pages);
            goto clean;
        }
        io->bytes_trans += ret;
    
       // printf("  Page %d succesfull.\n",info->pg_per_blk - io->left_pages);

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
    
    io.blk_id = args->write_blkid;
    io.tgt_name = args->write_tgt;
 
    printf("\n ### LNVM FULL BLOCK WRITE ###\n");
    printf("\n WRITING TO DEVICE...\n");
    
    ret = lnvm_io(&io, &info, WRITE);
    if (ret)
        goto clean;
    
    printf(" Full write of %d pages in block %d performed succesfully.\n", info.pg_per_blk, io.blk_id);
    printf(" Total bytes written: %u bytes\n", io.bytes_trans);
    printf("\n");

clean:
    free(io.buf_data);
}

void static lnvm_read(struct arguments *args) 
{
    /* if args->read_pgid > 0, read individual page */
     
    int ret;
    static struct nvm_io_info io;
    static struct nvm_dev_info info;
    
    io.blk_id = args->read_blkid;
    io.tgt_name = args->read_tgt;
 
    printf("\n ### LNVM FULL BLOCK READ ###\n");
    printf("\n READING FROM DEVICE...\n");
    
    ret = lnvm_io(&io, &info, READ);
    if (ret)
        goto clean;

    printf(" Full read of %d pages in block %d performed succesfully.\n", info.pg_per_blk, io.blk_id);
    printf(" Total bytes read: %u bytes\n", io.bytes_trans);
    printf("%s",io.buf_data);
    printf("\n");

clean:
    free(io.buf_data);
}

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
