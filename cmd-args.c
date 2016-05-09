#include <argp.h>
#include <stdlib.h>
#include <linux/types.h>
#include <string.h>
#include "lnvm-manager.h"

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

struct argp argp_new = { opt_new, parse_opt_new, 0, doc_new};

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

struct argp argp_rm = { opt_rm, parse_opt_rm, 0, doc_rm};

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

struct argp argp_tgt = { opt_tgt, parse_opt_tgt, 0, doc_tgt};

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

struct argp argp_getblk = { opt_getblk, parse_opt_getblk, 0,
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

struct argp argp_putblk = { opt_putblk, parse_opt_putblk, 0,
                                                            doc_putblk};

/* END CMD PUT BLOCK */

/* CMD IO WRITE/READ */

static error_t parse_opt_io(int key, char *arg, struct argp_state *state)
{
    struct arguments *args = state->input;

    switch (key) {
        case 'b':
            args->io_blkid = atoi(arg);
            if (args->io_blkid < 0)
                argp_usage(state);
            args->arg_num++;
            args->io_flag |= IOARGB; 
            break;
        case 'n':
            strcpy(args->io_tgt,arg);
            args->arg_num++;
            args->io_flag |= IOARGN;
            break;
        case 's':
            args->io_pgstart = atoi(arg);
            if (args->io_pgstart < 0)
                argp_usage(state);
            args->arg_num++;
            args->io_flag |= IOARGS;
            break;
        case 'p':
            args->io_nrpages = atoi(arg);
            if (args->io_nrpages < 0)
                argp_usage(state);
            args->arg_num++;
            args->io_flag |= IOARGP;
            break;
        case 'v':
            args->arg_num++;
            args->io_flag |= IOARGV;
            break;
        case ARGP_KEY_ARG:
            if (args->arg_num > 5)
                argp_usage(state);
            break;
        case ARGP_KEY_END:
            if (args->arg_num < 2 || (!args->io_flag & IOARGB) 
                                  || (!args->io_flag & IOARGN))
                argp_usage(state);
            break;
        default:
            return ARGP_ERR_UNKNOWN;
    }

    return 0;
}

static struct argp_option opt_write[] = {
    {"blockid", 'b', "BLOCK_ID", 0, "Block ID. <int>"},
    {"page_start", 's', "PAGE_START", 0, "Page start ID within the block"},
    {"nr_pages", 'p', "NUMBER_OF_PAGES", 0, "Number of pages to read"}, 
    {"target", 'n', "TARGET_NAME", 0, "Target name. e.g. 'mydev'"},
    {"verbose", 'v', 0, 0, "Print info and output to the screen"},    
    {0}
};

static char doc_write[] =
   "\nWe consider 256 pages per block for now (This info should come from "
                                                                "kernel).\n"
   "Use this command to overwrite the whole block, "
   "to write an indidual page or a range of pages.\n"
   "Use 'v' to see information and output during read/write.\n"
   "\n Full block: use only 'b' and 'n' keys\n"
   " Individual page: use only 'b', 'n' and 's' keys or 'p' = 1\n"
   " A range of pages: use all keys ('b','n','s' and 'p')\n"
   "\n\vExamples:\n"
   "  lnvm write -b 1022 -n mydev (full block write)\n"
   "  lnvm write -b 100 -s 10 -n mydev (individual page write)\n"
   "  lnvm write -b 75 -n mydev -s 5 -p 10 (range page write. From page " 
                                                                "5 to 14)\n"
   "  lnvm write -b 1000 -n mydev -p 8 (range page write. From page 0 to 7)\n";

static struct argp_option opt_read[] = {
    {"blockid", 'b', "BLOCK_ID", 0, "Block ID. <int>"},
    {"page_start", 's', "PAGE_START", 0, "Page start ID within the block"},
    {"nr_pages", 'p', "NUMBER_OF_PAGES", 0, "Number of pages to read"}, 
    {"target", 'n', "TARGET_NAME", 0, "Target name. e.g. 'mydev'"},    
    {"verbose", 'v', 0, 0, "Print info and output to the screen"},
    {0}
};

static char doc_read[] =
   "\nWe consider 256 pages per block for now (This info should come from " 
                                                                "kernel).\n"
   "Use this command to read a full block, an invidual or a range of page.\n"
   "Use 'v' to see information and output during read/write.\n"
   "\n Full block: use only 'b' and 'n' keys\n"
   " Individual page: use only 'b', 'n' and 's' keys or 'p' = 1\n"
   " A range of pages: use all keys ('b','n','s' and 'p')\n" 
   "\n\vExamples:\n"
   "  lnvm read -b 50 -n mydev (full block read)\n"
   "  lnvm read -b 50 -s 10 -n mydev (individual page read)\n"
   "  lnvm read -b 50 -n mydev > output.file (full block read with output "
                                                                    "file)\n"
   "  lnvm read -b 50 -n mydev -s 5 -p 10 (range page read. From page " 
                                                                "5 to 14)\n"
   "  lnvm read -b 50 -n mydev -p 8 (range page read. From page 0 to 7)\n";

struct argp argp_write = { opt_write, parse_opt_io, 0, doc_write};
struct argp argp_read = { opt_read, parse_opt_io, 0, doc_read};

/* END CMD IO READ/WRITE */

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

error_t parse_opt (int key, char *arg, struct argp_state *state)
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