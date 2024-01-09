#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "common.h"

unsigned int shared_counters[N]; // contatore delle richieste delle risorse
/**
 * TODO: 
 * 
 * Obiettivi:
 * - dichiarare i semafori necessari per garantire la mutua esclusione 
 *   nell'accesso alle celle dell'array shared_counters
 **/	 
// lo i-esimo semaforo gestisce la mutua esclusione per la risorsa i-esima
sem_t shared_res_sem[N]; 

// struttura dati con gli argomenti per thread connection_handler
typedef struct handler_args_s {
    unsigned int client_id;
    int socket_desc;
    struct sockaddr_in* client_addr;
} handler_args_t;

void semaphore_init();
void semaphore_destroy();
unsigned int process_resource(unsigned int client_id, unsigned int resource_id);
void* connection_handler(void* arg);

int main(int argc, char* argv[]) {	  	
    /**
     * TODO: 
     * Obiettivi:
     * - inizializzare i semafori 
     * - gestire eventuali errori di inizializzazione
     **/
    int ret;
    semaphore_init();
    
    // creo le strutture per la socket
    int socket_desc, client_desc;
    struct sockaddr_in server_addr = {0};
    
    int sockaddr_len = sizeof(struct sockaddr_in); // da riusare per le accept()
    
    // inizializzo la socket
    socket_desc = socket(AF_INET , SOCK_STREAM , 0);
    if(socket_desc == -1) handle_error("Impossibile creare la socket");
    
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_family      = AF_INET;
    server_addr.sin_port        = htons(SERVER_PORT);
    
    // abilito l'opzione SO_REUSEADDR per riavviare il server dopo un crash
    int reuseaddr_opt = 1;
    ret = setsockopt(socket_desc, SOL_SOCKET, SO_REUSEADDR, &reuseaddr_opt, sizeof(reuseaddr_opt));
    if(ret) handle_error("Impossibile settare l'opzione SO_REUSEADDR");
    
    // bind & listen
    ret = bind(socket_desc, (struct sockaddr*) &server_addr, sockaddr_len);
    if(ret) handle_error("Impossibile fare il binding indirizzo-socket");

    ret = listen(socket_desc, 16);
    if(ret) handle_error("Impossibile ascoltare dalla socket");

    // loop per gestire connessioni in ingresso con nuovi thread
    printf("Server pronto ad accettare connessioni!\n");
    unsigned int client_id = 0;
    while (1) { 
        
        pthread_t thread;
        // alloco un buffer client_addr per la connessione
        struct sockaddr_in* client_addr = calloc(1, sizeof(struct sockaddr_in));
    
        // accetto connessioni in ingresso
        client_desc = accept(socket_desc, (struct sockaddr*) client_addr, (socklen_t*) &sockaddr_len);
        if (client_desc == -1 && errno == EINTR) continue; // check per segnali di interruzione
        if (client_desc == -1) handle_error("Impossibile aprire la socket per connessioni in ingresso.");
        printf("Connessione accettata\n");
        /**
         * TODO: 
         * Obiettivi:
         * - preparare gli argomenti da passare al thread che eseguirà
         *   connection_handler (si veda handler_args_t all'inizio)
         * - creare un thread che esegua connection_handler() passando
         *   la struct con gli argomenti predisposta
         * - gestire eventuali errori
         * 
         * Si tenga a mente che NON verranno effettuate in futuro
         * operazioni di join sul thread appena creato.
         **/
        // typedef struct handler_args_s {
        //     unsigned int client_id;
        //     int socket_desc;
        //     struct sockaddr_in* client_addr;
        // } handler_args_t;
        handler_args_t *args = (handler_args_t *) malloc(sizeof(handler_args_t));
        args->client_id     = client_id;
        args->socket_desc   = client_desc;
        args->client_addr   = client_addr;
        ret = pthread_create(&thread, NULL, connection_handler, args);
        if(ret == -1)
            handle_error("Error while creating pthread");

        ret = pthread_detach(thread);
        if(ret == -1)
            handle_error("Error while detaching thread");
        
        // incremento il client ID dopo ogni connessione accettata
        client_id++;
    }

    semaphore_destroy();

    return 0;
}

