/* IBM_PROLOG_BEGIN_TAG                                                   */
/* This is an automatically generated prolog.                             */
/*                                                                        */
/* $Source: src/block/test/blockio.c $                                    */
/*                                                                        */
/* IBM Data Engine for NoSQL - Power Systems Edition User Library Project */
/*                                                                        */
/* Contributors Listed Below - COPYRIGHT 2015                             */
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
/**
 *******************************************************************************
 * \file
 * \brief Block Interface I/O Driver
 * \details
 *   This runs I/O to the capi Block Interface using aread/awrite/aresult. The
 *   expected iops are 300k-400k per capi card.                               \n
 *   Using the queuedepth (-q) option affects iops, as there are less cmds.   \n
 *   Example:                                                                 \n
 *                                                                            \n
 *     blockio -d /dev/sg10                                                   \n
 *     d:/dev/sg10 r:100 q:300 s:4 p:0 n:1 i:o err:0 mbps:1401 iops:358676    \n
 *                                                                            \n
 *     blockio -d /dev/sg10 -s 20 -q 1 -r 70 -p                               \n
 *     d:/dev/sg10 r:70 q:1 s:20 p:1 n:1 i:0 err:0 mbps:26 iops:6905
 *******************************************************************************
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <assert.h>
#include <fcntl.h>
#include <malloc.h>
#include <capiblock.h>
#include <inttypes.h>
//#include <asm/cachectl.h>

#define _8K   8*1024
#define _4K   4*1024
#define NBUFS _8K

#define TIME_INTERVAL _8K
#define SET_NBLKS     TIME_INTERVAL*4

#ifdef _AIX
#define USLEEP 0
//#define USLEEP 200
#else
#define USLEEP 0
#endif

/**
********************************************************************************
** \brief print the usage
** \details
**   -d device     *the device name to run Block IO                           \n
**   -r %rd        *the percentage of reads to issue (0..100)                 \n
**   -q queuedepth *the number of outstanding ops to maintain                 \n
**   -n nblocks    *the number of 4k blocks in each I/O request               \n
**   -s secs       *the number of seconds to run the I/O                      \n
**   -p            *run in physical lun mode                                  \n
**   -i            *run using interrupts, not polling
**   -t            *determine type of benchmark
**
*******************************************************************************/

/**
*******************************************************************************
** type of benchmark
t=0, random write with flushing cache blocks
t=1, random write with out flushing cache blocks
t=2, random read with flushing cache blocks
t=3, random read with out flushign cache blocks

*******************************************************************************/


void usage(void)
{
    printf("Usage:\n");
    printf("   \
[-d device] [-r %%rd] [-q queuedepth] [-n nblocks] [-s secs] [-p] [-i]\n");
    exit(0);
}

/**
********************************************************************************
** \brief print errno and exit
** \details
**   An IO has failed, print the errno and exit
*******************************************************************************/
void io_error(int id, int err)
{
    fprintf(stderr, "io_error: errno:%d\n", err);
    cblk_close(id,0);
    cblk_term(NULL,0);
    exit(err);
}

/**
********************************************************************************
** \brief main
** \details
**   process input parms                                                      \n
**   open device                                                              \n
**   alloc memory                                                             \n
**   loop running IO until secs expire                                        \n
**   print IO stats                                                           \n
**   cleanup
*******************************************************************************/





