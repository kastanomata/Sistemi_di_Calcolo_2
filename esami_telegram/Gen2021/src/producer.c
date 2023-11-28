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


void openMemory() {
     /**
     * COMPLETARE QUI
     *
     * Obiettivi:
     * - ++richiedere al kernel l'accesso alla memoria condivisa creata da elaborator.c
     * -++ Gestire gli errori.
     *
     */   
     int ret;
     
     fd_shm= shm_open(SH_MEM_NAME,O_EXCL| O_RDWR,0666);
     if(fd_shm<0) handle_error("Producer: shm_open error");
     
     ret=ftruncate(fd_shm, sizeof(struct shared_memory));
     if(ret<0) handle_error("Producer: ftruncate error");
     
     myshm_ptr= mmap(0, sizeof(struct shared_memory),PROT_READ | PROT_WRITE,MAP_SHARED,fd_shm,0);
     if(myshm_ptr==MAP_FAILED) handle_error("Producer: mmap error");
     
      
   

}

void request(){
    int balance = generateRandomNumber(10000),ret;
    printf("+++++Ho un bilancio di %d\n",balance);
    while(1){
        /** 
         * COMPLETARE QUI
         *
         * Il producer genera una serie di input da scrivere nel buffer 
         * Ogni input dovraà essere elaborato dall'elaborator
         * a cui viene corrisposto un pagamento (reward)
         * Quando il pagamento supera la disponibilità il producer termina la
         * propria attività
         * 
         * Obiettivi:
         * - gestire la sezione critica opportunamente tramite i semafori
         * - gestire gli errori 
         */
         int my_input=generateRandomNumber(500);
         int my_reward= 500;
         
         if(my_reward>balance){
			 fprintf(stdout,"Producer request: NON POSSO PIU' PAGARE ...exiting\n\n");
			 break;
		}
         
         
         ret=sem_wait(&(myshm_ptr->sem_empty));
         if(ret<0) handle_error("Producer request(): sem_wait error sem_empty");
         
         ret=sem_wait(&(myshm_ptr->sem_mem));
         if(ret<0) handle_error("Producer request(): sem_wait error sem_mem");
         
         //==============||| CS ||=============================================0
         
         myshm_ptr->buf[myshm_ptr->write_index].input=my_input;
         myshm_ptr->buf[myshm_ptr->write_index].reward= my_reward;
         
         myshm_ptr->write_index= (myshm_ptr->write_index+1)%BUFFER_SIZE;
         
         //ESCO DALLA SEZIONE CRITICA========================================
         
         balance-=my_reward;
         
          ret=sem_post(&(myshm_ptr->sem_mem));
          if(ret<0) handle_error("Producer request(): sem_post error sem_mem");
          
         
         
         ret=sem_post(&(myshm_ptr->sem_filled));
          if(ret<0) handle_error("Producer request(): sem_post error sem_filled");

      
    }

}

void closeSemaphores() {
    /** 
     * COMPLETARE QUI
     *
     * Obiettivi:
     * - gestire la chiusura dei semafori
     * - gestire gli errori 
     */
     int ret;
     ret= sem_destroy(&(myshm_ptr->sem_empty));
     if(ret<0) handle_error("Producer : sem_destroy error sem_empty");
     ret= sem_destroy(&(myshm_ptr->sem_filled));
     if(ret<0) handle_error("Producer : sem_destroy error sem_filled");
     ret= sem_destroy(&(myshm_ptr->sem_mem));
     if(ret<0) handle_error("Producer : sem_destroy error sem_mem");
  }

void closeMemory() {
    /** 
     * COMPLETARE QUI
     *
     * Obiettivi:
     * - chiudere la memoria condivisa
     * - chiedere al kernel di eliminare la memoria condivisa
     * - gestire gli errori 
     */
     int ret;
     
     ret= munmap(myshm_ptr, sizeof( struct shared_memory));
     if(ret<0) handle_error("Producer: munmap error");
     
     ret=close(fd_shm);
     if(ret<0) handle_error("Producer: close fd_shm error");
    

}


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

