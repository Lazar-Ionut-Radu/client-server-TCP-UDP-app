#include <iostream>
#include <vector>
#include <cmath>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <vector>

#include "subscriber.hpp"
#include "utils.hpp"

// Instance of the struct that holds info about the subscriber.
static subscriber_t subscriber;

void init_subscriber(char *id, char *server_ip ,int server_port) {
    // Set the id of the client.
    subscriber.id = (char *)malloc(sizeof(char) * strlen(id));
    DIE(!subscriber.id, "malloc() failed");
    strcpy(subscriber.id, id);

    // Set the server port.
    subscriber.server_port = server_port;

    // Init the socket.
    subscriber.socket = socket(AF_INET, SOCK_STREAM, 0);
    DIE(subscriber.socket < 0, "socket() failed");

    // Init the sockaddr_in structure.
    memset((void *) &subscriber.addr_in, 0, sizeof(struct sockaddr_in));
    subscriber.addr_in.sin_family = AF_INET;
    subscriber.addr_in.sin_port = htons(server_port);
    int err = inet_aton(server_ip, &subscriber.addr_in.sin_addr);
    DIE(err == 0, "inet_aton() failed");

    // Connect the client to the server.
    err = connect(subscriber.socket, (struct sockaddr *) &subscriber.addr_in, sizeof(sockaddr));
    DIE(err < 0, "connect() failed");
    
    // Send the id to the server.
    err = send(subscriber.socket, subscriber.id, strlen(subscriber.id), MSG_NOSIGNAL);
    DIE (err < 0, "send() failed");

    // Init the fd sets and add stdin.
    FD_ZERO(&subscriber.in_fd_set);
    FD_ZERO(&subscriber.helper_fd_set);
    FD_SET(0, &subscriber.in_fd_set);
    FD_SET(subscriber.socket, &subscriber.in_fd_set);

    subscriber.fdmax = subscriber.socket;
}

bool handle_stdin_cmd(std::vector<char*> cmd, char buff[BUFSIZ]) {
    // Exit command.
    if (strncmp(cmd[0], "exit", 4) == 0 && cmd.size() == 1) {
        // Update the server we disconnected.
        int err = send(subscriber.socket, "disconnect", 10, MSG_NOSIGNAL);
        DIE(err < 0, "send() failed");

        // Close the socket to the server.
        close(subscriber.socket);

        return true;
    }

    // Subscribe command.
    if (strncmp(cmd[0], "subscribe", 9) == 0 && cmd.size() == 3) {
        // Send message to server.
        int err = send(subscriber.socket, buff, strlen(buff), MSG_NOSIGNAL);
        DIE(err < 0, "send() failed");

        // Print appropriate msg.
        std::cout << "Subscribed to topic.\n";
    
        return false;
    }

    // Unsubscribe command.
    if (strncmp(cmd[0], "unsubscribe", 11) == 0 && cmd.size() == 2) {
        // Send message to server
        int err = send(subscriber.socket, buff, strlen(buff), MSG_NOSIGNAL);
        DIE(err < 0, "send() failed");

        // Print appropriate msg.
        std::cout << "Unsubscribed to topic.\n";

        return false;
    }

    return false;
}

// Return true if we need to exit the program. 
bool handle_server_cmd(char buff[BUFSIZ]) {
    // The server closed, close this client too.
    if (strncmp(buff, "out", 3) == 0) {
        // Close the socket.
        close(subscriber.socket);
    
        return true;
    }

    // This client is already connected.
    if (strncmp(buff, "used", 4) == 0) {
        // Close the socket.
        close(subscriber.socket);

        return true;
    }

    // Print the other messages from the server.
    std::cout << buff << std::endl;

    return false;
}

// Receives 3 parameters:
//     -> the id of the client (string)
//     -> the ip of the server (dotted-decimal)
//     -> the port of the server
int main(int argc, char **argv) {
    // Stop if the executable is called with not enough params
    DIE(argc < 4, "Insufficient params");
    
    // No buffering on stdout.
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    // Init the subscriber.
    init_subscriber(argv[1], argv[2], atoi(argv[3]));

    while (true) {
        subscriber.helper_fd_set = subscriber.in_fd_set;
        int err = select(subscriber.fdmax + 1, &subscriber.helper_fd_set, NULL, NULL, NULL);
        DIE(err < 0, "select() failed()");

        // Receive command from stdin.
        if (FD_ISSET(0, &subscriber.helper_fd_set)) {
            // Read the command and tokenize it.
            char buffer[BUFSIZ];
            fgets(buffer, BUFSIZ - 1, stdin);
            char buffer_cpy[BUFSIZ];
            strcpy(buffer_cpy, buffer);

            std::vector<char*> cmd = tokenize(buffer);
        
            // Exit if needed.
            if (handle_stdin_cmd(cmd, buffer_cpy))
                return 0;
        
        } else { // Receive commands from the server.
            char buffer[BUFSIZ];
            err = recv(subscriber.socket, buffer, BUFSIZ, 0);
            DIE(err < 0, "recv() failed");

            // Close this client if needed.
            if (handle_server_cmd(buffer))
                return 0;
        }

    }

    return 0;
}