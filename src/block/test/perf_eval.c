//
// Created by Jiamin Huang on 1/19/16.
//
#define _GNU_SOURCE
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
#include <sys/types.h>
#include <unistd.h>
#include <linux/aio_abi.h>
#include <sys/syscall.h>

#define _4K (4*1024)
#define MAX_BLK 40
#define NUM_BLK 1048576
#define NUM_EXP 10000
#define BLOCK_STEP 1
#define QD 500
#define CAPI 0
#define FILE 1
#define READ 0
#define WRITE 1
#define PLUN 1

#define MIN(x, y) ((x < y) ? (x) : (y))

int SEQUENTIAL = 0;

typedef struct timeval timeval;
typedef struct timespec timespec;
typedef struct iocb iocb;
typedef struct io_event io_event;
extern int posix_memalign (void **__memptr, size_t __alignment, size_t __size);

inline int io_setup(unsigned nr, aio_context_t *ctxp)
{
    return syscall(__NR_io_setup, nr, ctxp);
}

inline int io_destroy(aio_context_t ctx) 
{
    return syscall(__NR_io_destroy, ctx);
}

inline int io_submit(aio_context_t ctx, long nr,  struct iocb **iocbpp) 
{
    return syscall(__NR_io_submit, ctx, nr, iocbpp);
}

inline int io_getevents(aio_context_t ctx, long min_nr, long max_nr,
        struct io_event *events, struct timespec *timeout)
{
    return syscall(__NR_io_getevents, ctx, min_nr, max_nr, events, timeout);
}

off_t rand_4k_offset(size_t num_blocks) {
    return ((off_t) rand()) % (NUM_BLK - num_blocks);
}

int open_capi() {
    int rc = cblk_init(NULL,0);
    if (rc) {
        fprintf(stderr,"cblk_init failed with rc = %d and errno = %d\n",
                rc,errno);
        exit(1);
    }
    int flags = 0;
    if (!PLUN) {
        flags  = CBLK_OPN_VIRT_LUN;
    }
    // flags |= CBLK_OPN_NO_INTRP_THREADS;
    int id = cblk_open("/dev/sg15", QD, O_RDWR, 0, flags);
    if (id == NULL_CHUNK_ID) {
        if      (ENOSPC == errno) fprintf(stderr,"cblk_open: ENOSPC\n");
        else if (ENODEV == errno) fprintf(stderr,"cblk_open: ENODEV\n");
        else                      fprintf(stderr,"cblk_open: errno:%d\n",errno);
        cblk_term(NULL,0);
        exit(errno);
    }

    size_t nblks = 0;
    if (!PLUN) {
        rc = cblk_get_lun_size(id, &nblks, 0);
    } else {
        rc = cblk_get_size(id, &nblks, 0);
    }
    if (rc) {
        fprintf(stderr, "cblk_get_lun_size failed: errno: %d\n", errno);
        exit(errno);
    }
    if (!PLUN) {
        nblks = nblks < MAX_BLK ? MAX_BLK : nblks;
        rc = cblk_set_size(id, nblks, 0);
        if (rc) {
            fprintf(stderr, "cblk_set_size failed, errno: %d\n", errno);
            exit(errno);
        }
    }
    return id;
}

void io_error(int id, int err) {
    if (id > 0) {
        cblk_close(id,0);
        cblk_term(NULL,0);
    }
    printf("IO Error: %s\n", strerror(err));
    exit(err);
}

void drop_caches() {
    int fd;
    char* data = "3";

    sync();
    fd = open("/proc/sys/vm/drop_caches", O_WRONLY);
    write(fd, data, sizeof(char));
    close(fd);
}

long diff_time(timeval start, timeval end) {
    return (end.tv_usec - start.tv_usec) + (end.tv_sec - start.tv_sec) * 1000000;
}

