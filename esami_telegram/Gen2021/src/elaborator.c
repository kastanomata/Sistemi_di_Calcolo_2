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


//definizione shared memory
struct shared_memory *myshm_ptr;
int fd_shm;
int ret,i;

void initMemory() {
     /**
     * COMPLETARE QUI
     *
     * Obiettivi:
     * - richiedere al kernel di creare una memoria condivisa (nome definito in common.h)
     * - configurare la sua dimensione per contenere la struttura struct shared_memory
     * - mappare la memoria condivisa nel puntatore myshm_ptr 
     * - inizializzare la memoria a 0
     * - Gestire gli errori.
     *
     */  
     int ret;
     
     //fare unlink della memoria condivisa
     shm_unlink(SH_MEM_NAME);
     
     fd_shm= shm_open(SH_MEM_NAME, O_CREAT |O_RDWR,0666);
     if(fd_shm<0) handle_error("Elaborator: shm_open error"); 
     
     ret=ftruncate(fd_shm, sizeof(struct shared_memory));
      if(ret<0) handle_error("Elaborator: ftruncate error"); 
      
      myshm_ptr= mmap(NULL,sizeof(struct shared_memory), PROT_READ | PROT_WRITE ,MAP_SHARED,fd_shm,0);
      if(myshm_ptr==MAP_FAILED) handle_error("Elaborator: mmap failed");
      
      //inizializzo memoria a 0
      memset(myshm_ptr,0,sizeof(struct shared_memory));
     
  
}

void openSemaphores() {
     /**
     * COMPLETARE QUI
     *
     * Obiettivi:
     * - inizializzare i semafori definiti in common.h e presenti all'interno della memoria condivisa
     *   ATTENZIONE: i semafori sono condivisi 
     * tra processi 
     * che non sono legati da parentela (fork)
     *   non lo abbiamo visto nelle esercitazioni, ma ho comunque detto che c'è un parametro
     *   da settare opportunamente in questo caso
     * - gestire gli errori
     */
     int ret;
     
     //USO LA SEM INIT CON FLAG 1
     // perché i sem sono condivisi tra processi che non sono legati da parentela ( no fork())
     
     ret= sem_init(&(myshm_ptr->sem_empty),1,BUFFER_SIZE);
     if(ret<0) handle_error("Elaborator: sem_init sem_empty error");
     
     ret= sem_init(&(myshm_ptr->sem_filled),1,0);
     if(ret<0) handle_error("Elaborator: sem_init sem_filled error");
     
     ret= sem_init(&(myshm_ptr->sem_mem),1,1);
     if(ret<0) handle_error("Elaborator: sem_init sem_mem error");
     
     
 }    
  

void close_everything() {
    /** 
     * COMPLETARE QUI
     *
     * Obiettivi:
     * - chiudere i semafori
     * - chiudere la memoria condivisa
     * - chiedere al kernel di eliminare la memoria condivisa
     * - gestire gli errori 
     */
     
     printf("Elaborator: chiudo i semafori....\n");
     int ret;
     ret= sem_destroy(&(myshm_ptr->sem_empty));
     if(ret<0) handle_error("Elaborator, close_everything: sem_destroy error sem_empty");
     
     ret= sem_destroy(&(myshm_ptr->sem_filled));
     if(ret<0) handle_error("Elaborator, close_everything: sem_destroy error sem_filled");
     
     ret= sem_destroy(&(myshm_ptr->sem_mem));
     if(ret<0) handle_error("Elaborator, close_everything: sem_destroy error sem_mem");
     
      printf("Elaborator: chiudo la shm mem....\n");
      ret= munmap(myshm_ptr,sizeof(struct shared_memory));
      if(ret<0) handle_error("Elaborator, close_everything: munmap error");
      
      //chiudo descrittore della shared memory normale
      ret= close(fd_shm);
      if(ret<0) handle_error("Elaborator, close_everything: close fd_shm error");
      
      ret=shm_unlink(SH_MEM_NAME);
      if(ret<0) handle_error("Elaborator, close_everything: shm_unlink error");
     
     
  
}



void consume(){
    int numOps = 0;
    int totalreward = 0;
    int ret;
    while (1) {
        printf("ready to read an element\n");fflush(stdout);

        /** 
         * COMPLETARE QUI
         *
         * L'elaborator preleva un elemento dal buffer e lo elabora (simulato con una pausa) 
         * Occasionalmente stampa quanto ha guadagnato da tutti i task
         * 
         * Obiettivi:
         * - gestire opportunamente la sezione critica tramite i semafori
         * - gestire gli errori 
         **/
         ret=sem_wait(&(myshm_ptr->sem_filled));
         if(ret<0) handle_error("Elaborator consume: sem_wait sem_filled error");
          
         ret=sem_wait(&(myshm_ptr->sem_mem));
         if(ret<0) handle_error("Elaborator consume: sem_wait sem_mem error");
         
         //-----CS----
         struct cell value= myshm_ptr->buf[myshm_ptr->read_index];
         
        
         
         myshm_ptr->read_index= (myshm_ptr->read_index +1)% BUFFER_SIZE;
         
         //esco dallaCS
         ret=sem_post(&(myshm_ptr->sem_mem));
         if(ret<0) handle_error("Elaborator consume: sem_post sem_mem error");
         
         ret=sem_post(&(myshm_ptr->sem_empty));
         if(ret<0) handle_error("Elaborator consume: sem_post sem_empty error");
         
         //elaboro il dato
         
         printf("Elaborator: elaboro il dato...  ..... ....");
         
         sleep(2);
         
         totalreward+= value.reward;
         numOps++;
         printf("Fatto !!!!\n");
          
          
          
          if(numOps%10==0)
			  printf("Elaboratore consume() ----- ho guadagnato %d per ora\n",totalreward);


    }
}

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
    printf("creating shared memory\n");fflush(stdout);
    
    initMemory();
    
    printf("opening semaphores\n");fflush(stdout);
    openSemaphores();

    consume();
    //we never reach this point
    exit(EXIT_SUCCESS);
}
