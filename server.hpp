#ifndef SERVER_HPP
#define SERVER_HPP

#include <netinet/in.h>
#include <vector>

// Struct that contains information about a client.

#define TYPE_INT 0
#define TYPE_SHORT_REAL 1
#define TYPE_FLOAT 2
#define TYPE_STRING 3


#define TYPE_INT 0
#define TYPE_SHORT_REAL 1
#define TYPE_FLOAT 2
#define TYPE_STRING 3

typedef struct upd_message_t {
    char topic[50];
    uint8_t data_type;
    char data[1501];
} udp_message_t;

typedef struct topic_t {
    char name[51];
    bool sf;
} topic_t;

typedef struct client_t {
    char id[51];
    int socket;
    bool connected;
    std::vector<topic_t> topics;
    std::vector<char *> stored;
} client_t;

typedef struct server_t {
    int port;
    int socket_tcp; 
    int socket_udp;
    struct sockaddr_in sockaddr_in_tcp;
    struct sockaddr_in sockaddr_in_udp;
    fd_set in_fd_set;
    fd_set helper_fd_set;
    int fdmax;
    std::vector<client_t> clients;
} server_t;

// Inits the data used in the server.
void init_server(int port);

// Handle the input receive from standard in.
bool handle_stdin_input(char buff[BUFSIZ]);

// Handle the input received from tcp clients.
void handle_client_input(char buff[BUFSIZ], int fd);

// Handle input when a new tcp client connects.
void handle_tcp_input();

// Handle the udp message and put in it the message ptr.
void parse_udp_message(udp_message_t *udp_message, char **message);

// Handles messages received from the udp client.
void handle_udp_input();

#endif // SERVER_HPP