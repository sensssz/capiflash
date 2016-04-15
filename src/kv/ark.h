/* IBM_PROLOG_BEGIN_TAG                                                   */
/* This is an automatically generated prolog.                             */
/*                                                                        */
/* $Source: src/kv/ark.h $                                                */
/*                                                                        */
/* IBM Data Engine for NoSQL - Power Systems Edition User Library Project */
/*                                                                        */
/* Contributors Listed Below - COPYRIGHT 2014,2015                        */
/* [+] International Business Machines Corp.                              */
/*                                                                        */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License");        */
/* you may not use this file except in compliance with the License.       */
/* You may obtain a copy of the License at                                */
/*                                                                        */
/*     http://www.apache.org/licenses/LICENSE-2.0                         */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/*                                                                        */
/* IBM_PROLOG_END_TAG                                                     */
#ifndef __ARK_H__
#define __ARK_H__

#include <pthread.h>

#include "hash.h"
#include "bl.h"
#include "ea.h"
#include "bt.h"
#include "tag.h"
#include "queue.h"
#include "sq.h"
#include "lru2l.h"

#define PT_OFF  0
#define PT_IDLE 1
#define PT_RUN  2
#define PT_EXIT 3

#define DIVCEIL(_x, _y)  (((_x) / (_y)) + (((_x) % (_y)) ? 0 : 1))

// Stats to collect on number of K/V ops
// and IOs
typedef struct ark_stats {
  volatile uint64_t ops_cnt;
  volatile uint64_t io_cnt;
  volatile uint64_t kv_cnt;
  volatile uint64_t blk_cnt;
  volatile uint64_t byte_cnt;
} ark_stats_t;

#define ARK_P_VERSION_1       1
#define ARK_P_VERSION_2       2

#define ARK_PERSIST_CONFIG    1
#define ARK_PERSIST_END       2
#define ARK_PERSIST_HT        3
#define ARK_PERSIST_HT_BV     4
#define ARK_PERSIST_HT_IV     5
#define ARK_PERSIST_BL        6
#define ARK_PERSIST_BL_IV     7

typedef struct p_cntr
{
#define ARK_P_MAGIC           "ARKPERST"
  char     p_cntr_magic[8];
  uint64_t p_cntr_version;
  uint32_t p_cntr_type;
  uint64_t p_cntr_size;
  uint64_t p_cntr_cfg_offset;
  uint64_t p_cntr_cfg_size;
  uint64_t p_cntr_ht_offset;
  uint64_t p_cntr_ht_size;
  uint64_t p_cntr_bl_offset;
  uint64_t p_cntr_bl_size;
  uint64_t p_cntr_bliv_offset;
  uint64_t p_cntr_bliv_size;
  char     p_cntr_data[];
} p_cntr_t;

typedef struct p_ark
{
  uint64_t flags;
  uint64_t pblocks;
  uint64_t size;
  uint64_t bsize;
  uint64_t bcount;
  uint64_t blkbits;
  uint64_t grow;
  uint64_t hcount;
  uint64_t vlimit;
  uint64_t blkused;
  ark_stats_t  pstats;
  int      nasyncs;
  int      basyncs;
  int      nthrds;
  int      ntasks;
} P_ARK_t;

#define ARK_VERBOSE_SIZE_DEF  1048576
#define ARK_VERBOSE_BSIZE_DEF    4096
#define ARK_VERBOSE_HASH_DEF  1048576
#define ARK_VERBOSE_VLIMIT_DEF    256
#define ARK_VERBOSE_BLKBITS_DEF    34
#define ARK_VERBOSE_GROW_DEF     1024
#define ARK_VERBOSE_NTHRDS_DEF      1

#define ARK_MAX_ASYNC_OPS         128
#define ARK_MAX_TASK_OPS           48

#define ARK_NUM_HOT_ITEM          100
#define ARK_NUM_POSSIBLE_ITEM      10

typedef struct _ark {
  uint64_t flags;
  uint32_t persload;
  uint64_t pers_cs_bytes;    // amt required for only the Control Structures
  uint64_t pers_max_blocks;  // amt required to persist a full ark
  char    *persdata;
  uint64_t size;
  uint64_t bsize;
  uint64_t bcount;
  uint64_t blkbits;
  uint64_t grow;
  uint64_t hcount;
  uint64_t vlimit;
  uint64_t blkused;
  uint64_t nasyncs;
  uint64_t ntasks;

  ark_stats_t      pers_stats;

  uint32_t holds;

  int nthrds;
  int basyncs;
  int npart;
  int nactive;
  int ark_exit;
  int pcmd;
  int rthread;
  int astart;

  pthread_mutex_t mainmutex;

//  hit_list_t    *hit_list;
  lru2l_t   *lru;

  hash_t *ht; // hashtable
  BL *bl; // block lists
  struct _ea      *ea; // in memory store space
  struct _rcb     *rcbs;
  struct _tcb     *tcbs;
  struct _iocb    *iocbs;
  struct _scb     *poolthreads;
  struct _pt      *pts;
  struct _tags    *rtags;
  struct _tags    *ttags;

} _ARK;

#define _ARC _ARK

