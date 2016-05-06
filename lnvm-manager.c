/*  This software is based on 'lnvm' by Matias Bjørling
    Now, instead of direct ioctl calls, we manage lightNVM
    devices through 'liblightnvm'.
*/        

#include <stdio.h>
#include <stdlib.h>
#include <argp.h>
#include <string.h>
#include <linux/lightnvm.h>
#include <liblightnvm.h>

enum cmdtypes {
    LNVM_TGT = 1,
    LNVM_DEV,
    LNVM_NEW,
    LNVM_RM
};

struct arguments
{
    int cmdtype;
    int arg_num;   
 
    char *new_tgt;
    char *new_dev;
    char *new_name;
    int lun_begin;
    int lun_end;

    char *rm_name;
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
        " Init target (tgt0) with (nvme0n1) device using rrpc type on luns [0,1,2,3].\n"
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

static char doc_global[] = "\n*** LNVM MANAGER ***\n"
                    " \nlnvm-manager is a tool to manage LightNVM-enabled devices\n"
                    " such as OpenChannel SSDs.\n\n"
                    "  Available commands:\n"
                    "   tgt     Show registered lightNVM targets\n"
                    "   dev     Show registered lightNVM devices\n"
                    "   new     Init a target on top of a device\n"
                    "   rm      Remove a target from a device\n";

static void cmd_prepare(struct argp_state *state, struct arguments *args, char *cmd, struct argp *argp_cmd)
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
            if (strcmp(arg, "tgt") == 0)
                args->cmdtype = LNVM_TGT;
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
            break;
        default:
            return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

static struct argp argp = {NULL, parse_opt, "lnvm [<cmd> [cmd-options]]", doc_global};

void show_targets(struct arguments *args)
{
    struct nvm_ioctl_info c;
    int ret, i;

    memset(&c, 0, sizeof(struct nvm_ioctl_info));
    
    ret = nvm_get_info(&c);
    if(ret){
        printf("nvm_get_info error. Do you have root privilegies? If yes, look to 'dmesg'. %d\n",ret);
        return;
    }                     

    printf("\n### LNVM TARGET TYPES ###\n");
    printf(" LightNVM version (%u, %u, %u). %u target type(s) registered.\n",c.version[0],c.version[1],c.version[2],c.tgtsize);

    for(i=0; i<c.tgtsize; i++){
        struct nvm_ioctl_tgt_info *tgt = &c.tgts[i];
        printf("  Type: %s (%u, %u, %u)\n", tgt->target.tgtname, tgt->version[0],tgt->version[1],tgt->version[2]);
    }
    printf("\n");
}

void show_devices(struct arguments *args)
{
    struct nvm_ioctl_get_devices devs;
    int ret, i;

    ret = nvm_get_devices(&devs);
    if (ret) {
        printf("nvm_get_devices error. Do you have root privilegies? If yes, look to 'dmesg'. %d\n",ret);        
        return;
    }

    printf("\n### LNVM DEVICES ###\n");
    printf(" nr_devices: %u\n", devs.nr_devices);

    for (i = 0; i < devs.nr_devices && i < 31; i++) {
        struct nvm_ioctl_dev_info *dev = &devs.info[i];
        struct nvm_ioctl_dev_info info;
        
        strncpy(info.dev, dev->dev, DISK_NAME_LEN);
        ret = nvm_get_device_info(&info); 
        if(ret){
            printf("nvm_get_device_info error.\n");
        }        

        printf("\n  Device: %s. Managed by %s (%u, %u, %u)\n", dev->dev, dev->bmname,
                dev->bmversion[0], dev->bmversion[1],
                dev->bmversion[2]);
        printf("   Sector size: %d\n",info.prop.sec_size);
        printf("   Sectors per page: %d\n",info.prop.sec_per_page);
        printf("   Nr of planes: %d\n",info.prop.nr_planes);
        printf("   Nr of LUNs: %d\n",info.prop.nr_luns);
        printf("   Nr of Channels: %d\n",info.prop.nr_channels);
        printf("   Plane mode: %d\n",info.prop.plane_mode);
        printf("   max_sec_io: %d\n",info.prop.max_sec_io);
        printf("   oob_size: %d\n",info.prop.oob_size); 
    }
    printf("\n");
}

void create_tgt(struct arguments *args)
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
        printf(" nvm_create_target ERROR. Do you have root privilegies? If yes, look to 'dmesg'.\n");
        return;
    }

    printf(" LNVM Target created succesfully. Device: %s, target: %s, file: /dev/%s\n",args->new_dev,args->new_tgt,args->new_name);
    printf("\n");
}

void remove_tgt(struct arguments *args)
{
    struct nvm_ioctl_tgt_remove c;
    int ret;

    sprintf(c.tgtname, "%s", args->rm_name);
    c.flags = 0;

    printf("\n### LNVM REMOVE TARGET ###\n");

    ret = nvm_remove_target(&c);

    if(ret){
        printf("nvm_remove_target error. Do you have root privilegies? If yes, look to 'dmesg'.\n");
        return;
    }

    printf(" LNVM Target removed succesfully. file: /dev/%s\n",args->rm_name);
    printf("\n");
}

int main(int argc, char **argv)
{
    struct arguments args = { 0 };

    args.lun_begin=0;
    args.lun_end=0;

    argp_parse(&argp, argc, argv, ARGP_IN_ORDER, NULL, &args);

    switch (args.cmdtype)
    {
        case LNVM_TGT:
            show_targets(&args);
            break;
        case LNVM_DEV:
            show_devices(&args);
            break;
        case LNVM_NEW:
            create_tgt(&args);
            break;
        case LNVM_RM:
            remove_tgt(&args);
            break;
        default:
            printf("Invalid command.\n");            
    }

    return 0;
}