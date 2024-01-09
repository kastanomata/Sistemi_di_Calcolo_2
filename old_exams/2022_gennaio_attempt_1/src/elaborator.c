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
#include <signal.h>
#include "common.h"


// definizione shared memory
int fd_shm;
struct shared_memory *myshm_ptr;

void initMemory();
void openSemaphores();
void close_everything();
void consume();

/* Signal Handler for SIGINT */
void sigintHandler(int sig_num)
{
    printf("\n SIGINT or CTRL-C detected. Exiting gracefully \n");
    close_everything();
    fflush(stdout);
    exit(0);
}

int main(int argc, char** argv) {
    /* Set the SIGINT (Ctrl-C) signal handler to sigintHandler
       Refer http://en.cppreference.com/w/c/program/signal */
    signal(SIGINT, sigintHandler);
    printf("creating shared memory\n");
    fflush(stdout);
    initMemory();
    printf("opening semaphores\n");
    fflush(stdout);
    openSemaphores();

    consume();
    //we never reach this point
    exit(EXIT_SUCCESS);
}

void initMemory() {
     /**
     * TODO:
     * Obiettivi:
     * - richiedere al kernel di creare una memoria condivisa (nome definito in common.h)
     * - configurare la sua dimensione per contenere la struttura struct shared_memory
     * - mappare la memoria condivisa nel puntatore myshm_ptr 
     * - inizializzare la memoria a 0
     * - Gestire gli errori.
     */
    int ret;
    fd_shm = shm_open(SH_MEM_NAME, O_CREAT | O_EXCL | O_RDWR, 0600);
    if(fd_shm == -1) handle_error("Error while creating shared_memory");
    ret = ftruncate(fd_shm, sizeof(struct shared_memory));
    if(ret == -1) handle_error("Error while truncating shared_memory");
    myshm_ptr = mmap(NULL, sizeof(struct shared_memory), PROT_READ | PROT_WRITE, MAP_SHARED, fd_shm, 0);
    if(myshm_ptr == MAP_FAILED) handle_error("Error while mapping shared_memory to pointer");
    memset(myshm_ptr, 0, sizeof(struct shared_memory));
}

void openSemaphores() {
    int ret;
     /**
     * TODO:
     * Obiettivi:
     * - inizializzare i semafori definiti in common.h e presenti all'interno della memoria condivisa
     *   ATTENZIONE: i semafori sono condivisi tra processi che non sono legati da parentela (fork)
     *   non lo abbiamo visto nelle esercitazioni, ma ho comunque detto che c'è un parametro
     *   da settare opportunamente in questo caso
     * - gestire gli errori
     */
    // sem_t empty_sem; -> tiene conto di quanti indici sono liberi
    ret = sem_init(&(myshm_ptr->empty_sem), 1, BUFFER_SIZE);
    if(ret == -1) handle_error("Error while initializing empty_sem");
    // sem_t full_sem;  -> tiene conto di quanti indici sono occupate
    ret = sem_init(&(myshm_ptr->full_sem), 1, 0); 
    if(ret == -1) handle_error("Error while initializing full_sem");
    // sem_t cs_sem;    -> gestisce la mutuale exclusion
    ret = sem_init(&(myshm_ptr->cs_sem), 1, 1);
    if(ret == -1) handle_error("Error while initializing cs_sem");
    
}

void close_everything() {
    int ret;
    /** 
     * TODO:
     * Obiettivi:
     * - chiudere i semafori
     * - chiudere la memoria condivisa
     * - chiedere al kernel di eliminare la memoria condivisa
     * - gestire gli errori 
     */
    ret = sem_destroy(&(myshm_ptr->empty_sem));
    if(ret == -1) handle_error("Error while destroying empty_semaphore");
    ret = sem_destroy(&(myshm_ptr->full_sem));
    if(ret == -1) handle_error("Error while destroying full_semaphore");
    ret = sem_destroy(&(myshm_ptr->cs_sem));
    if(ret == -1) handle_error("Error while destroying cs semaphore");
    ret = close(fd_shm);
    if(ret == -1) handle_error("Error while closing shared_memory file descriptor");
    ret = munmap(myshm_ptr, sizeof(struct shared_memory));
    if(ret == -1) handle_error("Error while unmapping shared_memory data pointer");
    ret = shm_unlink(SH_MEM_NAME);
    if(ret == -1) handle_error("Error while unlinking shared_memory");

}

void consume(){
    int numOps = 0;
    int totalreward = 0;
    while (1) {
        int ret;
        printf("ready to read an element\n");fflush(stdout);

        /** 
         * TODO:
         * L'elaborator preleva un elemento dal buffer e lo elabora (simulato con una pausa) 
         * Occasionalmente stampa quanto ha guadagnato da tutti i task
         * 
         * Obiettivi:
         * - gestire opportunamente la sezione critica tramite i semafori
         * - gestire gli errori 
         **/
        ret = sem_wait(&(myshm_ptr->full_sem));
        if(ret == -1) handle_error("Error while waiting for resource to consume");
        // INIZIO CS
        ret = sem_wait(&(myshm_ptr->cs_sem));
        if(ret == -1) handle_error("Error while entering CS");
        printf("reading an element\n");fflush(stdout);
        struct cell value = myshm_ptr->buf[myshm_ptr->read_index];
        if (myshm_ptr->read_index == BUFFER_SIZE-1)
            myshm_ptr->read_index = 0;
        else
            myshm_ptr->read_index++;
        ret = sem_post(&(myshm_ptr->cs_sem));
        if(ret == -1) handle_error("Error while exiting CS");
        ret = sem_post(&(myshm_ptr->empty_sem));
        if(ret == -1) handle_error("Error while updating number of resources to consumer");
        // FINE CS
        printf("Elaborating value %d\n",value.input);
        printf("Elaborating reward %d\n",value.reward);
        struct timespec pause = {0};
        pause.tv_sec = 2;
        nanosleep(&pause, NULL);

        totalreward += value.reward;
        numOps++;
        if (numOps%10 == 0)
            printf("Total server reward: %d\n", totalreward);
    }
}
