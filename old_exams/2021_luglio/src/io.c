#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>

#include "common.h"
#include "io.h"

int send_to_socket(int sd, const char *msg) {

    /**
     * COMPLETARE QUI
     *
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
     int bytes_left= strlen(msg) +1;
     int bytes_sent=0,ret;
     
     while(bytes_left>0){
		 
		 ret= send(sd, msg + bytes_sent , bytes_left,0); //sto facendo una write perchè metto flag 0;
		 
		 if(ret==-1){//errori
			 if(errno==EINTR)
				continue; // riprovare perché interruzione prima di iniziare a inviare
			//errori generici
			handle_error("Io: send error");
		}
		
		bytes_left-=ret;
		bytes_sent+=ret;
	}


    /***/
    return bytes_sent ; /*return inserito per permettere la compilazione. Rimuovere*/

}

int recv_from_socket(int sd, char *msg, int max_len) {

    /**
     * COMPLETARE QUI
     *
     * Obiettivi:
     * - ++usare il descrittore sd per ricevere una stringa da salvare nel buffer
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
     *
     */
	int recv_bytes=0,ret;
	
	do{
		ret= recv(sd, msg + recv_bytes , sizeof(char),0) ; //la recv è come la read perché alla recv ho messo flag 0
		if(ret==0){
			fprintf(stderr,"Io: recv: la socket è stata chiusa inaspettatamente\n");
			recv_bytes=0;
			break;
		}
		
		if(ret<0){//errori come sempre
			if(errno==EINTR)
				continue; //si rifa da capo perchè interruzione prima di iniziare a leggere
			//errori generici oppure
			handle_error("Io: recv error");
		}
		
		recv_bytes+=ret;
		
		
		
	}while(msg[recv_bytes-1]!='\0' && recv_bytes<=max_len);

    /***/
    return recv_bytes; 
}