int main(int argc, char **argv)
{
    struct timeval  start, delta;
    long int        mil        = 1000000;
    float           esecs      = 0;
    uint8_t        **rbuf      = NULL;
    uint8_t        **wbuf      = NULL;
    int            *tags       = NULL;
    char           *dev        = NULL;
    char            FF         = 0xFF;
    char            c          = '\0';
    chunk_ext_arg_t ext        = 0;
    int             flags      = 0;
    int             rc         = 0;
    int             id         = 0;
    char           *_secs      = NULL;
    char           *_type      = NULL;
    char           *_QD        = NULL;
    char           *_RD        = NULL;
    char           *_nblocks   = NULL;
    uint32_t        plun       = 0;
    uint32_t        nsecs      = 4;
    uint32_t        QD         = 500;
    uint32_t        nRD        = 100;
    uint32_t        RD         = 0;
    uint32_t        WR         = 0;
    uint32_t        intrp_thds = 0;
    int             tag        = 0;
    int             rtag       = 0;
    uint32_t        lba        = 0;
    size_t          nblks      = 0;
    uint32_t        nblocks    = 1;
    uint32_t        cnt        = 0;
    uint32_t        pollN      = 0;
    uint64_t        status     = 0;
    uint32_t        TI         = TIME_INTERVAL;
    uint32_t        N          = 0;
    uint32_t        TIME       = 1;
    uint32_t        COMP       = 0;
		uint32_t        type       =0;

    /*--------------------------------------------------------------------------
     * process and verify input parms
     *------------------------------------------------------------------------*/
    while (FF != (c=getopt(argc, argv, "d:r:q:n:s:t:phi")))
    {
        switch (c)
        {
            case 'd': dev        = optarg; break;
            case 'r': _RD        = optarg; break;
            case 'q': _QD        = optarg; break;
            case 'n': _nblocks   = optarg; break;
            case 's': _secs      = optarg; break;
            case 't': _type =      optarg;       break;
            case 'p': plun       = 1;      break;
            case 'i': intrp_thds = 1;      break;
            case 'h':
            case '?': usage();             break;
        }
    }
    if (_secs)      nsecs   = atoi(_secs);
    if (_type)      type   = atoi(_type);
    if (_QD)        QD      = atoi(_QD);
    if (_nblocks)   nblocks = atoi(_nblocks);
    if (_RD)        nRD     = atoi(_RD);

    if (QD > _8K)   QD      = _8K;
    if (nRD > 100)  nRD     = 100;
    if (!plun)      nblocks = 1;
    if (dev == NULL) usage();

    srand48(time(0));
			

    N = QD;
    COMP  = QD < 5 ? 1 : QD/5;

    /*--------------------------------------------------------------------------
     * open device and set lun size
     *------------------------------------------------------------------------*/
    rc = cblk_init(NULL,0);
    if (rc)
    {
        fprintf(stderr,"cblk_init failed with rc = %d and errno = %d\n",
                rc,errno);
        exit(1);
    }
    if (!plun)       flags  = CBLK_OPN_VIRT_LUN;
    if (!intrp_thds) flags |= CBLK_OPN_NO_INTRP_THREADS;
    id = cblk_open(dev, QD, O_RDWR, ext, flags);
    printf("Success in opening device\n");
    if (id == NULL_CHUNK_ID)
    {
        if      (ENOSPC == errno) fprintf(stderr,"cblk_open: ENOSPC\n");
        else if (ENODEV == errno) fprintf(stderr,"cblk_open: ENODEV\n");
        else                      fprintf(stderr,"cblk_open: errno:%d\n",errno);
        cblk_term(NULL,0);
        exit(errno);
    }

    rc = cblk_get_lun_size(id, &nblks, 0);
    if (rc)
    {
        fprintf(stderr, "cblk_get_lun_size failed: errno: %d\n", errno);
        exit(errno);
    }
    if (!plun)
    {
        nblks = nblks > SET_NBLKS ? SET_NBLKS : nblks;
        rc = cblk_set_size(id, nblks, 0);
        if (rc)
        {
            fprintf(stderr, "cblk_set_size failed, errno: %d\n", errno);
            exit(errno);
        }
    }

    /*--------------------------------------------------------------------------
     * alloc data for IO
     *------------------------------------------------------------------------*/
    tags = malloc(QD*sizeof(int));
    rbuf = malloc(QD*sizeof(uint8_t*));
    wbuf = malloc(QD*sizeof(uint8_t*));

    for (tag=0; tag<QD; tag++)
    {
        if ((rc=posix_memalign((void**)&(rbuf[tag]), _4K, _4K*nblocks)))
        {
            fprintf(stderr,"posix_memalign failed, size=%d, rc=%d\n",
                    _4K*nblocks, rc);
            cblk_close(id,0);
            cblk_term(NULL,0);
            exit(0);
        }
        if ((rc=posix_memalign((void**)&(wbuf[tag]), _4K, _4K*nblocks)))
        {
            fprintf(stderr,"posix_memalign failed, size=%d, rc=%d\n",
                    _4K*nblocks, rc);
            cblk_close(id,0);
            cblk_term(NULL,0);
            exit(0);
        }
        memset(wbuf[tag],0x79,_4K*nblocks);
        tags[tag] = tag;
    }

    /*--------------------------------------------------------------------------
     * loop running IO until secs expire
     *------------------------------------------------------------------------*/
    gettimeofday(&start, NULL);

    if (type == 0)
    {
        printf("Testing random write with no hardware cache coherence\n");
        do
        {
            if (!WR) { WR=100-RD;}

            while (TIME && WR && N)
            {
                tag = tags[--N];
                //////////////////////////////////////////////////////////
                ///////////////write buffer flush////////////////////////
                int wr_ii=0;
                int wr_start_addr=0;
                while(wr_ii < _8K*nblocks)
                {
                    __asm__ __volatile ("dcbf %0,%1 \n"
                        :
                        :"b"(wr_start_addr), "r"(wbuf[tag]+wr_ii));

                    __asm__ __volatile ("sync\n"
                        :
                        :);

                    wr_ii += 128;//to be changed

                }

                 ////////////////////////////////////////////
                 ///////////////flush////////////////////////

                rc = cblk_awrite(id, wbuf[tag], lba, nblocks, &tag, NULL,
                             CBLK_ARW_WAIT_CMD_FLAGS | CBLK_ARW_USER_TAG_FLAG);
                if      (0 == rc) {--WR; lba+=2; if (lba >= nblks) lba=cnt%2;}
                else if (EBUSY == errno) {++N; usleep(USLEEP); continue;}
                else                     {io_error(id,errno);}
            }

            /* if the queuedepth is 1, don't immediately pound aresult */
            if (QD==1) usleep(USLEEP);

            /*----------------------------------------------------------------------
             * complete cmds until queue depth is QD-COMP
             *--------------------------------------------------------------------*/
            while (N < COMP)
            {
                rtag=0;
                rc = cblk_aresult(id, &rtag, &status, CBLK_ARESULT_BLOCKING|
                                                      CBLK_ARESULT_NEXT_TAG);
                if      (rc == 0) {++pollN; usleep(USLEEP); continue;}
                else if (rc < 0)  {io_error(id,errno);}
                ++cnt; tags[N++] = rtag;

            }
            /*----------------------------------------------------------------------
             * at an interval which does not impact performance, check if secs
             * have expired, and randomize lba
             *--------------------------------------------------------------------*/
            if (cnt > TI)
            {
                TI += TIME_INTERVAL;
                gettimeofday(&delta, NULL);
                if (delta.tv_sec - start.tv_sec >= nsecs) {TIME=0; COMP = QD;}
                lba = lrand48() % TIME_INTERVAL;
            }
        }
        while (TIME || QD-N);
    }
    else if (type ==1)
    {
        printf("Testing random write with hardware cache coherence\n");
        do
        {
            if (!WR) { WR=100-RD;}

            while (TIME && WR && N)
            {
                tag = tags[--N];

                rc = cblk_awrite(id, wbuf[tag], lba, nblocks, &tag, NULL,
                             CBLK_ARW_WAIT_CMD_FLAGS | CBLK_ARW_USER_TAG_FLAG);
                if      (0 == rc) {--WR; lba+=2; if (lba >= nblks) lba=cnt%2;}
                else if (EBUSY == errno) {++N; usleep(USLEEP); continue;}
                else                     {io_error(id,errno);}
            }

            /* if the queuedepth is 1, don't immediately pound aresult */
            if (QD==1) usleep(USLEEP);

            /*----------------------------------------------------------------------
             * complete cmds until queue depth is QD-COMP
             *--------------------------------------------------------------------*/
            while (N < COMP)
            {
                rtag=0;
                rc = cblk_aresult(id, &rtag, &status, CBLK_ARESULT_BLOCKING|
                                                      CBLK_ARESULT_NEXT_TAG);
                if      (rc == 0) {++pollN; usleep(USLEEP); continue;}
                else if (rc < 0)  {io_error(id,errno);}
                ++cnt; tags[N++] = rtag;

            }
            /*----------------------------------------------------------------------
             * at an interval which does not impact performance, check if secs
             * have expired, and randomize lba
             *--------------------------------------------------------------------*/
            if (cnt > TI)
            {
                TI += TIME_INTERVAL;
                gettimeofday(&delta, NULL);
                if (delta.tv_sec - start.tv_sec >= nsecs) {TIME=0; COMP = QD;}
                lba = lrand48() % TIME_INTERVAL;
            }
        }
        while (TIME || QD-N);
    }
    else if (type==2)
    {
        printf("Testing random read with no hardware cache coherence\n");
        do
        {
            if (!RD) {RD=nRD;}
            /*----------------------------------------------------------------------
             * send up to RD reads, as long as the queuedepth N is not max
             *--------------------------------------------------------------------*/
            while (TIME && RD && N)
            {
                tag = tags[--N];
                //flushing the cache
                //////////////////////////////////////////////////////////
                ///////////////read buffer flush//////////////////////////
                //////////////////////////////////////////////////////////
                int rd_ii=0;
                int rd_start_addr=0;
                while(rd_ii < _4K*nblocks)
                {
                    __asm__ __volatile ("dcbf %0,%1 \n"
                        :
                        :"b"(rd_start_addr), "r"(rbuf[tag]+rd_ii));
                    __asm__ __volatile ("sync\n"
                        :
                        :);


                    rd_ii += 128;//to be changed
                }

                //////////////////////////////////////////////////////////
                /////////////////red buffer flush////////////////////////
                rc = cblk_aread(id, rbuf[tag], lba, nblocks, &tag, NULL,
                                CBLK_ARW_WAIT_CMD_FLAGS | CBLK_ARW_USER_TAG_FLAG);
                if      (0 == rc) {--RD; lba+=2; if (lba >= nblks) lba=cnt%2;}
                else if (EBUSY == errno) {++N; usleep(USLEEP); continue;}
                else                     {io_error(id,errno);}
            }



            /* if the queuedepth is 1, don't immediately pound aresult */
            if (QD==1) usleep(USLEEP);

            /*----------------------------------------------------------------------
             * complete cmds until queue depth is QD-COMP
             *--------------------------------------------------------------------*/
            while (N < COMP)
            {
                rtag=0;
                rc = cblk_aresult(id, &rtag, &status, CBLK_ARESULT_BLOCKING|
                                                      CBLK_ARESULT_NEXT_TAG);
                if      (rc == 0) {++pollN; usleep(USLEEP); continue;}
                else if (rc < 0)  {io_error(id,errno);}
                ++cnt; tags[N++] = rtag;

            }
            /*----------------------------------------------------------------------
             * at an interval which does not impact performance, check if secs
             * have expired, and randomize lba
             *--------------------------------------------------------------------*/
            if (cnt > TI)
            {
                TI += TIME_INTERVAL;
                gettimeofday(&delta, NULL);
                if (delta.tv_sec - start.tv_sec >= nsecs) {TIME=0; COMP = QD;}
                lba = lrand48() % TIME_INTERVAL;
            }
        }
        while (TIME || QD-N);

    }
    else
    {
        printf("Testing random read with no hardware cache coherence\n");
        do
        {
            if (!RD) {RD=nRD;}
            /*----------------------------------------------------------------------
            * send up to RD reads, as long as the queuedepth N is not max
            *--------------------------------------------------------------------*/
            while (TIME && RD && N)
            {
                tag = tags[--N];

                rc = cblk_aread(id, rbuf[tag], lba, nblocks, &tag, NULL,
                            CBLK_ARW_WAIT_CMD_FLAGS | CBLK_ARW_USER_TAG_FLAG);
                if      (0 == rc) {--RD; lba+=2; if (lba >= nblks) lba=cnt%2;}
                else if (EBUSY == errno) {++N; usleep(USLEEP); continue;}
                else                     {io_error(id,errno);}
            }

            /* if the queuedepth is 1, don't immediately pound aresult */
            if (QD==1) usleep(USLEEP);

            /*----------------------------------------------------------------------
            * complete cmds until queue depth is QD-COMP
            *--------------------------------------------------------------------*/
            while (N < COMP)
            {
                rtag=0;
                rc = cblk_aresult(id, &rtag, &status, CBLK_ARESULT_BLOCKING|
                                                      CBLK_ARESULT_NEXT_TAG);
                if      (rc == 0) {++pollN; usleep(USLEEP); continue;}
                else if (rc < 0)  {io_error(id,errno);}
                ++cnt; tags[N++] = rtag;
            }
            /*----------------------------------------------------------------------
             * at an interval which does not impact performance, check if secs
             * have expired, and randomize lba
             *--------------------------------------------------------------------*/
            if (cnt > TI)
            {
                TI += TIME_INTERVAL;
                gettimeofday(&delta, NULL);
                if (delta.tv_sec - start.tv_sec >= nsecs) {TIME=0; COMP = QD;}
                lba = lrand48() % TIME_INTERVAL;
            }
        }
        while (TIME || QD-N);
    }

    /*--------------------------------------------------------------------------
     * print IO stats
     *------------------------------------------------------------------------*/
    gettimeofday(&delta, NULL);
    esecs = ((float)((delta.tv_sec*mil + delta.tv_usec) -
                     (start.tv_sec*mil + start.tv_usec))) / (float)mil;
    printf("d:%s r:%d q:%d s:%d p:%d n:%d i:%d pollN:%d mbps:%d iops:%d",
            dev, nRD, QD, nsecs, plun, nblocks, intrp_thds, pollN,
            (uint32_t)((float)((cnt*nblocks*4)/1024)/esecs),
            (uint32_t)((float)(cnt/esecs)));
    if (plun && nblocks > 1)
    {
        printf(" 4k-iops:%d", (uint32_t) ((float) (cnt * nblocks) / esecs));
    }
    printf("\n");


    chunk_stats_t stats;
    int rc_stat =  0;
    rc_stat=cblk_get_stats(id,&stats,0);
    if (rc_stat==0)
    {
        printf(" Total number of blocks written: %ld\n",stats.num_blocks_written);
        printf(" Total number of blocks read: %ld\n",stats.num_blocks_read);
        printf(" Total number of cache hits: %ld\n",stats.num_cache_hits);
        printf(" Additional parameter: %ld\n",stats.max_num_act_awrites);
        printf("\n");
    }
    else
    {
        printf("Error getting stats \n");
    }

    /*--------------------------------------------------------------------------
     * cleanup
     *------------------------------------------------------------------------*/
    for (cnt=0; cnt<QD; cnt++)
    {
        free(rbuf[cnt]);
        free(wbuf[cnt]);
    }
    free(rbuf);
    free(wbuf);
    cblk_close(id,0);
    cblk_term(NULL,0);
    return 0;
}
