//
//  main.c
//  HttpServer2
//
//  Created by JuntaoWang on 3/19/15.
//  Copyright (c) 2015 JuntaoWang. All rights reserved.
//

/*
This code primarily comes from
http://www.prasannatech.net/2008/07/socket-programming-tutorial.html
and
http://www.binarii.com/files/papers/c_sockets.txt
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include "arduino.h"

int arduino_ready;

int start_server(int PORT_NUMBER)
{

    // structs to represent the server and client
    struct sockaddr_in server_addr,client_addr;

    int sock; // socket descriptor

    // 1. socket: creates a socket descriptor that you later use to make other system calls
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Socket");
        exit(1);
    }
    int temp;
    if (setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&temp,sizeof(int)) == -1) {
        perror("Setsockopt");
        exit(1);
    }

    // configure the server
    server_addr.sin_port = htons(PORT_NUMBER); // specify port number
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    bzero(&(server_addr.sin_zero),8);

    // 2. bind: use the socket and associate it with the port number
    if (bind(sock, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) == -1) {
        perror("Unable to bind");
        exit(1);
    }

    // 3. listen: indicates that we want to listn to the port to which we bound; second arg is number of allowed connections
    if (listen(sock, 5) == -1) {
        perror("Listen");
        exit(1);
    }

    // once you get here, the server is set up and about to start listening
    printf("\nServer configured to listen on port %d\n", PORT_NUMBER);
    fflush(stdout);


    // 4. accept: wait here until we get a connection on that port
    int sin_size = sizeof(struct sockaddr_in);
    int fd;

    while (1) {

        fd = accept(sock, (struct sockaddr *)&client_addr,(socklen_t *)&sin_size);
        printf("Server got a connection from (%s, %d)\n", inet_ntoa(client_addr.sin_addr),ntohs(client_addr.sin_port));

        // buffer to read data into
        char request[1024];

        // // 5. recv: read incoming message into buffer
        long bytes_received = recv(fd, request, 1024, 0);
        char choice = request[bytes_received - 1];
        if (choice != 'a' &&choice != 'b' &&choice != 'c' &&choice !='d' &&choice !='e' &&choice !='f') {
            bytes_received = recv(fd, request, 1024, 0);
            choice = request[bytes_received - 1];
        }

        // 6. send: send the message over the socket
        // note that the second argument is a char*, and the third is the number of chars

        if (arduino_ready) { //arduino connection is normal
            printf("Watch: %c\n", choice);
            send_to_arduino(choice);
            char *reply = get_watch_msg();
            printf("reply: %s\n", reply);
            int bytes = send(fd, reply, strlen(reply), 0);
            printf("bytes sent: %d\n", bytes);
        } else {
            char* reply = "{\n\"name\": \"No Arduino Connection!\"\n}\n";
            send(fd, reply, strlen(reply), 0);
        }

        // 7. close: close the socket connection
        close(fd);

    }
    //      close(sock);
    printf("Server closed connection\n");

    return 0;
}

int main(int argc, char *argv[])
{
    // check the number of arguments
    if (argc != 2)
    {
        printf("\nUsage: server [port_number]\n");
        exit(0);
    }
    arduino_ready = initialize_arduino();

    int PORT_NUMBER = atoi(argv[1]);
    start_server(PORT_NUMBER);
    close_communication();
}