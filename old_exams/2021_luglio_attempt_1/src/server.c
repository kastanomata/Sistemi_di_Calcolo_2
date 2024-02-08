#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>  // htons()
#include <netinet/in.h> // struct sockaddr_in
#include <sys/socket.h>
#include <signal.h>

#include "common.h"
#include "io.h"

pid_t server_pid;
int shutting_down = 0;

void connection_handler(int socket_desc, struct sockaddr_in *client_addr);
void reverse_string(char* buf);
void signal_handler(int sig_no);

int main(int argc, char *argv[])
{
    int ret;
    int client_desc;

    // some fields are required to be filled with 0
    struct sockaddr_in server_addr = {0};
    //struct sockaddr_in client_addr = {0};

    int sockaddr_len = sizeof(struct sockaddr_in); // we will reuse it for accept()

    server_addr.sin_addr.s_addr = INADDR_ANY; // we want to accept connections from any interface
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT); // don't forget about network byte order!	
	int sk_fd;

    sk_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(sk_fd == -1) 
        handle_error("Error while initializing socket");

    /* We enable SO_REUSEADDR to quickly restart our server after a crash:
     * for more details, read about the TIME_WAIT state in the TCP protocol */
    int reuseaddr_opt = 1;
    ret = setsockopt(sk_fd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr_opt, sizeof(reuseaddr_opt));
    if(ret) handle_error("Cannot set SO_REUSEADDR option");

    /**
     * TODO:
     * Obiettivi:
     * - Inizializzare la socket per connessione lato server
     * - Gestire eventuali errori
     */
    ret = bind(sk_fd, (struct sockaddr *) &server_addr, sockaddr_len);
    if(ret == -1)
        handle_error("Error while binding socket");
    ret = listen(sk_fd, MAX_CONN_QUEUE);
    if(ret == -1)
        handle_error("Error while listening for connection on socket");


    // We set up a handler for SIGTERM and SIGINT signals
    server_pid = getpid();
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = &signal_handler;
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGINT, &action, NULL);

    printf("[Parent] Server started!\n");

    // loop to manage incoming connections forking the server process
    while (1) {
        pid_t pid;
        // accept incoming connection
        struct sockaddr_in client_addr = {0};
        client_desc = accept(sk_fd, (struct sockaddr *) &client_addr, (socklen_t *)&sockaddr_len);
        if (shutting_down) break;
        if (client_desc == -1 && errno == EINTR) continue; // check for interruption by (other) signals
        if (client_desc == -1) handle_error("Cannot open socket for incoming connection");

        printf("[Parent] Incoming connection accepted...\n");

        /**
         * TODO:
         * Obiettivi:
         * - creare un processo figlio per gestire la connessione
         * - nel processo figlio:
         *      * rilasciare le risorse che il figlio non ha bisogno di usare
         *      * invocare la funzione connection_handler
         *      * rilasciare le risorse acquisite dal figlio
         * - nel processo padre:
         *      * rilasciare le risorse che il padre non ha bisogno di usare
         *      * resettare la struttura dati necessaria per accettare nuove
         *        connessioni
         * - gestire eventuali errori, terminando l'applicazione
         */
        pid = fork();
        switch(pid) {
        case -1:
            handle_error("Error while creating child process");
            break;
        case 0: // child
            ret = close(sk_fd);
            if(ret == -1)
                handle_error("Error while closing server socket fd in child");
            connection_handler(client_desc, &client_addr);
            exit(EXIT_SUCCESS);
            break;
        default:
            ret = close(client_desc);
            if(ret == -1)
                handle_error("Error while closing child socket fd");
        }
        /***/
    }

    /**
     * TODO:
     * Obiettivi:
     * - rilasciare le risorse acquisite dal processo principale del server
     * - gestire eventuali errori, terminando l'applicazione
     */	
    ret = close(sk_fd);
    if(ret == -1)
        handle_error("Error while closing server socket fd");

    /***/

    exit(EXIT_SUCCESS);
}

void reverse_string(char* buf)
{
    int i;
    int len = strlen(buf);
    char tmp;
    for(i = 0; i < len/2; i++) {
        tmp = buf[i];
        buf[i] = buf[len-i-1];
        buf[len-i-1] = tmp;
    }
}

void connection_handler(int socket_desc, struct sockaddr_in *client_addr)
{
    int ret;

    char buf[MSG_MAX_LENGTH];

    char *quit_command = QUIT_COMMAND;
    size_t quit_command_len = strlen(quit_command);

    sprintf(buf, "Hi! I'm an echo server.\nI will send you back whatever"
            " you send me. I will stop if you send me %s :-)\n", quit_command);

    send_to_socket(socket_desc, buf);

    // echo loop
    while (1) {
        // read message from client
        ret = recv_from_socket(socket_desc, buf, MSG_MAX_LENGTH);
        if(ret == 0) handle_error("Error reading from socket");

        printf("[Child] Received request: %s\n", buf);

        if (ret == quit_command_len && !memcmp(buf, quit_command, quit_command_len)) break;

        reverse_string(buf);

        send_to_socket(socket_desc, buf);
        printf("[Child] Sent response: %s\n", buf);
    }

    printf("[Child] Connection closed");

}

void signal_handler(int sig_no) 
{
    if (getpid() == server_pid) {
        shutting_down = 1;
    }
}