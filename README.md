
# Client-Server TCP-UDP Application message management.

This project is a homework for the communication protocols course at University Politehnica, Bucharest. 

## Description

This project contains 3 main parts:
    
    - a server: which creates a link between UDP clients and TCP clients

    - UDP clients: they connect to the server and publish messages inside different topics (managed by the server).

    - TCP clients: they connect to the server and (un)subscribe to different topics. When a message is posted to that topic, the message is sent to them.

### Server

Implemented in the server.cpp & server.cpp files they link the UDP & TCP clients using sockets. The neccesary information for a server is stored in the structure:

```c++
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
```

which includes file descriptors for each client, a of clients, and the port of the server. These are all initialised inside
```c++
void init_server(int port);
```

The server accept only one type of command from the standard input, "exit", which sends messages to the clients to close themselves, dealloc's everything and closes the server. 
When a message is received from a UDP client it is forwarded to each TCP client.
When the client receives a message from a TCP client it updates their state (connected/disconnected, subscribed to different topics). These are achieved inside:

```c++
bool handle_stdin_input(char buff[BUFSIZ]);

void handle_client_input(char buff[BUFSIZ], int fd);

void handle_udp_input();
```

When a TCP client tries to connect twice (with the same id) to the server, the latter receives a message from the server so that it will be closed.

### TCP client

Are stored inside the client using the following struct:

```c++
typedef struct client_t {
    char id[51];
    int socket;
    bool connected;
    std::vector<topic_t> topics;
    std::vector<char *> stored;
} client_t;
```

The client can accept 3 types of commands from the input: 

- exit: The server receives a message "out" from the client.

- subscribe <TOPIC> <SF>: The message is forwarded to the server so that it can maintain the subscription. The SF (store and forward) field is a boolean. When SF is set the message with store the messages received from UDP when the TCP client is disconnected and forward them later.

- unsubscribe <TOPIC>

These are implemented in:

```c++
bool handle_stdin_cmd(std::vector<char*> cmd);

bool handle_server_cmd(char buff[BUFSIZ]);
```

The messages received when the 
### UDP client
Implemented by the course's team.

It sends messages store inside a struct:

```c++
typedef struct upd_message_t {
    char topic[50];
    uint8_t data_type;
    char data[1501];
} udp_message_t;
```

Data type what kind of messages are sent:

- integers (0)
- short reals (1)
- floats (2)
- strings (3)

# Directory structure

```bash
├── Enunt_Tema_2_Protocoale_2022_2023.pdf
├── Makefile
├── pcom_hw2_udp_client
│   ├── sample_payloads.json
│   ├── three_topics_payloads.json
│   ├── udp_client.py
│   └── utils
│       ├── __init__.py
│       ├── __pycache__
│       │   ├── __init__.cpython-310.pyc
│       │   └── unpriv_port.cpython-310.pyc
│       └── unpriv_port.py
├── README.md
├── server.cpp
├── server.hpp
├── subscriber.cpp
├── subscriber.hpp
├── test.py
├── utils.cpp
└── utils.hpp
```

Contains the makefile used for compiling the .cpp files. test.py is the checker, together with the pcom_hw2_udp_client/* files, is provided by the course's team.

# Checker

In order to run the checker run the command:

```bash
sudo python3 test.py
```# Client-Server-TCP-UDP-message-management-application
