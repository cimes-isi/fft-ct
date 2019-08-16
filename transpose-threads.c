/**
 * Transpose functions.
 *
 * @author Kaushik Datta <kdatta@isi.edu>
 * @author Connor Imes <cimes@isi.edu>
 * @date 2019-08-06
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "transpose-threads.h"
#include "util.h"

struct tr_thread_arg {
    const void* restrict A;
    void* restrict B;
    size_t A_rows, A_cols, r_min, r_max, c_min, c_max, thr_num;
};

static void tt_arg_init(struct tr_thread_arg *tt_arg,
                        const void* restrict A, void* restrict B,
                        size_t A_rows, size_t A_cols,
                        size_t r_min, size_t r_max, size_t c_min, size_t c_max,
                        size_t thr_num)
{
    tt_arg->A = A;
    tt_arg->B = B;
    tt_arg->A_rows = A_rows;
    tt_arg->A_cols = A_cols;
    tt_arg->r_min = r_min;
    tt_arg->r_max = r_max;
    tt_arg->c_min = c_min;
    tt_arg->c_max = c_max;
    tt_arg->thr_num = thr_num;
}

#define TRANSPOSE_BLK(A, B, A_rows, A_cols, r_min, c_min, r_max, c_max) { \
    size_t r, c; \
    for (r = (r_min); r < (r_max); r++) { \
        for (c = (c_min); c < (c_max); c++) { \
            (B)[(c) * (A_rows) + (r)] = (A)[(r) * (A_cols) + (c)]; \
        } \
    } \
}

static void *transpose_thread_flt(void *args)
{
    const struct tr_thread_arg *tt_arg = (const struct tr_thread_arg *)args;
    TRANSPOSE_BLK((const float* restrict)tt_arg->A,
                  (float* restrict )tt_arg->B,
                  tt_arg->A_rows, tt_arg->A_cols,
                  tt_arg->r_min, tt_arg->c_min, tt_arg->r_max, tt_arg->c_max);
    pthread_exit((void *)tt_arg->thr_num);
}

static void *transpose_thread_dbl(void *args)
{
    const struct tr_thread_arg *tt_arg = (const struct tr_thread_arg *)args;
    TRANSPOSE_BLK((const double* restrict)tt_arg->A,
                  (double* restrict )tt_arg->B,
                  tt_arg->A_rows, tt_arg->A_cols,
                  tt_arg->r_min, tt_arg->c_min, tt_arg->r_max, tt_arg->c_max);
    pthread_exit((void *)tt_arg->thr_num);
}

static void transpose_threads_row(const void* restrict A, void* restrict B,
                                  size_t A_rows, size_t A_cols, size_t num_thr,
                                  void *(*start_routine)(void *))
{
    size_t thr_num, r_min, r_max;
    size_t num_thr_with_max_rows, min_rows_per_thread, max_rows_per_thread;
    pthread_t *threads = assert_malloc(num_thr * sizeof(pthread_t));
    struct tr_thread_arg *args = assert_malloc(num_thr * sizeof(struct tr_thread_arg));

    // divide the rows as evenly as possible among the threads
    num_thr_with_max_rows = A_rows % num_thr;
    min_rows_per_thread = A_rows / num_thr;
    max_rows_per_thread = min_rows_per_thread + 1;

    for (thr_num = 0; thr_num < num_thr; thr_num++) {
        if (thr_num < num_thr_with_max_rows) {
            r_min = thr_num * max_rows_per_thread;
            r_max = r_min + max_rows_per_thread;
        } else {
            r_min = num_thr_with_max_rows * max_rows_per_thread +
                    (thr_num - num_thr_with_max_rows) * min_rows_per_thread;
            r_max = r_min + min_rows_per_thread;
        }
        tt_arg_init(&args[thr_num], A, B, A_rows, A_cols,
                    r_min, r_max, 0, A_cols, thr_num);
        errno = pthread_create(&threads[thr_num], NULL, start_routine,
                               &args[thr_num]);
        if (errno) {
            perror("pthread_create");
            exit(errno);
        }
    }

    // wait for the other threads
    for (thr_num = 0; thr_num < num_thr; thr_num++) {
        errno = pthread_join(threads[thr_num], NULL);
        if (errno) {
            perror("pthread_join");
            exit(errno);
        }
    }

    free(args);
    free(threads);
}

static void transpose_threads_col(const void* restrict A, void* restrict B,
                                  size_t A_rows, size_t A_cols, size_t num_thr,
                                  void *(*start_routine)(void *))
{
    size_t thr_num, c_min, c_max;
    size_t num_thr_with_max_cols, min_cols_per_thread, max_cols_per_thread;
    pthread_t *threads = assert_malloc(num_thr * sizeof(pthread_t));
    struct tr_thread_arg *args = assert_malloc(num_thr * sizeof(struct tr_thread_arg));

    // divide the columns as evenly as possible among the threads
    num_thr_with_max_cols = A_cols % num_thr;
    min_cols_per_thread = A_cols / num_thr;
    max_cols_per_thread = min_cols_per_thread + 1;

    for (thr_num = 0; thr_num < num_thr; thr_num++) {
        if (thr_num < num_thr_with_max_cols) {
            c_min = thr_num * max_cols_per_thread;
            c_max = c_min + max_cols_per_thread;
        } else {
            c_min = num_thr_with_max_cols * max_cols_per_thread +
                    (thr_num - num_thr_with_max_cols) * min_cols_per_thread;
            c_max = c_min + min_cols_per_thread;
        }
        tt_arg_init(&args[thr_num], A, B, A_rows, A_cols,
                    0, A_rows, c_min, c_max, thr_num);
        errno = pthread_create(&threads[thr_num], NULL, start_routine,
                               &args[thr_num]);
        if (errno) {
            perror("pthread_create");
            exit(errno);
        }
    }

    // wait for the other threads
    for (thr_num = 0; thr_num < num_thr; thr_num++) {
        errno = pthread_join(threads[thr_num], NULL);
        if (errno) {
            perror("pthread_join");
            exit(errno);
        }
    }

    free(args);
    free(threads);
}

void transpose_flt_threads_row(const float* restrict A, float* restrict B,
                               size_t A_rows, size_t A_cols,
                               size_t num_thr)
{
    transpose_threads_row(A, B, A_rows, A_cols, num_thr, &transpose_thread_flt);
}

void transpose_dbl_threads_row(const double* restrict A, double* restrict B,
                               size_t A_rows, size_t A_cols,
                               size_t num_thr)
{
    transpose_threads_row(A, B, A_rows, A_cols, num_thr, &transpose_thread_dbl);
}

void transpose_flt_threads_col(const float* restrict A, float* restrict B,
                               size_t A_rows, size_t A_cols,
                               size_t num_thr)
{
    transpose_threads_col(A, B, A_rows, A_cols, num_thr, &transpose_thread_flt);
}

void transpose_dbl_threads_col(const double* restrict A, double* restrict B,
                               size_t A_rows, size_t A_cols,
                               size_t num_thr)
{
    transpose_threads_col(A, B, A_rows, A_cols, num_thr, &transpose_thread_dbl);
}

void transpose_flt_threads_row_blocked(const float* restrict A,
                                       float* restrict B,
                                       size_t A_rows, size_t A_cols,
                                       size_t num_thr,
                                       size_t blk_rows, size_t blk_cols)
{
    // TODO
    exit(-1);
}

void transpose_dbl_threads_row_blocked(const double* restrict A,
                                       double* restrict B,
                                       size_t A_rows, size_t A_cols,
                                       size_t num_thr,
                                       size_t blk_rows, size_t blk_cols)
{
    // TODO
    exit(-1);
}

void transpose_flt_threads_col_blocked(const float* restrict A,
                                       float* restrict B,
                                       size_t A_rows, size_t A_cols,
                                       size_t num_thr,
                                       size_t blk_rows, size_t blk_cols)
{
    // TODO
    exit(-1);
}

void transpose_dbl_threads_col_blocked(const double* restrict A,
                                       double* restrict B,
                                       size_t A_rows, size_t A_cols,
                                       size_t num_thr,
                                       size_t blk_rows, size_t blk_cols)
{
    // TODO
    exit(-1);
}
