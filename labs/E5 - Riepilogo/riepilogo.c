#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/wait.h>
#include <errno.h>
#include <string.h>

//SHARED MEMORY
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
// SEMAPHORES
#include <semaphore.h>
// ERROR HANDLING
#include "common.h"

#define N 100   // child process count
#define M 10    // thread per child process count
#define T 3     // time to sleep for main process
#define DEBUG 0

#define FILENAME	"accesses.log"

/*
 * data structure required by threads
 */
typedef struct thread_args_s {
    unsigned int child_id;
    unsigned int thread_id;
} thread_args_t;

/*
 * parameters can be set also via command-line arguments
 */
int n = N, m = M, t = T;
int debug = DEBUG;

/* TODO: declare as many semaphores as needed to implement
 * the intended semantics, and choose unique identifiers for
 * them (e.g., "/mysem_critical_section") 
 * */
#define SEM_NAME    "/access_to_file_semaphore"
sem_t *file_access;

/* TODO: declare a shared memory and the data type to be placed 
 * in the shared memory, and choose a unique identifier for
 * the memory (e.g., "/myshm") 
 * Declare any global variable (file descriptor, memory 
 * pointers, etc.) needed for its management
 * */
#define SHMEM_NAME "/end_activity_shmem"
#define SHMEM_SIZE 2*sizeof(int)
int shared_memory_fd;
int *shared_memory_ptr;

/*
 * Ensures that an empty file with given name exists.
 * */
void init_file(const char *filename) {
    if(debug) printf("[Main] Initializing file %s...", FILENAME);
    fflush(stdout);
    int fd = open(FILENAME, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd<0) handle_error("error while initializing file");
    close(fd);
    if(debug) printf("closed...file correctly initialized!!!\n");
}

void parseOutput() {
    // identify the child that accessed the file most times
    int* access_stats = calloc(n, sizeof(int)); // initialized with zeros
    printf("[Main] Opening file %s in read-only mode...", FILENAME);
	fflush(stdout);
    int fd = open(FILENAME, O_RDONLY);
    if (fd < 0) handle_error("error while opening output file");
    printf("ok, reading it and updating access stats...");
	fflush(stdout);

    size_t read_bytes;
    int index;
    do {
        read_bytes = read(fd, &index, sizeof(int));
        if (read_bytes > 0)
            access_stats[index]++;
    } while(read_bytes > 0);
    printf("ok, closing it...");
	fflush(stdout);

    close(fd);
    printf("closed!!!\n");

    int max_child_id = -1, max_accesses = -1;
    for (int i = 0; i < n; i++) {
        printf("[Main] Child %d accessed file %s %d times\n", i, FILENAME, access_stats[i]);
        if (access_stats[i] > max_accesses) {
            max_accesses = access_stats[i];
            max_child_id = i;
        }
    }
    printf("[Main] ===> The process that accessed the file most often is %d (%d accesses)\n", max_child_id, max_accesses);
    free(access_stats);
}

void* thread_function(void* x) {
    thread_args_t *args = (thread_args_t*)x;
    if(debug) printf("\t\t\t[Child%d] [Thread%d] Inside of thread_function\n", args->thread_id, args->thread_id);
    /* TODO: protect the critical section using semaphore(s) as needed */ 
    if(sem_wait(file_access) == -1)
        handle_error_en(errno, "Error! while entering critical section");   
    // open file, write child identity and close file
    int fd = open(FILENAME, O_WRONLY | O_APPEND);
    if (fd < 0) handle_error("error while opening file");
    if(debug) printf("[Child#%d-Thread#%d] File %s opened in append mode!!!\n", args->child_id, args->thread_id, FILENAME);	

    write(fd, &(args->child_id), sizeof(int));
    if(debug) printf("[Child#%d-Thread#%d] %d appended to file %s opened in append mode!!!\n", args->child_id, args->thread_id, args->child_id, FILENAME);	

    close(fd);
    if(debug) printf("[Child#%d-Thread#%d] File %s closed!!!\n", args->child_id, args->thread_id, FILENAME);
    if(sem_post(file_access) == -1)
        handle_error_en(errno, "Error! while exiting critical section");   
    free(x);
    pthread_exit(NULL);
}

void mainProcess() {
    /* TODO: the main process waits for all the children to start,
     * it notifies them to start their activities, and sleeps
     * for some time t. Once it wakes up, it notifies the children
     * to end their activities, and waits for their termination.
     * Finally, it calls the parseOutput() method and releases
     * any shared resources. 
     * */
    if(debug) printf("[Main] Inside of mainProcess\n");
    
    if(debug) printf("[Main] Signaling to children start\n");
    *shared_memory_ptr = 1;

    sleep(t);

    if(debug) printf("[Main] Signaling to children stop\n");
    *shared_memory_ptr = 0;
    
}