typedef struct _scb {
  pthread_t        pooltid;
  queue_t         *rqueue;
  queue_t         *tqueue;
  queue_t         *ioqueue;
  int32_t          poolstate;
  int32_t          rlast;
  int32_t          dogc;
  uint32_t         holds;
  ark_stats_t      poolstats;
  pthread_mutex_t  poolmutex;
  pthread_cond_t   poolcond;
} scb_t;

// pool thread gets its id and the database struct
typedef struct _pt {
  int id;
  _ARK *ark;
} PT;


#define K_NULL 0
#define K_GET  1
#define K_SET  2
#define K_DEL  3
#define K_EXI  4
#define K_RAND 5
#define K_FIRST 6
#define K_NEXT 7
#define K_CALLBACK 8

#define A_NULL     0
#define A_INIT     1
#define A_COMPLETE 2
#define A_FINAL    3


// operation 
typedef struct _rcb {
  _ARK       *ark;
  uint64_t    klen;
  void       *key;
  uint64_t    vlen;
  void       *val;
  uint64_t    voff;

  uint64_t    pos;
  uint64_t    hash;

  int64_t     res;
  uint64_t    dt;
  int32_t     rc;

  uint64_t    rnum;
  int32_t     rtag;
  int32_t     ttag;
  int32_t     hold;
  int32_t     sthrd;
  int32_t     cmd;
  int32_t     stat;

  void        (*cb)(int errcode, uint64_t dt,int64_t res);
  pthread_cond_t  acond;
  pthread_mutex_t alock;
} rcb_t;

typedef struct _ari {
  _ARK     *ark;
  int64_t  hpos;
  int64_t  key;
  int32_t  ithread;
  uint64_t btsize;
  BT      *bt;
  BT      *bt_orig;
  uint8_t *pos;
} _ARI;

#define ARK_CMD_INIT         1
#define ARK_CMD_DONE         2
#define ARK_IO_SCHEDULE      3
#define ARK_IO_HARVEST       4
#define ARK_IO_DONE          5
#define ARK_SET_START        6
#define ARK_SET_PROCESS_INB  7
#define ARK_SET_WRITE        8
#define ARK_SET_FINISH       9
#define ARK_GET_START        10
#define ARK_GET_PROCESS      11
#define ARK_GET_FINISH       12
#define ARK_DEL_START        13
#define ARK_DEL_PROCESS      14
#define ARK_DEL_FINISH       15
#define ARK_EXIST_START      16
#define ARK_EXIST_FINISH     17
#define ARK_RAND_START       18
#define ARK_FIRST_START      19
#define ARK_NEXT_START       20

// operation 
typedef struct _tcb {
  int32_t         rtag;
  int32_t         ttag;
  int32_t         state;
  int32_t         sthrd;

  uint64_t        inblen;
  uint64_t        oublen;
  BTP             inb; // input bucket space - aligned
  BTP             inb_orig; // input bucket space
  BTP             oub; // output bucket space - aligned
  BTP             oub_orig; // output bucket space
  uint64_t        vbsize;
  uint8_t        *vb; // value buffer space - aligned
  uint8_t        *vb_orig; // value buffer space
  uint8_t        *vval; // value buffer space
  uint64_t        vblkcnt;
  int64_t         vblk;
  uint64_t        hpos;
  uint64_t        hblk;
  int64_t         nblk;
  uint64_t        blen;
  uint64_t        old_btsize;
  int64_t         vvlen;
  int32_t         new_key;
} tcb_t;

typedef struct _iocb
{
  struct _ea     *ea; // in memory store space
  int32_t         op;
  void           *addr;
  ark_io_list_t  *blist;
  uint64_t        nblks;
  uint64_t        start;
  uint64_t        new_start;
  uint64_t        cnt;
  int32_t         tag;
  uint32_t        issT;
  uint32_t        cmpT;
  uint32_t        hmissN;
  uint32_t        io_error;
  uint32_t        io_done;
} iocb_t;

int64_t ark_take_pool(_ARK *_arkp, ark_stats_t *stats, uint64_t n);
void ark_drop_pool(_ARK *_arkp, ark_stats_t *stats, uint64_t blk);

int ark_set_async_tag(_ARK *ark,  uint64_t klen, void *key, uint64_t vlen, void *val);
int ark_get_async_tag(_ARK *ark, uint64_t klen,void *key,uint64_t vbuflen,void *vbuf,uint64_t voff);
int ark_del_async_tag(_ARK *ark,  uint64_t klen, void *key);
int ark_exists_async_tag(_ARK *ark,  uint64_t klen, void *key);
int ark_rand_async_tag(_ARK *ark, uint64_t klen, void *key, int32_t ptid);
int ark_first_async_tag(_ARK *ark, uint64_t klen, void *key, _ARI *_arip, int32_t ptid);
int ark_next_async_tag(_ARK *ark, uint64_t klen, void *key, _ARI *_arip, int32_t ptid);

int ark_anyreturn(_ARK *ark, int *tag, int64_t *res);

void ea_async_io_init(_ARK *_arkp, int op, void *addr, ark_io_list_t *blist,
                      int64_t nblks, int start, int32_t tag, int32_t io_done);

#endif
