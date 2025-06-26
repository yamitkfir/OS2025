#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define BUFFER_SIZE 1024
#define ERROR_MSG "Error: %s. errno: %s\n"
#define INTER_ADDR "127.0.0.1"
#define OUTPUT_MSG "# of printable characters: %u\n"

int main(int argc, char *argv[]) {
    int sockfd = -1;
    char send_buff[BUFFER_SIZE];
    struct sockaddr_in my_addr;   // where we actually connected through
    struct sockaddr_in peer_addr; // where we actually connected to
    socklen_t addrsize = sizeof(struct sockaddr_in);
    size_t total_read; ssize_t bytes_read;
    char *ptr; unsigned int result;

    struct sockaddr_in serv_addr; // where we Want to get to
    int server_port;
    char* path_file;
    ssize_t bytes_sent;

    if (argc != 3) {
        printf(ERROR_MSG, "Usage: pcc_client <server_ip> <port>", "Invalid arguments");
        exit(EXIT_FAILURE);
    }
    if (inet_pton(AF_INET, argv[1], &serv_addr.sin_addr) <= 0) {
        printf(ERROR_MSG, "Invalid server IP address", strerror(errno));
        exit(EXIT_FAILURE);
    }
    server_port = atoi(argv[2]);
    path_file = argv[3];

    /* OPEN FILE */
    int fd = open(path_file, O_RDONLY);
    if (fd == -1) {
        printf(ERROR_MSG, "Could not open file", strerror(errno));
        close(pipefd[0]); // Close read end before returning
        return 0;
    }

    /* OPEN SOCKET */
    memset(send_buff, 0, sizeof(send_buff));
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf(ERROR_MSG, "Could not create socket", strerror(errno));
        return 1;
    }

    // print socket details
    getsockname(sockfd, (struct sockaddr *)&my_addr, &addrsize);
    printf("Client: socket created %s:%d\n", inet_ntoa((my_addr.sin_addr)),
            ntohs(my_addr.sin_port));

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(server_port);
    serv_addr.sin_addr.s_addr = inet_addr(INTER_ADDR); // hardcoded...
    printf("Client: connecting...\n");
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf(ERROR_MSG, "Could not connect to server", strerror(errno));
        return 1;
    }
    // print socket details again
    getsockname(sockfd, (struct sockaddr *)&my_addr, &addrsize);
    getpeername(sockfd, (struct sockaddr *)&peer_addr, &addrsize);
    printf("Client: Connected. \n"
            "\t\tSource IP: %s Source Port: %d\n"
            "\t\tTarget IP: %s Target Port: %d\n",
            inet_ntoa((my_addr.sin_addr)), ntohs(my_addr.sin_port),
            inet_ntoa((peer_addr.sin_addr)), ntohs(peer_addr.sin_port));

    /* Read from file and send to socket */
    
    while ((bytes_read = read(fd, send_buff, sizeof(send_buff))) > 0) {
        bytes_sent = 0;
        while (bytes_sent < bytes_read) {
            ssize_t sent = write(sockfd, send_buff + bytes_sent, bytes_read - bytes_sent);
            if (sent < 0) {
                printf(ERROR_MSG, "Failed to send data to server", strerror(errno));
                close(fd);
                close(sockfd);
                exit(EXIT_FAILURE);
            }
            bytes_sent += sent;
        }
    }
    if (bytes_read < 0) {
        printf(ERROR_MSG, "Failed to read from file", strerror(errno));
        close(fd);
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    shutdown(sockfd, SHUT_WR); // Optionally, shutdown the write side to signal EOF to server
    // TODO what happens without shutdown?
    close(fd);

    /* Receive result from server */
    total_read = 0;
    ptr = (char *)&result;
    while (total_read < sizeof(result)) {
        ssize_t bytes_read = read(sockfd, ptr + total_read, sizeof(result) - total_read);
        if (bytes_read <= 0) {
            printf(ERROR_MSG, "Failed to read from server", strerror(errno));
            break;
        }
        total_read += bytes_read;
    }
    
    close(sockfd);
    printf(OUTPUT_MSG, result);
    return 0;
}
