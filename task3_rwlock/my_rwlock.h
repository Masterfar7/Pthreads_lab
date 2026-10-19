#ifndef _MY_RWLOCK_H_
#define _MY_RWLOCK_H_

#include <pthread.h>

/*
 * Структура блокировки чтения-записи (в точности по методичке разд. 2.3.2):
 * - Мьютекс
 * - Две условные переменные (для читателей и для писателей)
 * - Счетчик читателей (сколько сейчас читают)
 * - Счетчик читателей, ожидающих блокировки
 * - Счетчик писателей, ожидающих блокировки
 * - Флаг активного писателя (занята ли блокировка писателем)
 */
typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t read_cond;
    pthread_cond_t write_cond;
    int readers;          /* Текущее число активных читателей */
    int readers_waiting;  /* Читатели в очереди ожидания */
    int writers_waiting;  /* Писатели в очереди ожидания */
    int writer_active;    /* 1 если есть активный писатель, иначе 0 */
} my_rwlock_t;

int my_rwlock_init(my_rwlock_t *rw);
int my_rwlock_destroy(my_rwlock_t *rw);
int my_rwlock_rdlock(my_rwlock_t *rw);
int my_rwlock_wrlock(my_rwlock_t *rw);
int my_rwlock_unlock(my_rwlock_t *rw);

#endif
