#include "server.h"


// socket debug messages
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

// sets socket flags so recv() don't block the thread
int set_non_blocking(int sockfd) {
    CHK(fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFD, 0) | O_NONBLOCK)) // NOLINT(hicpp-signed-bitwise)
    return 0;
}

// prints help
void print_help() {
    printf("Usage: server [OPTION]\n"
           "-v, --verbose\t\t\tDebug mode ON\n"
           "-p=INT, --port=INT\t\t\tspecify port\n"
           "-ip=\t\t\t\tspecify ip\n"
           "-u=INT, --userlimit=INT\t\tset user limit\n"
           "--gen-key\t\t\tgenerate new encryption key and quit\n"
           "-h, --help\t\t\tprint this and terminate\n");
}

// CLI argument parser

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
    } else if (strncmp(arg, "--gen-key", 9) == 0) {
        args->k = 1;
    } else if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
        print_help();
        exit(0);
    } else {
        printf("%sERROR: argument not recognized -- %s\n", RED, arg);
        exit(0);
    }

    return 1;
}

// generates history by client request
string gen_history() {
    string result;
    size_t offset = message_history.size() < HISTORY_LEN ? 0 : message_history.size() - HISTORY_LEN;
    for (auto it = message_history.begin() + offset; it != message_history.end(); it++) {
        result += *it + "\n";
    }
    return result;
}

// generate symmetric keys to be exchanged
void generate_key() {
    AutoSeededRandomPool rnd;
    rnd.GenerateBlock(AES_KEY, KEY_LEN);
    rnd.GenerateBlock(IV, IV_LEN);

    ArraySource(AES_KEY, sizeof(AES_KEY), true, new FileSink("key"));
    ArraySource(IV, sizeof(IV), true, new FileSink("iv"));
}

// reads encryption keys from files and creates objects
void init_encryption() {
    FileSource("key", true, new ArraySink(AES_KEY, sizeof(AES_KEY))); // NOLINT(bugprone-unused-raii)
    FileSource("iv", true, new ArraySink(IV, sizeof(IV))); // NOLINT(bugprone-unused-raii)
    AESEncryption = new CFB_Mode<AES>::Encryption(AES_KEY, KEY_LEN, IV);
    AESDecryption = new CFB_Mode<AES>::Decryption(AES_KEY, KEY_LEN, IV);
}

// this is a workaround for some weird bug
void reinit_encryption() {
    delete AESEncryption;
    delete AESDecryption;
    AESEncryption = new CFB_Mode<AES>::Encryption(AES_KEY, KEY_LEN, IV);
    AESDecryption = new CFB_Mode<AES>::Decryption(AES_KEY, KEY_LEN, IV);
}

// encryption function
void encrypt(char *message) {
    reinit_encryption();
    byte enc_buf[BUF_SIZE];
    memcpy(enc_buf, message, BUF_SIZE);
    AESEncryption->ProcessData(enc_buf, enc_buf, BUF_SIZE);
    memcpy(message, enc_buf, BUF_SIZE);
}

// decryption function
void decrypt(char *message) {
    reinit_encryption();
    byte enc_buf[BUF_SIZE];
    memcpy(enc_buf, message, BUF_SIZE);
    AESDecryption->ProcessData(enc_buf, enc_buf, BUF_SIZE);
    memcpy(message, enc_buf, BUF_SIZE);
}

