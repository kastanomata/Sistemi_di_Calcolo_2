#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h> 
#include <netinet/in.h> 
#include <sys/socket.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "common.h"

// semaforo named per gestire il grado di concorrenza nel server
sem_t* sem_connections;

void do_work(char* buf) {
    srand((int)getpid());
    sleep(1+rand()%TIME_TO_SLEEP); // attesa casuale tra 1 e TIME_TO_SLEEP+1 secondi
    sprintf(buf, SERVER_RESPONSE); // risposta standard per tutti i client
}

void connection_handler(int socket_desc, struct sockaddr_in* client_addr) {
    int ret, recv_bytes=0;

    char buf[1024];
    size_t buf_len = sizeof(buf);

    char client_name[30];
    size_t client_name_len = sizeof(client_name);

    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(client_addr->sin_addr), client_ip, INET_ADDRSTRLEN);
    uint16_t client_port = ntohs(client_addr->sin_port);

    /**
     * COMPLETARE QUI
     *
     * Obiettivi:
     * - Tramite il semaforo named, fare in modo che la richiesta venga messa in attesa
     *   se ci sono già max_connections attualmente già gestite attivamente dal server. 
     * - Non appena la connessione può essere servita, ricevere il primo messaggio dal client
     *   contenente il suo nome identificativo e salvarlo nel buffer buf.
     * - Il messaggio non ha necessariamente lunghezza predefinita, per cui limitarsi
     *   a ricevere al più la quantità massima di byte che il buffer può contenere.
     * - Gestire gli errori.
     *
     */
     
     //voglio accedere alla connessione
     printf("[CHild%d] chiede connessione...\n",getpid());
     
     ret= sem_wait(sem_connections);
     if(ret<0) handle_error("Sem_wait error");
     
     printf("[CHild%d] benissimo procedo!!!!\n",getpid());
     
    
     
     
A:		 ret= recv(socket_desc, buf + recv_bytes, buf_len,0);
		 if(ret==0) {
			 fprintf(stderr,"Connessione chiusa inaspettatamente\n");
			 _exit(EXIT_FAILURE);
		}
		 
		 if(ret<0){ //errori
			 if(errno==EINTR) // riprovare perchè interruzione prima di ricevere
				goto A;
			//errore generico
			handle_error("Recv error");
		}
		
		recv_bytes+=ret;
		

	printf("[Child%d] ,ho ricevuto dal client il mess %s \n",getpid(),buf);
	
	
    strncpy(client_name, buf, client_name_len);
    printf("Servo la connessione del client: [%s] con ip: %s e porta %i\n", client_name, client_ip, client_port);
    fflush(stdout);
    
    memset(buf,0,buf_len);

    // invoco una funzione per il processamento del buffer ricevuto
    do_work(buf);
    printf("[Child%d] devo inviare al client %s, la stringa %s \n",getpid(),client_name,buf);
    fflush(stdout);


    /**
     * COMPLETARE QUI
     *
     * Obiettivi:
     * - Inviare il messaggio di risposta contenuto in buf (una stringa di
     *   lunghezza variabile; includere il terminatore nell'invio).
     * - Liberare le "risorse" del server bloccate dal processo e chiudere
     *   tutti i descrittori rimasti aperti.   
     * - Gestire gli errori.
     *
     */
     
     int bytes_sent=0;
     int bytes_left=strlen(buf)+1;
    
     
    while(bytes_left>0){
     
		ret= send(socket_desc, buf+ bytes_sent,bytes_left,0);  // send è come write
		
		if(ret<0) { //errori
			if(errno==EINTR)
				continue;
			//errore generico
			fprintf(stderr,"[Child%d] error send error",getpid());
			handle_error("Send error");
		}
		
		bytes_sent+=ret;
		bytes_left-=ret;
		
	}
	
	printf("[Child%d] ho inviato alclient %s \n", getpid(),buf);
	
	ret=sem_post(sem_connections);
	if(ret<0) handle_error("Child error sem_post sem_connections");
	
	ret=close(socket_desc);
	if(ret<0) handle_error("Child error close client_desc");
	
	ret=sem_close(sem_connections);
	if(ret<0) handle_error("Child error sem_close sem_connections");
	
	
		
    printf("Connessione del client [%s] chiusa \n", client_name);
    fflush(stdout);
}


