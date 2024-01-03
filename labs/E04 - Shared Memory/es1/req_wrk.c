#include "common.h"

#include <stdio.h>
#include <stdlib.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <pthread.h>


#define CS_SEM_NAME "/cs_sem"
#define WORKER_SEM_NAME "/worker_sem"
#define REQUEST_SEM_NAME "/request_sem"
// data array
int * data;
/** COMPLETE THE FOLLOWING CODE BLOCK
*
* Add any needed resource 
**/
int file_descriptor;
sem_t * cs_sem;
sem_t * worker_sem;
sem_t * request_sem;



int request() {
  /** COMPLETE THE FOLLOWING CODE BLOCK
  *
  * map the shared memory in the data array
  **/
  data = mmap(0, SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, file_descriptor, 0);
  if(data == MAP_FAILED) handle_error("Error while mapping shared memory in request()\n"); 
  printf("request: mapped address: %p\n", data);

  if(sem_wait(cs_sem) == -1) 
    handle_error("Error while entering CS - request()\n");

  int i;
  for (i = 0; i < NUM; ++i) {
    data[i] = i;
  }
  printf("request: data generated\n");

  if(sem_post(worker_sem) == -1) // ci sono dati da elaborare
    handle_error("Error while signaling todo data\n");  
  if(sem_post(cs_sem) == -1) // nessuno sta elaborando dati
    handle_error("Error while exiting CS - request()\n");
  /** COMPLETE THE FOLLOWING CODE BLOCK
    *
    * Signal the worker that it can start the elaboration
    * and wait it has terminated
  **/
  if(sem_wait(request_sem) == -1)
    handle_error("Error while checking to print data\n");  
  if(sem_wait(cs_sem) == -1) 
    handle_error("Error while entering CS - request()\n");
  printf("request: acquire updated data\n");

  printf("request: updated data:\n");
  for (i = 0; i < NUM; ++i) {
    printf("%d\n", data[i]);
  }
  if(sem_post(cs_sem) == -1) // nessuno sta elaborando dati
    handle_error("Error while exiting CS - request()\n");

   /** COMPLETE THE FOLLOWING CODE BLOCK
    *
    * Release resources
    **/
  munmap(data,SIZE);

  return EXIT_SUCCESS;
}

int work() {

  /** COMPLETE THE FOLLOWING CODE BLOCK
  *
  * map the shared memory in the data array
  **/
  data = mmap(0, SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, file_descriptor, 0);
  if(data == MAP_FAILED) handle_error("Error while mapping shared memory in work()\n");
  printf("worker: mapped address: %p\n", data);

  /** COMPLETE THE FOLLOWING CODE BLOCK
    *
    * Wait that the request() process generated data
  **/
  printf("worker: waiting initial data\n");

  if(sem_wait(worker_sem) == -1) 
    handle_error("Error while waiting for work\n");
  if(sem_wait(cs_sem) == -1) 
    handle_error("Error while entering CS - work()\n");

  printf("worker: initial data acquired\n");

  printf("worker: update data\n");
  int i;
  for (i = 0; i < NUM; ++i) {
    data[i] = data[i] * data[i];
  }
  if(sem_post(request_sem) == -1) // ci sono dati da stampare
    handle_error("Error while signaling to print data\n");  

  printf("worker: release updated data\n");
  if(sem_post(cs_sem) == -1) // nessuno sta elaborando dati
    handle_error("Error while exiting CS - work()\n");

   /** COMPLETE THE FOLLOWING CODE BLOCK
    *
    * Signal the requester that elaboration terminated
    **/


  /** COMPLETE THE FOLLOWING CODE BLOCK
   *
   * Release resources
   **/

  return EXIT_SUCCESS;
}



int main(int argc, char **argv){

   /** COMPLETE THE FOLLOWING CODE BLOCK
    *
    * Create and open the needed resources 
    **/
    if(shm_unlink(SHM_NAME) == -1 || sem_unlink(CS_SEM_NAME) == -1 || sem_unlink(WORKER_SEM_NAME) == -1 || sem_unlink(REQUEST_SEM_NAME) == -1) 
      handle_error("Error while unlinking old stuff\n");

    
    if((file_descriptor = shm_open(SHM_NAME, O_CREAT | O_EXCL | O_RDWR, 0666)) == -1)
      handle_error("Error while opening shared memory\n");
    if(ftruncate(file_descriptor, SIZE) == -1) 
      handle_error("Error while truncating shared memory\n");

    cs_sem = sem_open(CS_SEM_NAME, O_CREAT | O_EXCL, 0600, 1);
    if(cs_sem == SEM_FAILED) 
      handle_error("Error while opening semaphore CS_SEM\n");
    worker_sem = sem_open(WORKER_SEM_NAME, O_CREAT | O_EXCL, 0600, 0);
    if(worker_sem == SEM_FAILED) 
      handle_error("Error while opening semaphore worker_sem\n");
    request_sem = sem_open(REQUEST_SEM_NAME, O_CREAT | O_EXCL, 0600, 0);
    if(request_sem == SEM_FAILED) 
      handle_error("Error while opening semaphore request_sem\n");

    int ret;
    pid_t pid = fork();
    if (pid == -1) {
        handle_error("main: fork");
    } else if (pid == 0) {
        work();
        _exit(EXIT_SUCCESS);
    }

    request();
    int status;
    ret = wait(&status);
    if (ret == -1)
      handle_error("main: wait");
    if (WEXITSTATUS(status)) handle_error_en(WEXITSTATUS(status), "request() crashed");


   /** COMPLETE THE FOLLOWING CODE BLOCK
    *
    * Close and release resources
    **/
    if(close(file_descriptor) == -1) 
      handle_error("Error while closing shared memory\n");
    if(shm_unlink(SHM_NAME) == -1) 
      handle_error("Error while unlinking shared memory\n");
    if(sem_close(cs_sem) == -1 || sem_unlink(CS_SEM_NAME) == -1)
      handle_error("Error while closing cs_sem\n");
    if(sem_close(worker_sem) == -1 || sem_unlink(WORKER_SEM_NAME) == -1)
      handle_error("Error while closing worker_sem\n");
    if(sem_close(request_sem) == -1 || sem_unlink(REQUEST_SEM_NAME) == -1)
      handle_error("Error while closing request_sem\n");
    _exit(EXIT_SUCCESS);

}