#ifndef TESTING

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

    if (args.k) {
        generate_key();
        return 0;
    }

    init_encryption();

    int listener;

    struct sockaddr_in addr{}, their_addr{};

    addr.sin_family = PF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(args.p));
    addr.sin_addr.s_addr = inet_addr(args.ip);

    socklen_t socklen;
    socklen = sizeof(struct sockaddr_in);

    static struct epoll_event ev, events[EPOLL_SIZE];

    ev.events = EPOLLIN | EPOLLET;

    char message[BUF_SIZE];

    int epfd;

    clock_t tStart;

    int client, res, epoll_events_count;


    CHK2(listener, socket(PF_INET, SOCK_STREAM, 0))
    printf("Main listener(fd=%d) created! \n", listener);

    set_non_blocking(listener);


    CHK(bind(listener, (struct sockaddr *) &addr, sizeof(addr)))
    printf("Listener bound to: %s\n", args.ip);

    CHK(listen(listener, 1))
    printf("Start to listen: %s!\n", args.ip);

    CHK2(epfd, epoll_create(EPOLL_SIZE))
    printf("Epoll(fd=%d) created!\n", epfd);


    ev.data.fd = listener;


    CHK(epoll_ctl(epfd, EPOLL_CTL_ADD, listener, &ev))
    printf("Main listener(%d) added to epoll!\n", epfd);


    while (true) {
        CHK2(epoll_events_count, epoll_wait(epfd, events, EPOLL_SIZE, EPOLL_RUN_TIMEOUT))
        if (DEBUG_MODE) printf("Epoll events count: %d\n", epoll_events_count);

        tStart = clock();

        for (int i = 0; i < epoll_events_count; i++) {
            if (DEBUG_MODE) {
                printf("events[%d].data.fd = %d\n", i, events[i].data.fd);
                debug_epoll_event(events[i]);

            }
            if (events[i].data.fd == listener) {
                CHK2(client, accept(listener, (struct sockaddr *) &their_addr, &socklen))
                if (DEBUG_MODE)
                    printf("connection from:%s:%d, socket assigned to:%d \n",
                           inet_ntoa(their_addr.sin_addr),
                           ntohs(their_addr.sin_port),
                           client);

                set_non_blocking(client);


                ev.data.fd = client;


                CHK(epoll_ctl(epfd, EPOLL_CTL_ADD, client, &ev))

                int disconnect = 0;

                if (clients_list.size() < args.u) {
                    clients_list.push_back(client);
                    username_dict[client] = to_string(client);
                } else {
                    disconnect = 1;
                }

                if (DEBUG_MODE) {
                    printf("Add new client(fd = %d) to epoll and now %d online\n", client,
                           static_cast<int>(clients_list.size()));
                }


                bzero(message, BUF_SIZE);
                if (!disconnect) {
                    string builder = STR_WELCOME + to_string(client) + "\n" + gen_history();
                    res = snprintf(message, BUF_SIZE, "%s%s%s", YEL, builder.c_str(), DEF);
                } else {
                    res = snprintf(message, BUF_SIZE, STR_DISCONNECT);
                }


                if (DEBUG_MODE) {
                    printf("%d\n", res);
                }

                encrypt(message);
                CHK2(res, send(client, message, BUF_SIZE, 0))


                if (disconnect) {
                    shutdown(client, SHUT_RDWR);
                    close(client);
                } else {
                    list<int>::iterator it;
                    for (it = clients_list.begin(); it != clients_list.end(); it++) {
                        if (*it != client) {
                            char msg[256];
                            bzero(msg, 256);
                            snprintf(msg, BUF_SIZE, "%sClient #%d has joined chat%s", BLU, client, DEF);
                            encrypt(msg);
                            CHK(send(*it, msg, BUF_SIZE, 0))
                        }
                    }
                }

            } else {
                CHK2(res, handle_message(events[i].data.fd))
            }
        }

        printf("Statistics: %d events handled at: %.2f second(s)\n",
               epoll_events_count,
               (double) (clock() - tStart) / CLOCKS_PER_SEC);
    }
}

#endif

