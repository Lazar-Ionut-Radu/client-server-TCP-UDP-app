#ifndef SUBSCRIBER_HPP
#define SUBSCRIBER_HPP

#include <netinet/in.h>

typedef struct subscriber_t {
    char *id;
    int server_port;
    int socket;
    struct sockaddr_in addr_in;
    struct in_addr server_ip;
    fd_set in_fd_set;
    fd_set helper_fd_set;
    int fdmax;
} subscriber_t;

// Inits the data used for the subscriber.
void init_subscriber(char *id, char *server_ip ,int server_port);

// Returns true if we need to exit the program.
bool handle_stdin_cmd(std::vector<char*> cmd);

// Returns true if we need to exit the program.
bool handle_server_cmd(char buff[BUFSIZ]);

#endif // SUBSCRIBER_HPP
