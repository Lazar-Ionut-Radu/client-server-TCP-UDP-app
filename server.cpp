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

#include "server.hpp"
#include "utils.hpp"

// Instance of struct that holds every info about the server.
static server_t server;

void init_server(int port) {
    // Set the port.
    server.port = port;

    // Inits the TCP & UDP sockets.
    server.socket_tcp = socket(AF_INET, SOCK_STREAM, 0);
    DIE(server.socket_tcp < 0, "socket() failed");

    server.socket_udp = socket(PF_INET, SOCK_DGRAM, 0);
    DIE(server.socket_udp < 0, "socket() failed");

    // Init the sockaddr_in structures.
    memset((char *) &server.sockaddr_in_tcp, 0, sizeof(server.sockaddr_in_tcp));
    memset((char *) &server.sockaddr_in_udp, 0, sizeof(server.sockaddr_in_udp));

    server.sockaddr_in_tcp.sin_family = AF_INET;
    server.sockaddr_in_tcp.sin_port = htons(port);
    server.sockaddr_in_tcp.sin_addr.s_addr = INADDR_ANY;

    server.sockaddr_in_udp.sin_family = AF_INET;
    server.sockaddr_in_udp.sin_port = htons(port);
    server.sockaddr_in_udp.sin_addr.s_addr = INADDR_ANY;

    // Bind the 2 sockets.
    int err;
    err = bind(server.socket_tcp, (struct sockaddr *) &server.sockaddr_in_tcp, sizeof(struct sockaddr));
    DIE (err < 0, "bind() failed");

    err = bind(server.socket_udp, (struct sockaddr *) &server.sockaddr_in_udp, sizeof(struct sockaddr));
    DIE (err < 0, "bind() failed");

    // Mark the tcp socket as a socket that accepts connection requests.
    err = listen(server.socket_tcp, SOMAXCONN);
    DIE (err < 0, "listen() failed");

    // Init the fd set and add stdin, and the 2 sockets to it.
    FD_ZERO(&server.in_fd_set);
    FD_ZERO(&server.helper_fd_set);
    FD_SET(server.socket_tcp, &server.in_fd_set);
    FD_SET(server.socket_udp, &server.in_fd_set);
    FD_SET(0, &server.in_fd_set);

    server.fdmax = std::max(server.socket_tcp, server.socket_udp);
}

int get_client_idx(char *id) {
    // Loop through the clients and cmp the id.
    for (size_t i = 0; i < server.clients.size(); ++i) {
        if (strcmp(id, server.clients[i].id) == 0) {
            return i;
        }
    }
    
    // -1 for not found.
    return -1;
}

bool handle_stdin_input(char buff[BUFSIZ]) {
    // Make sure the command is valid.
    if (strncmp(buff, "exit", 4) == 0) {
        // Free / Close everything.
        FD_ZERO(&server.in_fd_set);
        FD_ZERO(&server.helper_fd_set);
        close(server.socket_tcp);
        close(server.socket_udp);
        FD_SET(server.socket_tcp, &server.in_fd_set);
        FD_SET(server.socket_udp, &server.in_fd_set);

        // Send each client a msj to close.
        for (size_t i = 0; i < server.clients.size(); ++i) {
            int err = send(server.clients[i].socket, "out", 3, 0);
            DIE(err < 0, "send() failed");

            // Close their sockets.
            close(server.clients[i].socket);
            FD_SET(server.clients[i].socket, &server.in_fd_set);
        }

        // Exit.
        return true;
    }

    return false;
}

void handle_client_input(char buff[BUFSIZ], int fd) {
    // Get the index of the client in the list.
    int client_idx = -1;
    for (size_t i = 0; i < server.clients.size(); ++i) {
        if (server.clients[i].socket == fd) {
            client_idx = i;
            break;
        }
    }
    
    DIE(client_idx == -1, "Client not found");
    
    // If the client disconnected.
    if (strncmp(buff, "disconnect", 10) == 0) {
        server.clients[client_idx].connected = false;
        
        // Print appropriate message.
        std::cout << "Client " << server.clients[client_idx].id << " disconnected.\n";
        
        close(server.clients[client_idx].socket);
        FD_CLR(server.clients[client_idx].socket, &server.in_fd_set);
        FD_CLR(server.clients[client_idx].socket, &server.helper_fd_set);

        return;
    }

    // Tokenize the input.
    std::vector<char *> cmd = tokenize(buff);

    // Get the index of the topic in the clients list.
    int topic_idx = -1;
    for (size_t i = 0; i < server.clients[client_idx].topics.size(); ++i) {
        if (strcmp(server.clients[client_idx].topics[i].name, cmd[1]) == 0) {
            topic_idx = i;
            break;
        }
    }

    // Subscribe from topic.
    if (strncmp(buff, "subscribe", 9) == 0) {
        // Verify if the client is already subbed to the topic.
        if (topic_idx != -1 && server.clients[client_idx].topics.size() > 0)
            return;

        // If it is not add it to the list of subbed topics.
        topic_t new_topic;
        strcpy(new_topic.name, cmd[1]);
        if (cmd[2][1] == '1')
            new_topic.sf = true;
        else
            new_topic.sf = false;

        server.clients[client_idx].topics.push_back(new_topic);
        return;
    }

    // Unsubscribe from topic.
    if (strncmp(buff, "unsubscribe", 11) == 0) {
        // Verify if he is subbed to the topic.
        if (topic_idx == -1)
            return;

        // Remove it if so.
        server.clients[client_idx].topics.erase(std::next(server.clients[client_idx].topics.begin(), topic_idx));
    
        return;
    }
}

