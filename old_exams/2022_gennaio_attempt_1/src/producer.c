#include <string.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>       // nanosleep()
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include "common.h"

//definizione shared memory
struct shared_memory *myshm_ptr;
int fd_shm;

void openMemory();
void request();
void closeSemaphores();
void closeMemory();

int main(int argc, char** argv) {
    initRandomGenerator();

    //load memory
    printf("opening shared memory\n");fflush(stdout);
    openMemory();

    //request cycle
    request();

    //close semaphores
    closeSemaphores();

    //close memory
    closeMemory();

    exit(EXIT_SUCCESS);
}

void openMemory() {
     /**
     * TODO:
     * Obiettivi:
     * - richiedere al kernel l'accesso alla memoria condivisa creata da elaborator.c
     * - Gestire gli errori.
     */    
    fd_shm = shm_open(SH_MEM_NAME, O_RDWR, 0666);
    if(fd_shm == -1) handle_error("Error while opening shared_memory");
    myshm_ptr = mmap(NULL, sizeof(struct shared_memory), PROT_READ | PROT_WRITE, MAP_SHARED, fd_shm, 0);
    if(myshm_ptr == MAP_FAILED) handle_error("Error while mapping shared_memory to pointer");
}

void request() {
    int balance = generateRandomNumber(10000);
    while(1){
        /** 
         * TODO:
         * Il producer genera una serie di input da scrivere nel buffer 
         * Ogni input dovrà essere elaborato dall'elaborator
         * a cui viene corrisposto un pagamento (reward)
         * Quando il pagamento supera la disponibilità il producer termina la
         * propria attività
         * 
         * Obiettivi:
         * - gestire la sezione critica opportunamente tramite i semafori
         * - gestire gli errori 
         */

        int reward = generateRandomNumber(1000);
        if (reward>balance){
            printf("not enough money, exit\n");fflush(stdout);
            break;
        }
        balance -= reward;

        int input = generateRandomNumber(100);
        printf("Generated input %d\n",input);
        struct timespec pause = {0};
        pause.tv_nsec = 500000000; // 0.5 s (1*10^9 ns)
        nanosleep(&pause, NULL);
        int ret;
        ret = sem_wait(&(myshm_ptr->empty_sem));
        if(ret == -1) handle_error("Error while waiting for consumer to free space");
        ret = sem_wait(&(myshm_ptr->cs_sem));
        if(ret == -1) handle_error("Error while waiting to enter CS");
        // INIZIO CS
        myshm_ptr->buf[myshm_ptr->write_index].reward = reward;
        myshm_ptr->buf[myshm_ptr->write_index].input = input;
        myshm_ptr->write_index++;
        if (myshm_ptr->write_index == BUFFER_SIZE)
            myshm_ptr->write_index = 0;
        // FINE CS
        ret = sem_post(&(myshm_ptr->cs_sem));
        if(ret == -1) handle_error("Error while signaling end of CS");
        ret = sem_post(&(myshm_ptr->full_sem));
        if(ret == -1) handle_error("Error while signaling product to elaborate");
    }

}

void closeSemaphores() {
    int ret;
    /** 
     * TODO:
     * Obiettivi:
     * - gestire la chiusura dei semafori
     * - gestire gli errori 
     */
    ret = sem_destroy(&(myshm_ptr->empty_sem));
    if(ret == -1) handle_error("Error while destroying empty_semaphore");
    ret = sem_destroy(&(myshm_ptr->full_sem));
    if(ret == -1) handle_error("Error while destroying full_semaphore");
    ret = sem_destroy(&(myshm_ptr->cs_sem));
    if(ret == -1) handle_error("Error while destroying cs_semaphore");
}

void closeMemory() {
    int ret;
    /** 
     * TODO:
     * Obiettivi:
     * - chiudere la memoria condivisa
     * - chiedere al kernel di eliminare la memoria condivisa
     * - gestire gli errori 
     */
    ret = close(fd_shm);
    if(ret == -1) handle_error("Error while closing shared memory");
    ret = munmap(myshm_ptr, sizeof(struct shared_memory));
    if(ret == -1) handle_error("Error while unmapping shared memory");

}