int handle_message(int client) {

    char buf[BUF_SIZE], message[BUF_SIZE + 13];
    bzero(buf, BUF_SIZE);
    bzero(message, BUF_SIZE);

    int len;


    if (DEBUG_MODE) printf("Try to read from fd(%d)\n", client);
    CHK2(len, recv(client, buf, BUF_SIZE, 0))
    decrypt(buf);

    if (len == 0) {
        CHK(close(client))
        clients_list.remove(client);
        username_dict.erase(client);

        list<int>::iterator it;
        for (it = clients_list.begin(); it != clients_list.end(); it++) {
            char msg[256];
            bzero(msg, 256);
            snprintf(msg, BUF_SIZE, "%sClient #%d has left chat%s", BLU, client, DEF);
            encrypt(msg);
            CHK(send(*it, msg, BUF_SIZE, 0))
        }

        if (DEBUG_MODE)
            printf("Client with fd: %d closed! %d online\n", client, static_cast<int>(clients_list.size()));

    } else {

        if (strncmp(buf, CMD_ONLINE, strlen(CMD_ONLINE)) == 0) {

            string builder = "People online: ";

            list<int>::iterator it;
            for (it = clients_list.begin(); it != clients_list.end(); it++) {
                builder.append(username_dict[*it] + " ");
            }

            sprintf(message, "%s%s%s", RED, builder.c_str(), DEF);
            encrypt(message);
            CHK(send(client, message, BUF_SIZE, 0))
            if (DEBUG_MODE) printf("Message '%s' send to client with fd(%d) \n", message, client);

            return len;
        }

        if (strncmp(buf, CMD_SET_USERNAME, strlen(CMD_SET_USERNAME)) == 0) {
            string username = buf + (strlen(CMD_SET_USERNAME) + 1);

            list<int>::iterator it;
            for (it = clients_list.begin(); it != clients_list.end(); it++) {
                if (username_dict[*it] == username) {
                    return len;
                }
            }

            username_dict[client] = username;
            return len;
        }

        if (clients_list.size() == 1) { // this means that no one connected to server except YOU!
            string builder = username_dict[client] + ">> " + buf;
            message_history.emplace_back(builder);
            printf("%s\n", STR_NO_ONE_CONNECTED);
            char tmp[BUF_SIZE];
            strcpy(tmp, STR_NO_ONE_CONNECTED);
            encrypt(tmp);
            CHK(send(client, tmp, BUF_SIZE, 0))
            printf("%d\n", len);
            return len;
        }

        if (buf[0] == '@') { //private message
            string username = buf;
            size_t split = username.find_first_of(' ');
            username = username.substr(1, split - 1);

            string builder = GRN "[P]" + username_dict[client] + ">> " DEF + (buf + (split + 1));
            //lookup username in dict
            int client_id = -1;
            for (auto &it : username_dict) {
                if (it.second == username) {
                    client_id = it.first;
                }
            }

            //user not found
            if (client_id == -1) {
                client_id = client; //redirect message to himself
                builder = "SERVER: User " + username + " not found!";
            }

            //encrypt & send message
            strncpy(message, builder.c_str(), BUF_SIZE);
            encrypt(message);
            CHK(send(client_id, message, BUF_SIZE, 0))
            if (DEBUG_MODE) printf("Message '%s' send to client with fd(%d) \n", message, client_id);

        } else { //broadcast
            string builder = CYN + username_dict[client] + ">> " DEF + buf;
            message_history.emplace_back(builder);
            strncpy(message, builder.c_str(), BUF_SIZE);
            list<int>::iterator it;
            encrypt(message);
            for (it = clients_list.begin(); it != clients_list.end(); it++) {
                if (*it != client) {
                    CHK(send(*it, message, BUF_SIZE, 0))
                    if (DEBUG_MODE) printf("Message '%s' send to client with fd(%d) \n", message, *it);
                }
            }
            if (DEBUG_MODE)
                printf("Client(%d) received message successfully:'%s', total of %d bytes data\n", client, buf, len);
        }
    }

    return len;
}

#ifdef TESTING
#define CATCH_CONFIG_MAIN

#include "catch2/catch.hpp"

TEST_CASE("Test args init") {
    Args args{};
    CHECK(args.d == 0);
    CHECK(args.k == 0);
    CHECK(args.p == 0);
    CHECK(args.u == 0);
    CHECK(strlen(args.ip) == 0);
}

//-v, --verbose
TEST_CASE("Test args verbose") {
    Args args{};
    parse_argument(&args, "-v");
    CHECK(args.d == 1);
    CHECK(args.k == 0);
    CHECK(args.p == 0);
    CHECK(args.u == 0);
    CHECK(strlen(args.ip) == 0);

    args.d = 0;
    parse_argument(&args, "--verbose");
    CHECK(args.d == 1);
    CHECK(args.k == 0);
    CHECK(args.p == 0);
    CHECK(args.u == 0);
    CHECK(strlen(args.ip) == 0);
}

//-p, --port
TEST_CASE("Test args port") {
    Args args{};
    parse_argument(&args, "-p=6969");
    CHECK(args.d == 0);
    CHECK(args.k == 0);
    CHECK(args.p == 6969);
    CHECK(args.u == 0);
    CHECK(strlen(args.ip) == 0);

    args.d = 0;
    parse_argument(&args, "--port=11235");
    CHECK(args.d == 0);
    CHECK(args.k == 0);
    CHECK(args.p == 11235);
    CHECK(args.u == 0);
    CHECK(strlen(args.ip) == 0);
}

//-u, --userlimit
TEST_CASE("Test args user limit") {
    Args args{};
    parse_argument(&args, "-u=42");
    CHECK(args.d == 0);
    CHECK(args.k == 0);
    CHECK(args.p == 0);
    CHECK(args.u == 42);
    CHECK(strlen(args.ip) == 0);

    args.d = 0;
    parse_argument(&args, "--userlimit=420");
    CHECK(args.d == 0);
    CHECK(args.k == 0);
    CHECK(args.p == 0);
    CHECK(args.u == 420);
    CHECK(strlen(args.ip) == 0);
}

//--gen-key
TEST_CASE("Test args gen key") {
    Args args{};
    parse_argument(&args, "--gen-key");
    CHECK(args.d == 0);
    CHECK(args.k == 1);
    CHECK(args.p == 0);
    CHECK(args.u == 0);
    CHECK(strlen(args.ip) == 0);
}

#endif