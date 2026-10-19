#include "my_rwlock.h"

int my_rwlock_init(my_rwlock_t* rw) {
    if (rw == NULL) return -1;
    pthread_mutex_init(&rw->mutex, NULL);
    pthread_cond_init(&rw->read_cond, NULL);
    pthread_cond_init(&rw->write_cond, NULL);
    rw->readers = 0;
    rw->readers_waiting = 0;
    rw->writers_waiting = 0;
    rw->writer_active = 0;
    return 0;
}

int my_rwlock_destroy(my_rwlock_t* rw) {
    if (rw == NULL) return -1;
    pthread_mutex_destroy(&rw->mutex);
    pthread_cond_destroy(&rw->read_cond);
    pthread_cond_destroy(&rw->write_cond);
    return 0;
}

int my_rwlock_rdlock(my_rwlock_t* rw) {
    if (rw == NULL) return -1;
    pthread_mutex_lock(&rw->mutex);

    rw->readers_waiting++;
    /* Цикл while для защиты от spurious wakeup (Семинар 3) */
    while (rw->writer_active || rw->writers_waiting > 0) {
        pthread_cond_wait(&rw->read_cond, &rw->mutex);
    }
    rw->readers_waiting--;
    rw->readers++;

    pthread_mutex_unlock(&rw->mutex);
    return 0;
}

int my_rwlock_wrlock(my_rwlock_t* rw) {
    if (rw == NULL) return -1;
    pthread_mutex_lock(&rw->mutex);

    rw->writers_waiting++;
    /* Ждем, пока освободятся читатели и другие писатели */
    while (rw->writer_active || rw->readers > 0) {
        pthread_cond_wait(&rw->write_cond, &rw->mutex);
    }
    rw->writers_waiting--;
    rw->writer_active = 1;

    pthread_mutex_unlock(&rw->mutex);
    return 0;
}

int my_rwlock_unlock(my_rwlock_t* rw) {
    if (rw == NULL) return -1;
    pthread_mutex_lock(&rw->mutex);

    if (rw->writer_active) {
        rw->writer_active = 0;
    } else if (rw->readers > 0) {
        rw->readers--;
    }

    /* Приоритет писателям для избежания голодания */
    if (rw->readers == 0 && rw->writers_waiting > 0) {
        pthread_cond_signal(&rw->write_cond);
    } else if (rw->writers_waiting == 0 && rw->readers_waiting > 0) {
        pthread_cond_broadcast(&rw->read_cond);
    }

    pthread_mutex_unlock(&rw->mutex);
    return 0;
}
