#include "arena.h"

int main() {
    int shmid = shmget(SHM_KEY, sizeof(SharedData), IPC_CREAT | 0666);
    if (shmid < 0) {
        perror("shmget failed");
        exit(1);
    }

    SharedData *shm = (SharedData *)shmat(shmid, NULL, 0);

    sem_init(&shm->mutex, 1, 1);
    
    shm->orion_ready = true;
    shm->user_count = 0; 

    printf("Orion is ready (PID: %d)\n", getpid());

    while (1) {
        sleep(1);
    }

    shmdt(shm);
    return 0;
}