void childProcess(int child_id) {
    /* TODO: each child process notifies the main process that it
     * is ready, then waits to be notified from the main in order
     * to start. As long as the main process does not notify a
     * termination event [hint: use sem_getvalue() here], the child
     * process repeatedly creates m threads that execute function
     * thread_function() and waits for their completion. When a
     * notification has arrived, the child process notifies the main
     * process that it is about to terminate, and releases any
     * shared resources before exiting. */
    if(debug) printf("\t[childProcess%d] This is childProcess\n", child_id);
    shared_memory_fd = shm_open(SHMEM_NAME, O_RDWR, 0666);
    if(shared_memory_fd == -1)
        handle_error_en(errno, "Error! while opening shared memory");
    shared_memory_ptr = (int*) mmap(SHMEM_NAME, SHMEM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shared_memory_fd, 0);
    if(shared_memory_ptr == NULL)
        handle_error_en(errno, "Error! while mapping pointer to shared memory");
    
    pthread_t threads[m];
    while(*shared_memory_ptr == 0) {}
    for(int b = 0; *shared_memory_ptr == 1; b++) {
        if(debug) printf("\t\t[childProcess%d][Burst%d] Starting burst of threads\n", child_id, b);
        for(int i = 0; i < m; i++) {
            thread_args_t *args = (thread_args_t*) malloc(sizeof(thread_args_t));
            if(args == NULL) 
                handle_error_en(errno, "Error! while allocating memory for thread function arguments");
            args->child_id = child_id;
            args->thread_id = i;
            if(pthread_create(&threads[i], NULL, thread_function, args) == -1)
                handle_error_en(errno, "Error! while creating thread");
        }
        for(int i = 0; i < m ; i++) {
            if(pthread_join(threads[i], NULL) == -1)
                handle_error_en(errno, "Error! while waiting for threads to die");
        }
        if(debug) printf("\t\t[childProcess%d][Burst%d]  Child process has correctly waited for all the generated_threads to die, end of burst\n", child_id, b);
    }
    if(munmap(shared_memory_ptr, SHMEM_SIZE) == -1)
        handle_error_en(errno, "Error! while unmapping shared memory");
    if(close(shared_memory_fd) == -1)
        handle_error_en(errno, "Error! while closing shared memory");
    
}

int main(int argc, char **argv) {
    // arguments
    if (argc > 1) debug = atoi(argv[1]);
    if(debug != 0 && debug != 1) 
        handle_error("Error! debug value must be 1 (for debugging) or 0");
    if (argc > 2) n = atoi(argv[2]);
    if (argc > 3) m = atoi(argv[3]);
    if (argc > 4) t = atoi(argv[4]);
    

    // initialize the file
    init_file(FILENAME);

    /* TODO: initialize any semaphore needed in the implementation, and
     * create N children where the i-th child calls childProcess(i); 
     * initialize the shared memory (create it, set its size and map it in the 
     * memory), then the main process executes function mainProcess() once 
     * all the children have been created */

    // INITIALIZE SEMAPHORE
    sem_unlink(SEM_NAME);
    file_access = sem_open(SEM_NAME, O_CREAT | O_EXCL, 0600, 1);
    if(file_access == SEM_FAILED)
        handle_error_en(errno, "Error! while opening critical section semaphore");

    // INITIALIZE SHARED MEMORY
    shm_unlink(SHMEM_NAME);
    shared_memory_fd = shm_open(SHMEM_NAME, O_CREAT | O_EXCL | O_RDWR, 0666);
    if(shared_memory_fd == -1)
        handle_error_en(errno, "Error! while opening shared memory");
    if(ftruncate(shared_memory_fd, SHMEM_SIZE) == -1) 
        handle_error_en(errno, "Error! while truncating shared memory");
    shared_memory_ptr = (int*) mmap(SHMEM_NAME, SHMEM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shared_memory_fd, 0);
    if(shared_memory_ptr == NULL)
        handle_error_en(errno, "Error! while mapping pointer to shared memory");

    // CREATE CHILD_PROCESSES    
    pid_t children_pid[n];
    for(int i = 0; i<n; i++) {
        pid_t pid = fork();
        if(pid == -1) 
            handle_error_en(errno, "Error! while forking children");
        if(pid == 0) {
            childProcess(i);
            exit(EXIT_SUCCESS);
        } 
        children_pid[i] = pid;
    }

    mainProcess();

    for(int i = 0; i < n; i++) {
        if(waitpid(children_pid[i], NULL, 0) == -1)
            handle_error_en(errno, "Error! while waiting for children to die");
    }

    if(debug) printf("[Main] All child_processes have died\n");

    // CLOSE + UNLINK SEMAPHORE
    if(sem_close(file_access) == -1)
        handle_error_en(errno, "Error! while closing critical section semaphore");
    if(sem_unlink(SEM_NAME) == -1)
        handle_error_en(errno, "Error! while unlinking critical section semaphore");
    // UNMPAP + CLOSE + UNLINK SHARED MEMORY
    if(munmap(shared_memory_ptr, SHMEM_SIZE) == -1)
        handle_error_en(errno, "Error! while unmapping shared memory");
    if(close(shared_memory_fd) == -1)
        handle_error_en(errno, "Error! while closing shared memory");
    if(shm_unlink(SHMEM_NAME) == -1)
        handle_error_en(errno, "Error! while unlinking shared memory");
    parseOutput();
    exit(EXIT_SUCCESS);
}