#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <pthread.h>
#include "timer.h"
 
#define G 6.67430e-11
#define EPS 1e-9
 
int thread_count;
int n;
double dt = 0.01;
double tend;
int done_flag = 0;
 
pthread_mutex_t barrier_mutex;
pthread_cond_t barrier_cond;
int barrier_counter = 0;
int barrier_cycle = 0;
 
double* m;
double* x;
double* y;
double* z;
double* vx;
double* vy;
double* vz;
double* fx;
double* fy;
double* fz;
 
void Barrier_wait(void) {
    pthread_mutex_lock(&barrier_mutex);
    int my_cycle = barrier_cycle;
    barrier_counter++;
    if (barrier_counter == thread_count + 1) {
        barrier_counter = 0;
        barrier_cycle++;
        pthread_cond_broadcast(&barrier_cond);
    }
    else while (my_cycle == barrier_cycle)
        pthread_cond_wait(&barrier_cond, &barrier_mutex);
    pthread_mutex_unlock(&barrier_mutex);
}
 
void* routine(void* rank) {
    long my_rank = (long) rank;
 
    while (1) {
        Barrier_wait();
        if (done_flag) break;
 
        for (int i = my_rank; i < n; i += thread_count) {
            double fix = 0.0, fiy = 0.0, fiz = 0.0;
            for (int j = 0; j < n; j++) {
                if (i == j) continue;
                double dx = x[j] - x[i];
                double dy = y[j] - y[i];
                double dz = z[j] - z[i];
                double dist_sq = dx * dx + dy * dy + dz * dz + EPS;
                double dist = sqrt(dist_sq);
                double f_mag = (G * m[i] * m[j]) / (dist_sq * dist);
 
                fix += f_mag * dx;
                fiy += f_mag * dy;
                fiz += f_mag * dz;
            }
            fx[i] = fix;
            fy[i] = fiy;
            fz[i] = fiz;
        }
 
        Barrier_wait();
 
        for (int i = my_rank; i < n; i += thread_count) {
            x[i] += vx[i] * dt;
            y[i] += vy[i] * dt;
            z[i] += vz[i] * dt;
 
            vx[i] += (fx[i] / m[i]) * dt;
            vy[i] += (fy[i] / m[i]) * dt;
            vz[i] += (fz[i] / m[i]) * dt;
        }
 
        Barrier_wait();
    }
 
    return NULL;
}
 
int main(int argc, char* argv[]) {
    long thread;
    pthread_t* thread_handles;
 
    if (argc < 4) {
        fprintf(stderr, "usage: %s <tend> <input_file> <thread_count> [dt]\n", argv[0]);
        exit(1);
    }
 
    tend = atof(argv[1]);
    char* filename = argv[2];
    thread_count = strtol(argv[3], NULL, 10);
    if (argc >= 5) dt = atof(argv[4]);
 
    FILE* f = fopen(filename, "r");
    if (!f) {
        perror("Error opening input file");
        exit(1);
    }
 
    if (fscanf(f, "%d", &n) != 1) {
        fprintf(stderr, "Invalid input file format\n");
        exit(1);
    }
 
    m = malloc(n * sizeof(double));
    x = malloc(n * sizeof(double));
    y = malloc(n * sizeof(double));
    z = malloc(n * sizeof(double));
    vx = malloc(n * sizeof(double));
    vy = malloc(n * sizeof(double));
    vz = malloc(n * sizeof(double));
    fx = malloc(n * sizeof(double));
    fy = malloc(n * sizeof(double));
    fz = malloc(n * sizeof(double));
 
    for (int i = 0; i < n; i++) {
        if (fscanf(f, "%lf %lf %lf %lf %lf %lf %lf",
                   &m[i], &x[i], &y[i], &z[i], &vx[i], &vy[i], &vz[i]) != 7) {
            fprintf(stderr, "Error reading particle %d\n", i);
            exit(1);
        }
    }
    fclose(f);
 
    FILE* out = fopen("trajectories.csv", "w");
    if (!out) {
        perror("Error opening trajectories.csv");
        exit(1);
    }
 
    thread_handles = malloc(thread_count * sizeof(pthread_t));
    pthread_mutex_init(&barrier_mutex, NULL);
    pthread_cond_init(&barrier_cond, NULL);
 
    double start, end;
    GET_TIME(start);
 
    for (thread = 0; thread < thread_count; thread++)
        pthread_create(&thread_handles[thread], NULL, routine, (void*) thread);
 
    double t = 0.0;
    while (t <= tend + 1e-12) {
        fprintf(out, "%.4f", t);
        for (int i = 0; i < n; i++)
            fprintf(out, " %.6f %.6f", x[i], y[i]);
        fprintf(out, "\n");
 
        if (t >= tend) break;
 
        Barrier_wait();
        Barrier_wait();
        Barrier_wait();
 
        t += dt;
    }
 
    done_flag = 1;
    Barrier_wait();
 
    for (thread = 0; thread < thread_count; thread++)
        pthread_join(thread_handles[thread], NULL);
 
    GET_TIME(end);
 
    printf("Elapsed time = %e seconds\n", end - start);
    printf("Output saved to trajectories.csv\n");
 
    fclose(out);
    pthread_cond_destroy(&barrier_cond);
    pthread_mutex_destroy(&barrier_mutex);
 
    free(thread_handles);
    free(m);
    free(x); free(y); free(z);
    free(vx); free(vy); free(vz);
    free(fx); free(fy); free(fz);
 
    return 0;
}