void test_capi(int op, int sequential) {
    int rc = 0;
    int id = open_capi();
    uint8_t *buffer = NULL;
    if ((rc = posix_memalign((void **) &buffer, _4K, _4K * MAX_BLK))) {
        fprintf(stderr, "posix_memalign failed, size=%d, rc=%d\n",
                _4K * MAX_BLK, rc);
        cblk_close(id, 0);
        cblk_term(NULL, 0);
        exit(0);
    }
    if (op == WRITE) {
        memset(buffer, 0x79, _4K * MAX_BLK);
    }
    for (size_t num_blocks = 1; num_blocks <= MAX_BLK; num_blocks += BLOCK_STEP) {
        int lba = 0;
        timeval start;
        timeval end;
        clock_t cpu_start;
        clock_t cpu_end;
        gettimeofday(&start, NULL);
        cpu_start = clock();
        for (int count = 0; count < (NUM_EXP / 20); ++count) {
            if (op == READ) {
                rc = cblk_read(id, buffer, lba, num_blocks, 0);
            } else {
                rc = cblk_write(id, buffer, lba, num_blocks, 0);
            }
            if (rc > 0) {
                if (sequential) {
                    lba += num_blocks;
                } else {
                    lba = rand_4k_offset(num_blocks);
                }
            } else if (rc < 0) {
                io_error(id, errno);
            }
        }
        cpu_end = clock();
        gettimeofday(&end, NULL);
        long total_time = diff_time(start, end);
        double cpu_usage = ((cpu_end - cpu_start)) / (double) total_time;
        printf("%ld,%ld,%f,%f\n", num_blocks, total_time / (NUM_EXP / 20), cpu_usage * 100, (NUM_EXP / 20) * (1000000.0 / total_time));
    }
    free(buffer);
    cblk_close(id, 0);
    cblk_term(NULL, 0);
}

void test_capi_async(int op, int sequential) {
    int rc = 0;
    int id = open_capi();
    uint8_t *buffer = NULL;
    if ((rc = posix_memalign((void **) &buffer, _4K, _4K * MAX_BLK))) {
        fprintf(stderr, "posix_memalign failed, size=%d, rc=%d\n",
                _4K * MAX_BLK, rc);
        cblk_close(id, 0);
        cblk_term(NULL, 0);
        exit(0);
    }
    if (op == WRITE) {
        memset(buffer, 0x79, _4K * MAX_BLK);
    }
    for (size_t num_blocks = 1; num_blocks <= MAX_BLK; num_blocks += BLOCK_STEP) {
        int lba = 0;
        int tag = 0;
        timeval start;
        timeval end;
        uint64_t status = 0;
        int cmds_finished = 0;
        int queue_size = 0;
        int queue_thres = QD  / 8;
        clock_t cpu_start;
        clock_t cpu_end;
        gettimeofday(&start, NULL);
        cpu_start = clock();
        while (cmds_finished < NUM_EXP)
        {
            int num_cmds = MIN(QD - queue_size, 100);
            // printf("%d commands to send\n", num_cmds);
            for (int count = 0; count < num_cmds; ++count) {
                // puts("Sending commands");
                if (op == READ) {
                    rc = cblk_aread(id, buffer, lba, num_blocks, &tag, NULL,
                             CBLK_ARW_WAIT_CMD_FLAGS);
                } else {
                    rc = cblk_awrite(id, buffer, lba, num_blocks, &tag, NULL,
                             CBLK_ARW_WAIT_CMD_FLAGS);
                }
                // printf("%d commands sent\n", queue_size);
                if (rc >= 0) {
                    if (sequential) {
                        lba += num_blocks;
                    } else {
                        lba = rand_4k_offset(num_blocks);
                    }
                    ++queue_size;
                } else if (rc < 0) {
                    io_error(id, errno);
                }
            }
            for (int count = 0; cmds_finished < NUM_EXP && QD - queue_size < queue_thres; ++count) {
                // puts("Waiting for result");
                rc = cblk_aresult(id, &tag, &status, CBLK_ARESULT_BLOCKING|
                                                     CBLK_ARESULT_NEXT_TAG);
                if (rc < 0) {
                    io_error(id, errno);
                } else if (rc == 0) {
                    puts("Poll");
                    usleep(80);
                    count--;
                } else {
                    ++cmds_finished;
                    --queue_size;
                }
                // puts("Result fetched");
            }
        }
        cpu_end = clock();
        gettimeofday(&end, NULL);
        long total_time = diff_time(start, end);
        double cpu_usage = ((cpu_end - cpu_start)) / (double) total_time;
        printf("%ld,%ld,%f,%f\n", num_blocks, total_time / NUM_EXP, cpu_usage * 100, NUM_EXP * (1000000.0 / total_time));
        // printf("%d commands to comsume before restart\n", queue_size);
        while (queue_size > 0)
        {
            rc = cblk_aresult(id, &tag, &status, CBLK_ARESULT_BLOCKING|
                                                 CBLK_ARESULT_NEXT_TAG);
            if (rc < 0) {
                printf("%d left before finishing\n", queue_size);
                io_error(id, errno);
            } else if (rc == 0) {
                // puts("Poll");
                ++queue_size;
            }
            --queue_size;
        }
        // printf("All consumed\n");
    }
    free(buffer);
    cblk_close(id, 0);
    cblk_term(NULL, 0);
}

