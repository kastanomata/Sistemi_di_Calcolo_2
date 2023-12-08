    #include <stdio.h>
    #include <stdlib.h>
    #include <unistd.h>
    #include <sys/wait.h>
    #include <sys/mman.h>
    #include <fcntl.h>
    #include <semaphore.h>
    #include <pthread.h>
    #include <errno.h>
    #include <string.h>

    // macros for error handling
    #include "common.h"
    #define DEBUG 0

    #define N 10   // child process count
    #define M 2    // thread per child process count
    #define T 3     // time to sleep for main process

    #define FILENAME	"accesses.log"
    #define ACCESS_TO_FILE_NAME "/cs_sem"
    #define CHILDREN_READY_SEM "/children_ready_name"
    #define START_STOP_ACTIVITY_SEM "/start_activity_name"

    #define SHARED_MEMORY_NAME "/my_shm_mem_name"
    #define SHM_NUM 1
    

    /*
    * data structure required by threads
    */
    typedef struct thread_args_s {
        unsigned int child_id;
        unsigned int thread_id;
    } thread_args_t;

    typedef struct shared_mem_util {
        int fd;
        void *ptr;
    } shared_mem_t;


    /*
    * parameters can be set also via command-line arguments
    */
    int n = N, m = M, t = T;

    /* TODO: declare as many semaphores as needed to implement
    * the intended semantics, and choose unique identifiers for
    * them (e.g., "/mysem_critical_section") */
    sem_t *child_proc_ready;
    sem_t *access_to_file;
    sem_t *activity;

    /* TODO: declare a shared memory and the data type to be placed 
    * in the shared memory, and choose a unique identifier for
    * the memory (e.g., "/myshm") 
    * Declare any global variable (file descriptor, memory 
    * pointers, etc.) needed for its management
    */
    shared_mem_t shd_m;
    
    
    void init_file(const char *filename) {
        /*
        * Ensures that an empty file with given name exists.
        */
        printf("[Main] Initializing file %s...", FILENAME);
        fflush(stdout);
        int fd = open(FILENAME, O_WRONLY | O_CREAT | O_TRUNC, 0600);
        if (fd<0) handle_error("error while initializing file");
        close(fd);
        printf("closed...file correctly initialized!!!\n");
    }

    sem_t *create_named_semaphore(const char *name, mode_t mode, unsigned int value) {
        printf("[Main] Creating named semaphore %s... ", name);
        fflush(stdout);
        sem_t *ret;
        // TODO
        /*
            * Create a named semaphore with a given name, mode and initial value.
            * Also, tries to remove any pre-existing semaphore with the same name.
        */
        sem_unlink(name);
        if((ret = sem_open(name, O_CREAT | O_EXCL, mode, value)) == SEM_FAILED)
            handle_error("Error while creating semaphore");
        printf("done!!!\n");
        return ret;
    }

    void create_shared_memory(const char *name, int size) {
        printf("[Main] Creating shared memory %s... ", name);
        shm_unlink(name);
        if((shd_m.fd = shm_open(name, O_CREAT | O_EXCL | O_RDWR, 0666)) == -1 )
            handle_error("Error! while opening shared memory\n");
        if(ftruncate(shd_m.fd, size) == -1)
            handle_error("Error! while truncating shared memory\n");
        if((shd_m.ptr = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, shd_m.fd,0)) == MAP_FAILED)
            handle_error("Error! while mapping memory to my_shd_memory\n");
        printf("done!!!\n");
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

        int max_child_id = -1, max_accesses = -1, i;
        for (i = 0; i < n; i++) {
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

        /* TODO: protect the critical section using semaphore(s) as needed */

        // open file, write child identity and close file
        int fd = open(FILENAME, O_WRONLY | O_APPEND);
        if (fd < 0) handle_error("Error! while opening file");
        if(DEBUG) printf("[Child#%d-Thread#%d] File %s opened in append mode!!!", args->child_id, args->thread_id, FILENAME);	

        write(fd, &(args->child_id), sizeof(int));
        if(DEBUG) printf("[Child#%d-Thread#%d] %d appended to file %s opened in append mode!!!", args->child_id, args->thread_id, args->child_id, FILENAME);	

        close(fd);
        if(DEBUG) printf("[Child#%d-Thread#%d] File %s closed!!!", args->child_id, args->thread_id, FILENAME);

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
        */
        if(DEBUG) puts("[Main] mainProcess waiting for its children");
        
        for(int i = 0; ;i++) {
            if(i == N) break;
            int* child_flag = (int*) (shd_m.ptr+i*sizeof(int));
            while(*child_flag != 1) {};
        }
        if(DEBUG) puts("[Main] mainProcess has verified that all the children are ready!");
        int* checkered_flag = (int*) (shd_m.ptr+(n+1)*sizeof(int));
        if(DEBUG) puts("[Main] mainProcess has signaled to the children to start in one second!");
        sleep(1);
        *checkered_flag = 1;
        sleep(0.05);
        *checkered_flag = 0;
        if(DEBUG) puts("[Main] is terminating!");

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
        if(DEBUG) printf("\t[Child%d] Child ready to start activity!\n", child_id);
        int* child_flag = (int*) (shd_m.ptr+child_id*sizeof(int));
        *child_flag = 1;
        if(DEBUG) printf("\t[Child%d] Child has raised its flag! The flag is now %d!\n", child_id, *child_flag);
        int* checkered_flag = (int*) (shd_m.ptr+(n+1)*sizeof(int));
        while(*checkered_flag == 0) {}
        if(DEBUG) printf("\t[Child%d] Child has started!\n", child_id);
        while(*checkered_flag == 1) {
           if(DEBUG) printf("\t[Child%d] Child is doing things!\n", child_id); 

        }
    
       _exit(EXIT_SUCCESS);
    }

    int main(int argc, char **argv) {
        // arguments
        if (argc > 1) n = atoi(argv[1]);
        if (argc > 2) m = atoi(argv[2]);
        if (argc > 3) t = atoi(argv[3]);

        // initialize the file
        init_file(FILENAME);
        /* TODO: initialize any semaphore needed in the implementation, and
        * create N children where the i-th child calls childProcess(i); 
        * initialize the shared memory (create it, set its size and map it in the 
        * memory), then the main process executes function mainProcess() once 
        * all the children have been created 
        */
        create_shared_memory(SHARED_MEMORY_NAME, (n+1)*sizeof(int));
        memset(shd_m.ptr, 0, (n+1)*sizeof(int));
        activity = create_named_semaphore(START_STOP_ACTIVITY_SEM, 0600, 0);
        if(DEBUG) puts("-----\nTime to create processes!");
        for(int i = 0; i<n; i++) {
            pid_t pid = fork();        
            if(pid == -1) 
                handle_error("Error! while creating child");
            if(pid == 0) {
                childProcess(i);
                _exit(EXIT_SUCCESS);
            }
        }
        if(DEBUG) puts("-----\nChildren created");
        mainProcess();
        if(shm_close(shd_m) == -1) 
            handle_error;
        shm_unlink(SHARED_MEMORY_NAME);
        exit(EXIT_SUCCESS);
    }