void multi_process_server(int server_desc) {
    int ret;
    struct sockaddr_in client_addr = {0};
    int sockaddr_len = sizeof(struct sockaddr_in);
    pid_t pid;
    
    while (1) {

        /**
         * COMPLETARE QUI
         *
         * Obiettivi:
         * - Mettere il server in ascolto sulla socket server_desc ed accettare eventuali
         *   connessioni in arrivo. Salvare l'indirizzo del client da cui proviene la 
         *   connessione nella variabile client_addr.
         * - Per ogni connessione accettata, creare un processo figlio per gestirla.
         * - Il figlio deve chiudere i descrittori non necessari e chiamare la funzione 
         *   connection_handler per gestire la connessione. Una volta gestita la connessione
         *   il figlio deve terminare.
         * - Il padre deve chiudere i descrittori non necessari e rimettersi in attesa di 
         *   altre connessioni.
         * - Gestire eventuali errori.
         *
         */
		

        int client_desc = 0; // DA MODIFICARE 

        memset(&client_addr, 0, sizeof(struct sockaddr_in));
        
        client_desc= accept(server_desc, (struct sockaddr*) &client_addr, (socklen_t*) &sockaddr_len);
        if(client_desc<0) handle_error("Multi_process_server: accept client_desc error");
        
        printf("Connessione accettata!! Creo Processo figlio.....\n");
        pid=fork();
        if(pid<0) handle_error("Multi_process_server: fork error");
        if(pid==0){ // sono nel processo figlio
			printf("Sono il Child%d \n",getpid());
			ret=close(server_desc);
			if(ret<0) handle_error("Child close server_desc error\n");
			printf("Child%d avvio la connection_handler\n",getpid());
			
			connection_handler(client_desc,&client_addr);
			
			_exit(EXIT_SUCCESS);
			
		}
		
		//sono il processo padre
		//chiudo connessione con questo client;
		
		ret=close(client_desc);
		if(ret<0) handle_error("Server: error close client_desc");	
                
        
    }
}


void initialize_socket(int* p_socket_desc, struct sockaddr_in* p_server_addr) {
    int ret = 0;

    *p_socket_desc = socket(AF_INET , SOCK_STREAM , 0);
    if (*p_socket_desc < 0) handle_error("Errore nella creazione della socket");

    p_server_addr->sin_addr.s_addr = INADDR_ANY; 
    p_server_addr->sin_family      = AF_INET;
    p_server_addr->sin_port        = htons(SERVER_PORT); 

    int reuseaddr_opt = 1;
    ret = setsockopt(*p_socket_desc, SOL_SOCKET, SO_REUSEADDR, &reuseaddr_opt, sizeof(reuseaddr_opt));
    if (ret) handle_error("Errore nell'impostare l'opzione SO_REUSEADDR");
}


int main(int argc, char* argv[]) {

    int socket_desc, ret;
    struct sockaddr_in server_addr = {0};

     /**
     * COMPLETARE QUI
     *
     * Obiettivi:
     * - Creare il semaforo named definito da SEMAPHORE_NAME in modo da poter gestire
     *   al massimo MAX_CONNECTIONS connessioni contemporaneamente.
     * - Prima di creare il semaforo, assicurarsi che non esista.
     * - Gestire gli errori.
     *
     */
	sem_unlink(SEMAPHORE_NAME);
	
	sem_connections= sem_open(SEMAPHORE_NAME,O_CREAT | O_EXCL, 0660, MAX_CONNECTIONS);
	if(sem_connections==SEM_FAILED) handle_error("Server: sem_open error");


    initialize_socket(&socket_desc, &server_addr);

    ret = bind(socket_desc, (struct sockaddr*) &server_addr, sizeof(struct sockaddr_in));
    if (ret) handle_error("Errore nella bind");

    ret = listen(socket_desc, MAX_CONN_QUEUE);
    if (ret) handle_error("Errore nella listen");

    multi_process_server(socket_desc);
    

    exit(EXIT_SUCCESS);
}
