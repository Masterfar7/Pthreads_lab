#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <pthread.h>
#include "timer.h"

#define G_CONST 6.67430e-11

// барьер для синхронизации потоков (мак ос не поддерживает pthread_barrier)
typedef struct {
    pthread_mutex_t mtx;
    pthread_cond_t  cv;
    int      count;
    int      total;
    unsigned gen;
} barrier_t;

static void barrier_init(barrier_t *b, int total)
{
    pthread_mutex_init(&b->mtx, NULL);
    pthread_cond_init(&b->cv, NULL);
    b->count = 0;
    b->total = total;
    b->gen   = 0;
}

static void barrier_wait(barrier_t *b)
{
    pthread_mutex_lock(&b->mtx);
    unsigned my_gen = b->gen;
    if (++b->count == b->total) {
        b->gen++;
        b->count = 0;
        pthread_cond_broadcast(&b->cv);
    } else {
        while (b->gen == my_gen)
            pthread_cond_wait(&b->cv, &b->mtx);
    }
    pthread_mutex_unlock(&b->mtx);
}

static void barrier_destroy(barrier_t *b)
{
    pthread_mutex_destroy(&b->mtx);
    pthread_cond_destroy(&b->cv);
}

static int    n, nth, nsteps;
static double dt, tend;
static double *m, *x, *y, *z, *vx, *vy, *vz;
static double **fx, **fy, **fz;
static barrier_t bar;
static FILE *out;

static void write_row(double t)
{
    fprintf(out, "%.10g", t);
    for (int i = 0; i < n; i++)
        fprintf(out, ",%.10g,%.10g", x[i], y[i]);
    fputc('\n', out);
}

static void *worker(void *arg)
{
    int id = (int)(long)arg;
    int lo = (int)((long)id * n / nth);
    int hi = (int)((long)(id + 1) * n / nth);
    double *lfx = fx[id], *lfy = fy[id], *lfz = fz[id];

    for (int s = 1; s <= nsteps; s++) {
        for (int i = 0; i < n; i++)
            lfx[i] = lfy[i] = lfz[i] = 0.0;

        for (int i = id; i < n; i += nth) {
            for (int j = i + 1; j < n; j++) {
                double dx = x[j] - x[i];
                double dy = y[j] - y[i];
                double dz = z[j] - z[i];
                double r2 = dx*dx + dy*dy + dz*dz;
                double f = G_CONST * m[i] * m[j] / (r2 * sqrt(r2));
                lfx[i] += f * dx;   lfx[j] -= f * dx;
                lfy[i] += f * dy;   lfy[j] -= f * dy;
                lfz[i] += f * dz;   lfz[j] -= f * dz;
            }
        }
        barrier_wait(&bar);

        for (int i = lo; i < hi; i++) {
            double Fx = 0, Fy = 0, Fz = 0;
            for (int t = 0; t < nth; t++) {
                Fx += fx[t][i];
                Fy += fy[t][i];
                Fz += fz[t][i];
            }
            x[i] += vx[i] * dt;
            y[i] += vy[i] * dt;
            z[i] += vz[i] * dt;
            vx[i] += Fx / m[i] * dt;
            vy[i] += Fy / m[i] * dt;
            vz[i] += Fz / m[i] * dt;
        }
        barrier_wait(&bar);

        if (id == 0)
            write_row(s * dt);
    }
    return NULL;
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr,
            "Использование: %s tend input_file [nthreads] [dt] [output.csv]\n",
            argv[0]);
        return 1;
    }
    tend = atof(argv[1]);
    FILE *in = fopen(argv[2], "r");
    if (!in) { perror("input"); return 1; }

    nth = (argc > 3) ? atoi(argv[3]) : 4;
    double dt_req = (argc > 4) ? atof(argv[4]) : tend / 1000.0;
    out = stdout;
    if (argc > 5) {
        out = fopen(argv[5], "w");
        if (!out) { perror("output"); return 1; }
    }

    if (fscanf(in, "%d", &n) != 1 || n <= 0) {
        fprintf(stderr, "Ошибка чтения n\n");
        return 1;
    }

    double *buf = malloc(sizeof(double) * 7 * n);
    if (!buf) return 1;
    m  = buf;
    x  = m  + n;   y  = x  + n;   z  = y  + n;
    vx = z  + n;   vy = vx + n;   vz = vy + n;

    for (int i = 0; i < n; i++) {
        if (fscanf(in, "%lf %lf %lf %lf %lf %lf %lf",
                   &m[i], &x[i], &y[i], &z[i],
                   &vx[i], &vy[i], &vz[i]) != 7) {
            fprintf(stderr, "Ошибка чтения строки %d\n", i + 1);
            return 1;
        }
    }
    fclose(in);

    if (nth < 1) nth = 1;
    if (nth > n) nth = n;
    if (tend <= 0 || dt_req <= 0) {
        fprintf(stderr, "tend и dt должны быть > 0\n");
        return 1;
    }

    nsteps = (int)ceil(tend / dt_req);
    dt = tend / nsteps;

    double *fbuf = malloc(sizeof(double) * 3 * (size_t)nth * n);
    fx = malloc(sizeof(double *) * 3 * nth);
    if (!fbuf || !fx) return 1;
    fy = fx + nth;
    fz = fy + nth;
    for (int t = 0; t < nth; t++) {
        fx[t] = fbuf + (size_t)(3 * t) * n;
        fy[t] = fx[t] + n;
        fz[t] = fy[t] + n;
    }

    double start, finish;
    GET_TIME(start);

    write_row(0.0);

    barrier_init(&bar, nth);
    pthread_t *th = malloc(sizeof(pthread_t) * nth);
    for (long i = 0; i < nth; i++)
        pthread_create(&th[i], NULL, worker, (void *)i);

    for (int i = 0; i < nth; i++)
        pthread_join(th[i], NULL);

    GET_TIME(finish);
    fprintf(stderr, "Время: %.6f с (потоков: %d, шагов: %d, тел: %d)\n",
            finish - start, nth, nsteps, n);

    barrier_destroy(&bar);
    if (out != stdout) fclose(out);
    free(th);
    free(fx);
    free(fbuf);
    free(buf);
    return 0;
}