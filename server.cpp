#include <strings.h>
#include <string.h>
#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <list>
#include <time.h>

using namespace std;

#define RED   "\x1B[31m"
#define GRN   "\x1B[32m"
#define YEL   "\x1B[33m"
#define CYN   "\x1B[36m"
#define MAG   "\x1B[35m"
#define BLU   "\x1B[34m"
#define DEF   "\x1B[0m"

// Default buffer size
#define BUF_SIZE 4096

// Default port
#define SERVER_PORT 44444

// Server ip, you should change it to your own server ip address
#define SERVER_HOST "127.0.0.1"

// Default user limit
#define SERVER_USER_LIMIT 30

// Default timeout - http://linux.die.net/man/2/epoll_wait
#define EPOLL_RUN_TIMEOUT -1

// Count of connections that we are planning to handle (just hint to kernel)
#define EPOLL_SIZE 10000

// First welcome message from server
#define STR_WELCOME "Welcome! You ID is: Client #%d"

// Disconnect
#define STR_DISCONNECT "Server is full. Bye."

// Format of message population
#define STR_MESSAGE "Client #%d>> %s"

// Warning message if you alone in server
#define STR_NO_ONE_CONNECTED "No one connected to server except you!"


// Macros - exit in any error (eval < 0) case
#define CHK(eval) if(eval < 0){perror("eval"); exit(-1);}

// Macros - same as above, but save the result(res) of expression(eval)
#define CHK2(res, eval) if((res = eval) < 0){perror("eval"); exit(-1);}

// Preliminary declaration of functions
int set_non_blocking(int sockfd);

void debug_epoll_event(epoll_event ev);

int handle_message(int client);

// Debug epoll_event
void debug_epoll_event(epoll_event ev) {
    printf("fd(%d), ev.events:", ev.data.fd);

    if (ev.events & EPOLLIN)
        printf(" EPOLLIN ");
    if (ev.events & EPOLLOUT)
        printf(" EPOLLOUT ");
    if (ev.events & EPOLLET)
        printf(" EPOLLET ");
    if (ev.events & EPOLLPRI)
        printf(" EPOLLPRI ");
    if (ev.events & EPOLLRDNORM)
        printf(" EPOLLRDNORM ");
    if (ev.events & EPOLLRDBAND)
        printf(" EPOLLRDBAND ");
    if (ev.events & EPOLLWRNORM)
        printf(" EPOLLRDNORM ");
    if (ev.events & EPOLLWRBAND)
        printf(" EPOLLWRBAND ");
    if (ev.events & EPOLLMSG)
        printf(" EPOLLMSG ");
    if (ev.events & EPOLLERR)
        printf(" EPOLLERR ");
    if (ev.events & EPOLLHUP)
        printf(" EPOLLHUP ");
    if (ev.events & EPOLLONESHOT)
        printf(" EPOLLONESHOT ");

    printf("\n");

}


// Setup non blocking socket
int set_non_blocking(int sockfd) {
    CHK(fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFD, 0) | O_NONBLOCK));
    return 0;
}


// To store client's socket list
list<int> clients_list;

// for debug mode
int DEBUG_MODE = 0;

struct Args {
    int d;
    int p;
    int u;
    char ip[16];
};

void print_help() {
    printf("Usage: server [OPTION]\n"
           "-v, --verbose\t\t\tDebug mode ON\n"
           "-p, --port\t\t\tspecify port\n"
           "-ip\t\t\t\tspecify ip\n"
           "-u=INT, --userlimit=INT\t\tset user limit\n"
           "-h, --help\t\t\tprint this and terminate\n");
}

int parse_argument(Args *args, const char *arg) {
    if (strncmp(arg, "-v", 2) == 0) {
        args->d = 1;
    } else if (strncmp(arg, "--verbose", 8) == 0) {
        args->d = 1;
    } else if (strncmp(arg, "-p=", 3) == 0) {
        args->p = (int) strtol(arg + 3, nullptr, 10);
    } else if (strncmp(arg, "--port=", 7) == 0) {
        args->p = (int) strtol(arg + 7, nullptr, 10);
    } else if (strncmp(arg, "-ip=", 4) == 0) {
        strcpy(args->ip, arg + 4);
    } else if (strncmp(arg, "-u=", 3) == 0) {
        args->u = (int) strtol(arg + 3, nullptr, 10);
    } else if (strncmp(arg, "--userlimit=", 12) == 0) {
        args->u = (int) strtol(arg + 12, nullptr, 10);
    } else if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
        print_help();
        exit(0);
    } else {
        printf("%sERROR: argument not recognized -- %s\n", RED, arg);
        exit(0);
    }

    return 1;
}