void handle_tcp_input() {
    // Accept the connection.
    socklen_t len = sizeof(struct sockaddr);
    int new_socket = accept(server.socket_tcp, (struct sockaddr *) &server.sockaddr_in_tcp, &len);
    DIE(new_socket < 0, "accept() failed");

    // Deactivate neagle's algorithm
    int const c = 1;
    int err = setsockopt(new_socket, IPPROTO_TCP, c, (char *)&(c), sizeof(int));
    DIE(err != 0, "setsockopt() failed");
    
    // Receive the id of the client.
    char id[BUFSIZ];
    memset(id, 0, BUFSIZ);
    err = recv(new_socket, id, sizeof(id), 0);
    DIE (err < 0, "recv() failed");

    int client_idx = get_client_idx(id);
    // If the client in not in the client vector add it.

    if (client_idx < 0) {
        // Declare the client_t structure. and add it to the vector.
        client_t new_client;
        new_client.connected = true;
        new_client.socket = new_socket;
        strncpy(new_client.id, id, 50);
    
        server.clients.push_back(new_client);

        // Add the fd of the client to the fd set.
        FD_SET(new_client.socket, &server.in_fd_set);
        server.fdmax = std::max(new_client.socket, server.fdmax);
        
        // Print appropiate msg.
        std::cout << "New client " << id << " connected from " 
                << inet_ntoa(server.sockaddr_in_tcp.sin_addr) << ":"
                << ntohs(server.sockaddr_in_tcp.sin_port) << ".\n";

        return;
    }

    // If the client is already connected
    if (server.clients[client_idx].connected) {
        // Print appropiate msg.
        std::cout << "Client " << id << " already connected.\n";
        std::fflush(stdin);

        // Send the client a msg.
        err = send(new_socket, "used", 4, 0);
        DIE(err < 0, "send() failed");

        // Close the the fd.
        close(new_socket);

        return;
    }

    // If the client is not connected.
    if (!server.clients[client_idx].connected) {
        server.clients[client_idx].connected = true;

        // Change the socket.
        server.clients[client_idx].socket = new_socket;
        FD_SET(new_socket, &server.in_fd_set);
        server.fdmax = std::max(server.fdmax, new_socket);

        // Print appropriate msg.
        std::cout << "New client " << id << " connected from " 
                  << inet_ntoa(server.sockaddr_in_tcp.sin_addr) << ":"
                  << ntohs(server.sockaddr_in_tcp.sin_port) << ".\n";

        // Send the stored udp messages to the client.
        for (size_t i = 0; i < server.clients[client_idx].stored.size(); ++i) {
            int err = send(new_socket, server.clients[client_idx].stored[i], BUFSIZ, 0);
            DIE(err < 0, "send() failed");
        }

        // Remove everything from the stored messages.
        server.clients[client_idx].stored.clear();
    }
}

