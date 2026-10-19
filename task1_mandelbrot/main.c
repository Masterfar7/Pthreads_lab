#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "timer.h"

#define MAX_ITER 1000

int thread_count;
int npoints;
int total_found = 0;

double* res_x;
double* res_y;

pthread_mutex_t mutex;

int Is_in_mandelbrot(double cr, double ci) {
    double zr = 0.0, zi = 0.0;
    for (int iter = 0; iter < MAX_ITER; iter++) {
        double zr2 = zr * zr;
        double zi2 = zi * zi;
        if (zr2 + zi2 > 4.0) return 0;
        zi = 2.0 * zr * zi + ci;
        zr = zr2 - zi2 + cr;
    }
    return 1;
}

void* routine(void* rank) {
    long my_rank = (long) rank;

    int local_capacity = 500;
    double* local_x = malloc(local_capacity * sizeof(double));
    double* local_y = malloc(local_capacity * sizeof(double));
    int local_count = 0;

    int grid_n = 2000;
    double x_min = -2.0, x_max = 0.5;
    double y_min = -1.2, y_max = 1.2;
    int done = 0;

    for (int i = my_rank; i < grid_n && !done; i += thread_count) {
        pthread_mutex_lock(&mutex);
        int cur_total = total_found;
        pthread_mutex_unlock(&mutex);
        if (cur_total >= npoints) break;

        double cr = x_min + i * (x_max - x_min) / grid_n;
        for (int j = 0; j < grid_n && !done; j++) {
            double ci = y_min + j * (y_max - y_min) / grid_n;

            if (Is_in_mandelbrot(cr, ci)) {
                local_x[local_count] = cr;
                local_y[local_count] = ci;
                local_count++;

                if (local_count == local_capacity) {
                    pthread_mutex_lock(&mutex);
                    for (int k = 0; k < local_count && total_found < npoints; k++) {
                        res_x[total_found] = local_x[k];
                        res_y[total_found] = local_y[k];
                        total_found++;
                    }
                    done = (total_found >= npoints);
                    pthread_mutex_unlock(&mutex);

                    local_count = 0;
                }
            }
        }
    }

    if (local_count > 0) {
        pthread_mutex_lock(&mutex);
        for (int k = 0; k < local_count && total_found < npoints; k++) {
            res_x[total_found] = local_x[k];
            res_y[total_found] = local_y[k];
            total_found++;
        }
        pthread_mutex_unlock(&mutex);
    }

    free(local_x);
    free(local_y);
    return NULL;
}

int main(int argc, char* argv[]) {
    long thread;
    pthread_t* thread_handles;

    if (argc < 3) {
        fprintf(stderr, "usage: %s <thread_count> <npoints>\n", argv[0]);
        exit(1);
    }

    thread_count = strtol(argv[1], NULL, 10);
    npoints = strtol(argv[2], NULL, 10);

    thread_handles = malloc(thread_count * sizeof(pthread_t));
    res_x = malloc(npoints * sizeof(double));
    res_y = malloc(npoints * sizeof(double));
    pthread_mutex_init(&mutex, NULL);

    double start, end;
    GET_TIME(start);

    for (thread = 0; thread < thread_count; thread++)
        pthread_create(&thread_handles[thread], NULL, routine, (void*) thread);

    for (thread = 0; thread < thread_count; thread++)
        pthread_join(thread_handles[thread], NULL);

    GET_TIME(end);

    printf("Points found: %d / %d\n", total_found, npoints);
    printf("Time = %e seconds\n", end - start);

    FILE* f = fopen("mandelbrot.csv", "w");
    if (f) {
        fprintf(f, "x,y\n");
        for (int i = 0; i < total_found; i++)
            fprintf(f, "%.6f,%.6f\n", res_x[i], res_y[i]);
        fclose(f);
    }

    pthread_mutex_destroy(&mutex);
    free(thread_handles);
    free(res_x);
    free(res_y);

    return 0;
}