void test_capi_async_timed(int op, int sequential) {
    int rc = 0;
    int id = open_capi();
    uint8_t *buffer = NULL;
    if ((rc = posix_memalign((void **) &buffer, _4K, _4K * MAX_BLK))) {
        fprintf(stderr, "posix_memalign failed, size=%d, rc=%d\n",
                _4K * MAX_BLK, rc);
        cblk_close(id, 0);
        cblk_term(NULL, 0);
        exit(0);
    }
    if (op == WRITE) {
        memset(buffer, 0x79, _4K * MAX_BLK);
    }
    for (size_t num_blocks = 1; num_blocks <= MAX_BLK; num_blocks += BLOCK_STEP) {
        int lba = 0;
        int tag = 0;
        timeval start;
        timeval end;
        uint64_t status = 0;
        int cmds_finished = 0;
        int queue_size = 0;
        int queue_thres = QD  / 8;
        int TI = 1024;
        clock_t cpu_start;
        clock_t cpu_end;
        long total_time = 0;
        gettimeofday(&start, NULL);
        cpu_start = clock();
        while (total_time < 2000000)
        {
            int num_cmds = MIN(QD - queue_size, 100);
            // printf("%d commands to send\n", num_cmds);
            for (int count = 0; count < num_cmds; ++count) {
                // puts("Sending commands");
                if (op == READ) {
                    rc = cblk_aread(id, buffer, lba, num_blocks, &tag, NULL,
                             CBLK_ARW_WAIT_CMD_FLAGS);
                } else {
                    rc = cblk_awrite(id, buffer, lba, num_blocks, &tag, NULL,
                             CBLK_ARW_WAIT_CMD_FLAGS);
                }
                // printf("%d commands sent\n", queue_size);
                if (rc >= 0) {
                    if (sequential) {
                        lba += num_blocks;
                    } else {
                        lba = rand_4k_offset(num_blocks);
                    }
                    ++queue_size;
                } else if (rc < 0) {
                    io_error(id, errno);
                }
            }
            for (int count = 0; QD - queue_size < queue_thres; ++count) {
                // puts("Waiting for result");
                rc = cblk_aresult(id, &tag, &status, CBLK_ARESULT_BLOCKING|
                                                     CBLK_ARESULT_NEXT_TAG);
                if (rc < 0) {
                    io_error(id, errno);
                } else if (rc == 0) {
                    puts("Poll");
                    usleep(80);
                    count--;
                } else {
                    ++cmds_finished;
                    --queue_size;
                }
                // puts("Result fetched");
            }

            if (cmds_finished > TI) {
                TI += 1024;
                gettimeofday(&end, NULL);
                total_time = diff_time(start, end);
            }
        }
        cpu_end = clock();
        double cpu_usage = ((cpu_end - cpu_start)) / (double) total_time;
        printf("%ld,%ld,%f,%f\n", num_blocks, total_time / cmds_finished, cpu_usage * 100, cmds_finished * (1000000.0 / total_time));
        // printf("%d commands to comsume before restart\n", queue_size);
        while (queue_size > 0)
        {
            rc = cblk_aresult(id, &tag, &status, CBLK_ARESULT_BLOCKING|
                                                 CBLK_ARESULT_NEXT_TAG);
            if (rc < 0) {
                printf("%d left before finishing\n", queue_size);
                io_error(id, errno);
            } else if (rc == 0) {
                // puts("Poll");
                ++queue_size;
            }
            --queue_size;
        }
        // printf("All consumed\n");
    }
    free(buffer);
    cblk_close(id, 0);
    cblk_term(NULL, 0);
}