void parse_udp_message(udp_message_t *udp_message, char **message) {
    // INT
    if (udp_message->data_type == TYPE_INT) {
        int sign = udp_message->data[0];
        uint32_t integer = ntohl(*(uint32_t *)(udp_message->data + 1));

        if (sign == 1)
            integer = -integer;

        sprintf(*message, "%s:%d - %s - INT - %d",
               inet_ntoa(server.sockaddr_in_udp.sin_addr),
               ntohs(server.sockaddr_in_udp.sin_port),
               udp_message->topic, 
               integer);
    }

    // SHORT REAL
    if (udp_message->data_type == TYPE_SHORT_REAL) {
        double short_real = ntohs(*(uint16_t *)udp_message->data);

        sprintf(*message, "%s:%d - %s - SHORT_REAL - %.2f",
               inet_ntoa(server.sockaddr_in_udp.sin_addr),
               ntohs(server.sockaddr_in_udp.sin_port),
               udp_message->topic, 
               short_real / 100);
    }

    // FLOAT
    if (udp_message->data_type == TYPE_FLOAT) {
        int sign = udp_message->data[0];
        int mantisa = ntohl(*(uint32_t *)(udp_message->data + 1));
        int exponent = udp_message->data[5];

        if (sign == 1)
            mantisa = -mantisa;

        double val = (double) mantisa / pow(10, exponent);
        sprintf(*message, "%s:%d - %s - FLOAT - %.4f",
               inet_ntoa(server.sockaddr_in_udp.sin_addr),
               ntohs(server.sockaddr_in_udp.sin_port),
               udp_message->topic, 
               val);
    }

    // STRING
    if (udp_message->data_type == TYPE_STRING) {
        sprintf(*message, "%s:%d - %s - STRING - %s",
               inet_ntoa(server.sockaddr_in_udp.sin_addr),
               ntohs(server.sockaddr_in_udp.sin_port),
               udp_message->topic, 
               udp_message->data);
    }
}

void handle_udp_input() {
    // Accept the connection.
    socklen_t len = sizeof(struct sockaddr);
    char buffer[BUFSIZ];
    memset(buffer, 0, BUFSIZ);
    int err = recvfrom(server.socket_udp, buffer, BUFSIZ, 0,
                       (struct sockaddr *)&server.sockaddr_in_udp, &len);
    DIE(err < 0, "recvfrom() failed");

    // Cast the input.
    udp_message_t *udp_message = (udp_message_t *)buffer;

    // Print
    char *forward_buffer = (char *)malloc(sizeof(char) * BUFSIZ);
    DIE(!forward_buffer, "malloc() failed");

    // Parse the udp message.
    parse_udp_message(udp_message, &forward_buffer);
    
    // Itterate through the vector of clients.
    for (size_t client_idx = 0; client_idx < server.clients.size(); ++client_idx) {
        // Itterate through the client's topic list to find this one.
        for (size_t topic_idx = 0; topic_idx < server.clients[client_idx].topics.size(); ++topic_idx) {
            if (strcmp(server.clients[client_idx].topics[topic_idx].name, udp_message->topic) == 0) {
                // If the client is active forward the message to him.
                if (server.clients[client_idx].connected) {
                    int err = send(server.clients[client_idx].socket, forward_buffer, BUFSIZ, 0);
                    DIE(err < 0, "send() failed");

                }
                // Otherwise save it if the store-and-forward bit is set.
                else {
                    if (server.clients[client_idx].topics[topic_idx].sf) {
                        char *new_stuff = (char *)malloc(sizeof(char) * BUFSIZ);
                        DIE(!new_stuff, "malloc() failed");

                        strcpy(new_stuff, forward_buffer);
                        server.clients[client_idx].stored.push_back(new_stuff);
                        
                    }
                }
            }
            
        }
    }

    free(forward_buffer);
}

// Receives only one parameter, the port.
int main(int argc, char **argv) {
    int err;

    // Stop the execution if the executable is called with no params.
    DIE(argc == 1, "Insufficient params");

    // No buffering on stdout.
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    // Init the server.
    init_server(atoi(argv[1]));

    while (1) {
        // Save in the helper fdset the socket from where we receive info.
        FD_ZERO(&server.helper_fd_set);
        server.helper_fd_set = server.in_fd_set;
        err = select(server.fdmax + 1, &server.helper_fd_set, NULL, NULL, NULL);
        DIE(err < 0, "select() failed");

        for (int fd = 0; fd <= server.fdmax; ++fd) {
            if (FD_ISSET(fd, &server.helper_fd_set)) {
                if (fd == 0) { // Input from stdin.
                    // Read the command from stdin.
                    char buffer[BUFSIZ];
                    memset(buffer, 0, BUFSIZ);
                    fgets(buffer, BUFSIZ - 1, stdin);

                    // Exit if needed
                    if (handle_stdin_input(buffer))
                        return 0;
                } else if (fd == server.socket_tcp) { // New client connection
                    handle_tcp_input();
                } else if (fd == server.socket_udp) {
                    handle_udp_input();
                } else { // Input from a client.
                    // Receive the id of the client.
                    char buffer[BUFSIZ];
                    memset(buffer, 0, BUFSIZ);
                    int err = recv(fd, buffer, BUFSIZ, 0);
                    DIE (err < 0, "recv() failed");

                    handle_client_input(buffer, fd);
                }
            }
        }
    }

    return 0;
}