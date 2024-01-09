#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>

#include "common.h"
#include "io.h"

int send_to_socket(int sd, const char *msg) 
{
    int bytes_sent = 0;
    int msg_len = strlen(msg) + 1; // we account for the terminating null byte
    int ret = 0;
    /**
     * TODO:
     * Obiettivi:
     * - usare il descrittore sd per inviare tutti i byte della stringa msg
     *   (incluso il terminatore di stringa \0)
     * - come valore di ritorno, restituire il numero di byte inviati (incluso
     *   il terminatore di stringa \0)
     * - gestire eventuali interruzioni, riprendendo l'invio da dove era stato
     *   interrotto
     * - gestire eventuali errori, terminando l'applicazione
     *
     */
    do {
        ret = send(sd, msg + bytes_sent, msg_len - bytes_sent, 0);
        if(ret == -1) {
            if(errno == EINTR)
                continue;
            else    
                handle_error("Error while sending message");
        }
        bytes_sent += ret;
    } while(bytes_sent < msg_len);
    printf("Bytes sent: %d\n", bytes_sent);
    /***/
    return bytes_sent;

}

int recv_from_socket(int sd, char *msg, int max_len) 
{
    // "Hello!/0" -> 7 bytes, max_len 1024, bytes_recv = 7, return 6
    int ret;
    char terminator = '\0';
    int bytes_recv = 0;

    /**
     * TODO:
     * Obiettivi:
     * - usare il descrittore sd per ricevere una stringa da salvare nel buffer
     *   msg (incluso il terminatore di stringa \0); la lunghezza della stringa
     *   non è nota a priori, ma comunque minore di max_len;
     * - ricevere fino ad un massimo di max_len bytes (incluso il terminatore di
     *   stringa \0);
     * - come valore di ritorno, restituire il numero di byte ricevuti (escluso
     *   il terminatore di stringa \0)
     * - in caso di chiusura inaspettata della socket, restituire 0
     * - gestire eventuali interruzioni, riprendendo la ricezione da dove era
     *   stata interrotta
     * - gestire eventuali errori, terminando l'applicazione
     */
    do {   
        ret = recv(sd, msg + bytes_recv, sizeof(char), 0);
        if(ret == -1) {
            if(errno == EINTR)
                continue;
            else    
                handle_error("Error while receiving message");
        } else if(ret == 0) {
            bytes_recv = 1;
            break;
        }
        bytes_recv++;
    } while(bytes_recv <= max_len && msg[bytes_recv - 1] != terminator);
    printf("Bytes received: %d\n", bytes_recv - 1);

    return bytes_recv -1 ;

    /***/

}
