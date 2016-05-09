#ifndef LNVM_H
#define LNVM_H

#include <linux/types.h>
#include <liblightnvm.h>

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
    int nr_pages;
    int start_pg;
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

enum ioargs_flags {
    IOARGB = 1,
    IOARGN = 2,
    IOARGS = 4,
    IOARGP = 8,
    IOARGV = 16
};

struct arguments
{
    /* GLOBAL */
    int         cmdtype;
    int         arg_num;   
    /* CMD NEW */
    char        *new_tgt;
    char        *new_dev;
    char        *new_name;
    int         lun_begin;
    int         lun_end;
    /* CMD RM */
    char        *rm_name;
    /* CMD TGT */
    char        *tgt_name;
    /* CMD GETBLK */
    int         getblk_argn;
    NVM_VBLOCK  getblk_vblk;
    char        getblk_tgt[DISK_NAME_LEN];
    /* CMD PUTBLK */
    int         putblk_argn;
    NVM_VBLOCK  putblk_vblk;
    char        putblk_tgt[DISK_NAME_LEN];
    /* CMD IO WRITE/READ */
    char        io_tgt[DISK_NAME_LEN];
    uint32_t    io_blkid;
    uint32_t    io_pgstart;
    uint32_t    io_nrpages; 
    uint8_t     io_flag;
};

error_t parse_opt (int, char *, struct argp_state *);

#endif /* LNVM_H */