void test_file(int op, int sequential) {
    int fd = open("/root/flash/flash_test", O_RDWR, 0777);
    if (fd == -1) {
        io_error(-1, errno);
        exit(0);
    }
    long rc = 0;
    uint8_t *buffer;
    if ((rc = posix_memalign((void **) &buffer, _4K, _4K * MAX_BLK))) {
        fprintf(stderr, "posix_memalign failed, size=%d, rc=%ld\n",
                _4K * MAX_BLK, rc);
        close(fd);
        exit(0);
    }
    if (op == WRITE) {
        memset(buffer, 0x79, _4K * MAX_BLK);
    }
    for (size_t num_blocks = 1; num_blocks <= MAX_BLK; num_blocks += BLOCK_STEP) {
        timeval start;
        timeval end;
        clock_t cpu_start;
        clock_t cpu_end;
        long total_time = 0;
        long total_cpu = 0;
        lseek(fd, 0, SEEK_SET);
        int num_exp = MIN(NUM_EXP, 500);
        for (int count = 0; count < num_exp; ++count) {
            if (!SEQUENTIAL) {
                off_t off_set = rand_4k_offset(num_blocks) * _4K;
                if (lseek(fd, off_set, SEEK_SET) != off_set) {
                    printf("%zu\n", off_set);
                    io_error(-1, errno);
                }
            }
            gettimeofday(&start, NULL);
            cpu_start = clock();
            if (op == READ) {
                rc = read(fd, buffer, _4K * num_blocks);
            } else {
                rc = write(fd, buffer, _4K * num_blocks);
                rc = fsync(fd);
            }
            if (rc < 0) {
                io_error(-1, errno);
            }
            cpu_end = clock();
            gettimeofday(&end, NULL);
            total_time += diff_time(start, end);
            total_cpu += cpu_end - cpu_start;
            drop_caches();
        }
        double cpu_usage = total_cpu / (double) total_time;
        printf("%ld,%ld,%f,%f\n", num_blocks, total_time / num_exp, cpu_usage * 100, num_exp * (1000000.0 / total_time));
    }
    free(buffer);
    close(fd);
}

