#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <pthread.h>


// data array
int * data;
/** 
 * DONE: Add any needed resource 
**/
int shm_fd;
sem_t *sem_req;
sem_t *sem_wrk;

int request() {
    int ret;
    /** 
     * DONE: map the shared memory in the data array
     **/
    data = mmap(NULL, SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if(data == MAP_FAILED) handle_error("Error while mapping char *data to shared memory");
    printf("request: mapped address: %p\n", data);

    int i;
    for (i = 0; i < NUM; ++i) {
        data[i] = i;
    }
    printf("request: data generated\n");

    /** 
     * DONE: Signal the worker that it can start the elaboration and wait it has terminated
     **/
    ret = sem_post(sem_wrk);
    if(ret == -1) handle_error("Error while raising worker_sem");
    printf("request: acquire updated data\n");
    ret = sem_wait(sem_req);
    if(ret == -1) handle_error("Error while waiting for request_sem");
    
    printf("request: updated data:\n");
    for (i = 0; i < NUM; ++i) {
        switch(i) {
            case(NUM-1):
                printf("%d.\n", data[i]);
                break;
            default:
                printf("%d, ", data[i]);
                break;
        }
    }
    /**
     * DONE: Release resources
     **/
    ret = munmap(data, SIZE);
    if(ret == -1) handle_error("Error while unmapping char *data from shared memory");
    return EXIT_SUCCESS;
}

int work() {
    int ret;
    /**
     * DONE: map the shared memory in the data array 
     **/
    data = mmap(NULL, SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if(data == MAP_FAILED) handle_error("Error while mapping char *data to shared memory");
    printf("worker: mapped address: %p\n", data);
    /**
     * DONE: Wait that the request() process generated data
     **/
    ret = sem_wait(sem_wrk);
    if(ret == -1) handle_error("Error while waiting for worker_sem");
    printf("worker: waiting initial data\n");
    printf("worker: initial data acquired\n");
    printf("worker: update data\n");
    int i;
    for (i = 0; i < NUM; ++i) {
        data[i] = data[i] * data[i];
    }
    printf("worker: release updated data\n");
    /**
     * DONE: Signal the requester that elaboration terminated
     **/
    ret = sem_post(sem_req);
    if(ret == -1) handle_error("Error while raising req_sem");
    /**
     * DONE: Release resources: close shared memory and semaphore
     **/
    ret = close(shm_fd);
    if(ret == -1) handle_error("Error while closing shared memory");
    ret = sem_close(sem_req);
    if(ret == -1) handle_error("Error while closing request_sem");
    ret = sem_close(sem_wrk);
    if(ret == -1) handle_error("Error while closing worker_sem");
    return EXIT_SUCCESS;
}

int main(int argc, char **argv) {
    int ret;
    /** 
     * DONE: Create and open the needed resources
     * -> sem_req
     * -> sem_wrk
     * -> shared memory 
    **/
    sem_req = sem_open(SEM_NAME_REQ, O_CREAT | O_EXCL, 0600, 0);
    if(sem_req == SEM_FAILED) handle_error("Error while opening named semaphore!");
    sem_wrk = sem_open(SEM_NAME_WRK, O_CREAT | O_EXCL, 0600, 0);
    if(sem_wrk == SEM_FAILED) handle_error("Error while opening named semaphore!");
    shm_fd = shm_open(SHM_NAME, O_CREAT | O_EXCL | O_RDWR, 0600);
    if(shm_fd == -1) handle_error("Error while opening shared memory");
    ret = ftruncate(shm_fd, SIZE);
    if(ret == -1) handle_error("Error while truncating shared memory");

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


    /** 
     * DONE: Close and release resources: close shared memory and semaphores, unlink both
     **/
    ret = close(shm_fd);
    if(ret == -1) handle_error("Error while closing shared_memory"); 
    ret = shm_unlink(SHM_NAME);
    if(ret == -1) handle_error("Error while unlinking shared_memory");
    ret = sem_close(sem_req);
    if(ret == -1) handle_error("Error while closing request_sem");
    ret = sem_unlink(SEM_NAME_REQ);
    if(ret == -1) handle_error("Error while unlinking request_sem");
    ret = sem_close(sem_wrk);
    if(ret == -1) handle_error("Error while closing worker_sem");
    ret = sem_unlink(SEM_NAME_WRK);
    if(ret == -1) handle_error("Error while unlinking worker_sem");
   
    _exit(EXIT_SUCCESS);

}
