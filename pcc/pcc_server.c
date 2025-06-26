#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define BUFFER_SIZE 1024
#define MAX_CLIENTS 10
#define PORT 10000
#define ERROR_MSG "Error: %s. errno: %s\n"
#define OUTPUT_MSG "char '%c' : %u times\n"
#define SIZE_OF_ARR (126-32)
static int total_num_bytes[SIZE_OF_ARR] = {0};
static int num_of_clients = 0;

void sigint_handler(int signum);

int main(int argc, char *argv[]) {
    struct sockaddr_in serv_addr;
    struct sockaddr_in my_addr;
    struct sockaddr_in peer_addr;
    socklen_t addrsize = sizeof(struct sockaddr_in);
    char data_buff[BUFFER_SIZE];

    if (signal(SIGINT, sigint_handler) == SIG_ERR) {
        printf(ERROR_MSG, "Could not set SIGINT handler", strerror(errno));
        return 1;
    }

    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    memset(&serv_addr, 0, addrsize);

    serv_addr.sin_family = AF_INET;
    // INADDR_ANY = any local machine address
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(PORT);

    if (0 != bind(listenfd, (struct sockaddr *)&serv_addr, addrsize)) {
        printf(ERROR_MSG, "Could not bind to port", strerror(errno));
        return 1;
    }
    if (0 != listen(listenfd, MAX_CLIENTS)) {
        printf(ERROR_MSG, "Could not listen on port", strerror(errno));
        return 1;
    }

    /* NOW LOGIC STARTS */
    while (1) {
        // Accept a connection.
        // Can use NULL in 2nd and 3rd arguments. but we want to print the client socket details        
        int connfd = accept(listenfd, (struct sockaddr *)&peer_addr, &addrsize);
        if (connfd < 0) {
            printf(ERROR_MSG, "Accept Failed", strerror(errno));
            return 1;
        }

        num_of_clients++;
        unsigned int this_specific_counter = 0;

        getsockname(connfd, (struct sockaddr *)&my_addr, &addrsize);
        getpeername(connfd, (struct sockaddr *)&peer_addr, &addrsize);
        printf("Server says: Client connected.\n"
            "\t\tClient IP: %s Client Port: %d\n"
            "\t\tServer IP: %s Server Port: %d\n",
            inet_ntoa(peer_addr.sin_addr), ntohs(peer_addr.sin_port),
            inet_ntoa(my_addr.sin_addr), ntohs(my_addr.sin_port));
        
        // Read data from the client
        memset(data_buff, 0, sizeof(data_buff));
        ssize_t nread;
        ssize_t totalread = 0;
        while ((nread = read(connfd, data_buff, sizeof(data_buff))) > 0) {
            for (ssize_t i = 0; i < nread; ++i) {
                char c = data_buff[i];
                if (c >= 32 && c <= 126) {
                    total_num_bytes[c - 32]++;
                    this_specific_counter++;
                }
            }
        }
        if (nread < 0) {
            printf(ERROR_MSG, "Read Failed", strerror(errno));
            close(connfd);
            continue;
        }

        // Send this_specific_counter to the client using write (no loop)
        ssize_t n = write(connfd, &this_specific_counter, sizeof(this_specific_counter));
        if (n < 0) {
            printf(ERROR_MSG, "Write Failed", strerror(errno));
        }

        close(connfd);
        num_of_clients--;
    }
}

void sigint_handler(int signum) {
    int i = 0;
    for (i; i<SIZE_OF_ARR; i++) {
        printf(OUTPUT_MSG, (char)(i + 32), total_num_bytes[i]);
    }
    exit(0);
}