int main(int argc, char *argv[]) {

    Args args{};

    for (int i = 1; i < argc; i++) {
        parse_argument(&args, argv[i]);
    }

    if (strlen(args.ip) == 0) {
        strcpy(args.ip, SERVER_HOST);
    }

    if (args.p == 0) {
        args.p = SERVER_PORT;
    }

    if (args.u == 0) {
        args.u = SERVER_USER_LIMIT;
    }

    if (args.d) {
        DEBUG_MODE = 1;
        printf("%sDebug:%d%s\n", YEL, args.d, DEF);
        printf("%sPort:%d%s\n", YEL, args.p, DEF);
        printf("%sUser limit:%d%s\n", YEL, args.u, DEF);
        printf("%sIP:%s%s\n", YEL, args.ip, DEF);
    }

    if (argc > 1) DEBUG_MODE = 1;

    if (DEBUG_MODE) {
        printf("Debug mode is ON!\n");
        printf("MAIN: argc = %d\n", argc);
        for (int i = 0; i < argc; i++)
            printf(" argv[%d] = %s\n", i, argv[i]);
    } else printf("Debug mode is OFF!\n");

    // *** Define values
    //     main server listener
    int listener;

    // define ip & ports for server(addr)
    //     and incoming client ip & ports(their_addr)
    struct sockaddr_in addr{}, their_addr{};
    //     configure ip & port for listen
    addr.sin_family = PF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(args.p));
    addr.sin_addr.s_addr = inet_addr(args.ip);

    //     size of address
    socklen_t socklen;
    socklen = sizeof(struct sockaddr_in);

    //     event template for epoll_ctl(ev)
    //     storage array for incoming events from epoll_wait(events)
    //        and maximum events count could be EPOLL_SIZE
    static struct epoll_event ev, events[EPOLL_SIZE];
    //     watch just incoming(EPOLLIN)
    //     and Edge Triggered(EPOLLET) events
    ev.events = EPOLLIN | EPOLLET;

    //     chat message buffer
    char message[BUF_SIZE];

    //     epoll descriptor to watch events
    int epfd;

    //     to calculate the execution time of a program
    clock_t tStart;

    // other values:
    //     new client descriptor(client)
    //     to keep the results of different functions(res)
    //     to keep incoming epoll_wait's events count(epoll_events_count)
    int client, res, epoll_events_count;


    // *** Setup server listener
    //     create listener with PF_INET(IPv4) and
    //     SOCK_STREAM(sequenced, reliable, two-way, connection-based byte stream)
    CHK2(listener, socket(PF_INET, SOCK_STREAM, 0));
    printf("Main listener(fd=%d) created! \n", listener);

    //    setup non blocking socket
    set_non_blocking(listener);

    //    bind listener to address(addr)
    CHK(bind(listener, (struct sockaddr *) &addr, sizeof(addr)));
    printf("Listener bound to: %s\n", args.ip);

    //    start to listen connections
    CHK(listen(listener, 1));
    printf("Start to listen: %s!\n", args.ip);

    // *** Setup epoll
    //     create epoll descriptor
    //     and backup store for EPOLL_SIZE of socket events
    CHK2(epfd, epoll_create(EPOLL_SIZE));
    printf("Epoll(fd=%d) created!\n", epfd);

    //     set listener to event template
    ev.data.fd = listener;

    //     add listener to epoll
    CHK(epoll_ctl(epfd, EPOLL_CTL_ADD, listener, &ev));
    printf("Main listener(%d) added to epoll!\n", epfd);

    // *** Main cycle(epoll_wait)
    while (true) {
        CHK2(epoll_events_count, epoll_wait(epfd, events, EPOLL_SIZE, EPOLL_RUN_TIMEOUT));
        if (DEBUG_MODE) printf("Epoll events count: %d\n", epoll_events_count);
        // setup tStart time
        tStart = clock();

        for (int i = 0; i < epoll_events_count; i++) {
            if (DEBUG_MODE) {
                printf("events[%d].data.fd = %d\n", i, events[i].data.fd);
                debug_epoll_event(events[i]);

            }
            // EPOLLIN event for listener(new client connection)
            if (events[i].data.fd == listener) {
                CHK2(client, accept(listener, (struct sockaddr *) &their_addr, &socklen));
                if (DEBUG_MODE)
                    printf("connection from:%s:%d, socket assigned to:%d \n",
                           inet_ntoa(their_addr.sin_addr),
                           ntohs(their_addr.sin_port),
                           client);
                // setup non blocking socket
                set_non_blocking(client);

                // set new client to event template
                ev.data.fd = client;

                // add new client to epoll
                CHK(epoll_ctl(epfd, EPOLL_CTL_ADD, client, &ev));

                int disconnect = 0;

                // save new descriptor to further use
                if (clients_list.size() < args.u) {
                    clients_list.push_back(client); // add new connection to list of clients
                } else {
                    disconnect = 1;
                }

                if (DEBUG_MODE)
                    printf("Add new client(fd = %d) to epoll and now clients_list.size = %d\n",
                           client,
                           static_cast<int>(clients_list.size()));

                // send initial welcome message to client
                bzero(message, BUF_SIZE);
                if (!disconnect) {
                    res = sprintf(message, STR_WELCOME, client);
                } else {
                    res = sprintf(message, STR_DISCONNECT);
                }


                if (DEBUG_MODE) {
                    printf("%d\n", res);
                }

                CHK2(res, send(client, message, BUF_SIZE, 0));


                if (disconnect) {
                    shutdown(client, SHUT_RDWR);
                    close(client);
                } else {
                    list<int>::iterator it;
                    for (it = clients_list.begin(); it != clients_list.end(); it++) {
                        if (*it != client) { // ... except yourself of course
                            char msg[256];
                            bzero(msg, 256);
                            sprintf(msg, "Client #%d has joined chat", client);
                            CHK(send(*it, msg, BUF_SIZE, 0));
                        }
                    }
                }

            } else { // EPOLLIN event for others(new incoming message from client)
                CHK2(res, handle_message(events[i].data.fd));
            }
        }
        // print epoll events handling statistics
        printf("Statistics: %d events handled at: %.2f second(s)\n",
               epoll_events_count,
               (double) (clock() - tStart) / CLOCKS_PER_SEC);
    }

