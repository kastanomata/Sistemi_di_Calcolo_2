#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>  // htons() and inet_addr()
#include <netinet/in.h> // struct sockaddr_in
#include <sys/socket.h>

#include "common.h"

int main(int argc, char* argv[]) {
    int ret,bytes_sent,recv_bytes;
    char terminator = '\n';
    // variables for handling a socket
    int socket_desc;
    struct sockaddr_in server_addr = {0}; // some fields are required to be filled with 0

    socklen_t sockaddr_len = sizeof(struct sockaddr_in);

    /**
     *  TODO: create a socket for contacting the server
     *
     * Suggestions:
     * - protocollo AF_INET
     * - tipo SOCK_STREAM
     */
    socket_desc = socket(AF_INET, SOCK_STREAM, 0);
    if(socket_desc == -1) handle_error("Error while creating socket");

    if (DEBUG) fprintf(stderr, "Socket created...\n");

    /**
     *  TODO: set up parameters for the connection and initiate a connection to the server
     *
     * Suggestions:
     * - set the 3 fields of server:
     * - - server_addr.sin_addr.s_addr: we must specify the server address
     * - - server_addr.sin_family,
     * - - server_addr.sin_port (using htons() method)
     * - initiate a connection to the server
     * - - attention to the connect method:
     * - - it requires as second field struct sockaddr* addr, 
     * -   but our address is a struct sockaddr_in, hence we must 
     * -   cast it (struct sockaddr*) &server_addr
     */
    server_addr.sin_addr.s_addr = inet_addr(SERVER_ADDRESS);
    server_addr.sin_family = AF_INET; 
    server_addr.sin_port = htons(SERVER_PORT);
    ret = connect(socket_desc, (struct sockaddr *) &server_addr, sockaddr_len);

    if (DEBUG) fprintf(stderr, "Connection established!\n");

    char buf[1024];
    size_t buf_len = sizeof(buf);
    int msg_len = 0;
    memset(buf,0,buf_len);

  	/**
     *  TODO: Receive the command
     *
     * Suggestions:
     * - recv() with flags = 0 is equivalent to read() from a descriptor
     * - store the number of received bytes in recv_bytes
     * - for sockets, we get a 0 return value only when the peer closes
     *   the connection: if there are no bytes to read and we invoke
     *   recv() we will get stuck, because the call is blocking!
     * - deal with partially sent messages (we do not know the message size)
     **/
    recv_bytes = 0;
    do {
        ret = recv(socket_desc, buf + recv_bytes, 1, 0);
        if(ret == -1) {
            if(errno == EINTR) continue;
            else handle_error("Error while receiving welcome msg from server");
        }
        recv_bytes++;
    } while(recv_bytes < sizeof(buf) -1 && buf[recv_bytes - 1] != terminator); 
    buf[recv_bytes] = '\0';

    if (DEBUG) fprintf(stderr, "Received message of %d bytes...\n",recv_bytes);

    printf("%s", buf);

    // main loop
    while (1) {
        char* quit_command = SERVER_COMMAND;
        size_t quit_command_len = strlen(quit_command);

        printf("Insert your message: ");

        /* Read a line from stdin
         *
         * fgets() reads up to sizeof(buf)-1 bytes and on success
         * returns the first argument passed to it. */
        if (fgets(buf, sizeof(buf), stdin) != (char*)buf) {
            fprintf(stderr, "Error while reading from stdin, exiting...\n");
            exit(EXIT_FAILURE);
        }

        msg_len = strlen(buf);
//        buf[--msg_len] = '\0'; // remove '\n' from the end of the message

		/**
         *  TODO: send message to server
         *
         * Suggestions:
         * - send() with flags = 0 is equivalent to write() to a descriptor
         * - the number of bytes to send is msg_len as we have discarded '\n'
         * - deal with partially sent messages
         * - message size IS NOT buf size
         */
        bytes_sent = 0;
        do {
            ret = send(socket_desc, buf + bytes_sent, msg_len - bytes_sent, 0);
            if(ret == -1) {
                if(errno == EINTR) continue;
                else handle_error("Error while sending message to server");
            } 
            bytes_sent += ret;
        } while(bytes_sent < (sizeof(buf) - 2) && buf[bytes_sent-1] != terminator);

        if (DEBUG) fprintf(stderr, "Sent message of %d bytes...\n", bytes_sent);

        /**
         *  TODO: After a quit command we won't receive any more data from
         *  the server, thus we must exit the main loop.
         *
         * Suggestions:
         * - compare the number of bytes sent with the length of the
         *   quit command that tells the server to close the connection
         *   (see SERVER_COMMAND macro in common.h)
         * - perform a byte-to-byte comparsion when required using
         *   memcmp(const void *ptr1, const void *ptr2, size_t num)
         * - exit from the cycle when there is nothing left to receive
         */
        if(strlen(buf) == quit_command_len && !memcmp(buf, quit_command, quit_command_len))
            break;

        /**
         *  TODO: read message from server
         * Suggestions:
         * - recv() with flags = 0 is equivalent to read() from a descriptor
         * - for sockets, we get a 0 return value only when the peer closes
         *   the connection: if there are no bytes to read and we invoke
         *   recv() we will get stuck, because the call is blocking!
         * - deal with partially sent messages (we do not know the message size)
         */      
        recv_bytes = 0;
        do {
            ret = recv(socket_desc, buf + recv_bytes, 1, 0);
            if(ret == -1) {
                if(errno == EINTR) continue;
                else handle_error("Error while receiving msg from server");
            }
            recv_bytes++;
        } while(recv_bytes < (sizeof(buf) - 2) && buf[recv_bytes - 1] != terminator);
        buf[recv_bytes] = '\0';        

        if (DEBUG) fprintf(stderr, "Received answer of %d bytes...\n",recv_bytes);

        printf("Server response: %s\n", buf);
    }


    /**
     *  TODO: close socket and release unused resources
     */
    ret = close(socket_desc);
    if(ret == -1) handle_error("Error while closing socket_desc");

    if (DEBUG) fprintf(stderr, "Socket closed...\n");

    if (DEBUG) fprintf(stderr, "Exiting...\n");

    exit(EXIT_SUCCESS);
}