void* connection_handler(void* arg) {
    int ret;
    
    handler_args_t* args = (handler_args_t*)arg;
    int socket_desc = args->socket_desc;
    struct sockaddr_in* client_addr = args->client_addr;
    unsigned int client_id = args->client_id;
    
    char buf[1024];
    
    char* quit_command = SERVER_COMMAND;
    size_t quit_command_len = strlen(quit_command);
    char msg_terminator = '\n';
    
    // parso il client IP address e la porta
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(client_addr->sin_addr), client_ip, INET_ADDRSTRLEN);
    uint16_t client_port = ntohs(client_addr->sin_port); // port number is an unsigned short
    printf("Client %u connesso sulla porta %u \n", client_id, client_port);
    
    // ciclo di scambio messaggi
    unsigned int resource_id;
    while (1) {
        /**
         * TODO: 
         * Obiettivi:
         * - leggere un messaggio dal client e salvarlo nel buffer buf
         * - gestire eventuali interruzioni ed errori
         * - salvare il numero di byte ricevuti nella variabile 'bytes_read'
         *   e il valore restituito da una singola lettura in 'ret' 
         * - gestire chiusure inattese della connessione da parte del
         *   client uscendo dal loop usato per la lettura
         * 
         * Si tenga a mente che l'ultimo byte valido in un messaggio (la
         * cui lunghezza NON è nota a priori) è il terminatore di riga '\n'.
         **/
        int bytes_read = 0;
        do {
            ret = recv(socket_desc, buf + bytes_read, 1, 0);
            if(ret == -1) {
                if(errno == EINTR) continue;
                else handle_error("Error while receiving msg from client");
            }
            if(ret == 0) 
                handle_error("Error while receiving msg from client; Socket unexpectedly close");
            bytes_read++;
        } while(buf[bytes_read - 1] != msg_terminator && bytes_read < sizeof(buf));
        size_t msg_len = bytes_read - 1; // NON MODIFICARE (vedi commenti sopra)
        
        // verifico se devo terminare (comando "QUIT" ricevuto, o endpoint chiuso)
        if ((msg_len == quit_command_len && !memcmp(buf, quit_command, quit_command_len)) || ret == 0) break;
        
        // avvio il processamento della risorsa, aggiorno il relativo contatore e lo salvo in buf
        buf[msg_len] = '\0';
        int tmp = atoi(buf); // devo usare un int per gestire numeri negativi
        resource_id = (tmp > 0 && tmp < N) ? tmp : 0;
        unsigned int counter_updated = process_resource(client_id, resource_id);
        
        sprintf(buf, "[risorsa %u] contatore: %u", resource_id, counter_updated);
        msg_len = strlen(buf);
        
        /**
         * TODO: 
         * 
         * Obiettivi:
         * - inviare al client i primi msg_len bytes contenuti in buf
         * - gestire eventuali interruzioni ed errori
         * - assicurarsi che tutti i byte siano stati scritti
         **/
        int bytes_sent = 0;
        do {
            ret = send(socket_desc, buf + bytes_sent, msg_len - bytes_sent, 0);
            if(ret == -1) {
                if(errno == EINTR) continue;
                else handle_error("Error while responding to client");
            }
            bytes_sent += ret;
        } while(bytes_sent < msg_len);
    }
    
    /**
     * TODO: 
     * 
     * Obiettivi:
     * - chiudere descrittori in uso
     * - gestire eventuali errori
     * - liberare memoria in uso al thread
     */
    ret = close(socket_desc);
    if(ret == -1) handle_error("Error while closing socket");
    free(args);
     
    printf("Thread connection_handler terminato\n");
    
    pthread_exit(NULL);
}

unsigned int process_resource(unsigned int client_id, unsigned int resource_id) {
    /**
     * TODO: 
     * 
     * Obiettivi:
     * - gestire la mutua esclusione tra thread che provano ad accedere
     *   alla cella 'resource_id'-esima nella sezione critica
     * - gestire eventuali errori
     **/	 
    int ret;
    ret = sem_wait(&shared_res_sem[resource_id]);
    if(ret == -1) handle_error("Error while waiting for resource");
    /* inizio sezione critica */
    printf("Risorsa %u LOCKED dal client %u! Processamento in corso...\n", resource_id, client_id);
    unsigned int counter_updated = ++shared_counters[resource_id];
    printf("Nuovo contatore per risorsa %u: %u\n", resource_id, counter_updated);
    sleep(SLEEP_TIME);
    /* fine sezione critica */
    ret = sem_post(&shared_res_sem[resource_id]);
    if(ret == -1) handle_error("Error while releasing resource");
    
    printf("Risorsa %u UNLOCKED\n", resource_id);
    
    return counter_updated;
}

void semaphore_init() {
    int ret;
    for(int i = 0; i < N; i++) {
        ret = sem_init(&shared_res_sem[i], 0, 1);
        if(ret == -1)
            handle_error("Error while initializing semaphore");
    }    
}

void semaphore_destroy() {
    int ret;
    for(int i = 0; i < N; i++) {
        ret = sem_destroy(&shared_res_sem[i]);
        if(ret == -1)
            handle_error("Error while destroying semaphore");
    }  
}