//    close(listener);
//    close(epfd);
//
//    return 0;
}

// *** Handle incoming message from clients
int handle_message(int client) {
    // get row message from client(buf)
    //     and format message to populate(message)
    char buf[BUF_SIZE], message[BUF_SIZE + 13];
    bzero(buf, BUF_SIZE);
    bzero(message, BUF_SIZE);

    // to keep different results
    int len;

    // try to get new raw message from client
    if (DEBUG_MODE) printf("Try to read from fd(%d)\n", client);
    CHK2(len, recv(client, buf, BUF_SIZE, 0));

    // zero size of len mean the client closed connection
    if (len == 0) {
        CHK(close(client));
        clients_list.remove(client);

        list<int>::iterator it;
        for (it = clients_list.begin(); it != clients_list.end(); it++) {
            char msg[256];
            bzero(msg, 256);
            sprintf(msg, "Client #%d has left chat", client);
            CHK(send(*it, msg, BUF_SIZE, 0));
        }

        if (DEBUG_MODE)
            printf("Client with fd: %d closed! And now clients_list.size = %d\n", client,
                   static_cast<int>(clients_list.size()));
        // populate message around the world
    } else {

        if (clients_list.size() == 1) { // this means that no one connected to server except YOU!
            printf("%s\n", STR_NO_ONE_CONNECTED);
            CHK(send(client, STR_NO_ONE_CONNECTED, strlen(STR_NO_ONE_CONNECTED), 0));
            printf("%d\n", len);
            return len;
        }

        // format message to populate
        sprintf(message, STR_MESSAGE, client, buf);

        // populate message around the world ;-)...
        list<int>::iterator it;
        for (it = clients_list.begin(); it != clients_list.end(); it++) {
            if (*it != client) { // ... except youself of course
                CHK(send(*it, message, BUF_SIZE, 0));
                if (DEBUG_MODE) printf("Message '%s' send to client with fd(%d) \n", message, *it);
            }
        }
        if (DEBUG_MODE)
            printf("Client(%d) received message successfully:'%s', a total of %d bytes data...\n",
                   client,
                   buf,
                   len);
    }

    return len;
}