void test_file_async(int op, int sequential) {
    int fd = open("/flash_test", O_RDWR | O_DIRECT, 0777);
    if (fd == -1) {
        io_error(-1, errno);
        exit(0);
    }
    long rc = 0;
    aio_context_t ctx = 0;
    rc = io_setup(500, &ctx);
    if (rc < 0) {
        io_error(-1, errno);
    }

    uint8_t *buffer;
    if ((rc = posix_memalign((void **) &buffer, _4K, _4K * MAX_BLK))) {
        fprintf(stderr, "posix_memalign failed, size=%d, rc=%ld\n",
                _4K * MAX_BLK, rc);
        close(fd);
        exit(0);
    }
    if (op == WRITE) {
        memset(buffer, 0x79, _4K * MAX_BLK);
    }
    iocb *cmds[QD];
    io_event events[QD];
    for (int count = 0; count < QD; ++count) {
        iocb *cmd = (iocb *) malloc(sizeof(iocb));
        memset(cmd, 0, sizeof(iocb));
        cmd->aio_fildes = fd;
        if (op == WRITE) {
            cmd->aio_lio_opcode = IOCB_CMD_PWRITE;
        } else {
            cmd->aio_lio_opcode = IOCB_CMD_PREAD;
        }
        cmd->aio_buf = (uint64_t) buffer;
        cmd->aio_offset = 0;
        cmd->aio_nbytes = _4K * MAX_BLK;
        cmds[count] = cmd;
    }
    timespec timeout;
    timeout.tv_sec = 1;
    timeout.tv_nsec = 0;
    for (size_t num_blocks = 1; num_blocks <= MAX_BLK; num_blocks += BLOCK_STEP) {
        timeval start;
        timeval end;
        clock_t cpu_start;
        clock_t cpu_end;
        long total_time = 0;
        long total_cpu = 0;
        for (int count = 0; count < QD; ++count) {
            cmds[count]->aio_nbytes = _4K * num_blocks;
            if (SEQUENTIAL) {
                cmds[count]->aio_offset = count * _4K * num_blocks;
            } else {
                cmds[count]->aio_offset = rand_4k_offset(num_blocks);
            }
        }
        lseek(fd, 0, SEEK_SET);
        int num_exp = MIN(NUM_EXP, 500);
        int num_exps = num_exp;
        while (num_exps > 0) {
            int exps = MIN(100, num_exps);
            num_exps -= exps;
            gettimeofday(&start, NULL);
            cpu_start = clock();
            rc = io_submit(ctx, exps, cmds);
            if (rc < 0) {
                io_error(-1, errno);
            }
            rc = io_getevents(ctx, exps, exps, events, NULL);
            if (rc < 0) {
                io_error(-1, errno);
            }
            cpu_end = clock();
            gettimeofday(&end, NULL);
            total_time += diff_time(start, end);
            total_cpu += cpu_end - cpu_start;
            drop_caches();
        }
        double cpu_usage = total_cpu / (double) total_time;
        printf("%ld,%ld,%f,%f\n", num_blocks, total_time / num_exp, cpu_usage * 100, num_exp * (1000000.0 / total_time));
    }
    free(buffer);
    for (int count = 0; count < QD; ++count) {
        free(cmds[count]);
    }
    io_destroy(ctx);
    if (rc < 0) {
        io_error(-1, errno);
    }
    close(fd);
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        puts("Usage: ./pref_eval capi/file seq/rad read/write sync/async");
        exit(0);
    }
    int mode = 0;
    int op = 0;

    if (strcmp(argv[1], "capi") == 0) {
        mode = CAPI;
    } else if (strcmp(argv[1], "file") == 0) {
        mode = FILE;
    } else {
        puts("Unsupported mode");
        exit(0);
    }

    if (strcmp(argv[2], "seq") == 0) {
        SEQUENTIAL = 1;
    } else if (strcmp(argv[2], "rad") == 0) {
        SEQUENTIAL = 0;
    } else {
        puts("Unsupported mode");
        exit(0);
    }

    if (strcmp(argv[3], "read") == 0) {
        op = READ;
    } else if (strcmp(argv[3], "write") == 0) {
        op = WRITE;
    } else {
        puts("Unsupported op");
        exit(0);
    }

    srand(time(NULL));


    if (strcmp(argv[4], "sync") == 0) {
        if (mode ==  CAPI) {
            test_capi(op, SEQUENTIAL);
        } else {
            test_file(op, SEQUENTIAL);
        }
    } else if (strcmp(argv[4], "async") == 0) {
        if (mode ==  CAPI) {
            test_capi_async_timed(op, SEQUENTIAL);
        } else {
            test_file_async(op, SEQUENTIAL);
        }
    } else {
        puts("Unsupported sync mode");
        exit(0);
    }
    return 0;
}
