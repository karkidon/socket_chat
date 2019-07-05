#include <strings.h>
#include <cstring>
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
#include <crypto++/cryptlib.h>
#include <crypto++/secblock.h>
#include <crypto++/hrtimer.h>
#include <crypto++/osrng.h>
#include <crypto++/modes.h>
#include <crypto++/aes.h>
#include <crypto++/files.h>


//namespace resolution
using std::cout;
using std::endl;
using std::string;
using std::map;
using std::vector;
using std::list;
using std::to_string;
using CryptoPP::AutoSeededRandomPool;
using CryptoPP::ArraySource;
using CryptoPP::CFB_Mode;
using CryptoPP::AES;
using CryptoPP::FileSource;
using CryptoPP::FileSink;
using CryptoPP::ArraySink;


//colors
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

// server ip, you should change it to your own server ip address
#define SERVER_HOST "127.0.0.1"

// Default timeout - http://linux.die.net/man/2/epoll_wait
#define EPOLL_RUN_TIMEOUT -1

// Count of connections that we are planning to handle (just hint to kernel)
#define EPOLL_SIZE 10000

// Command to exit
#define CMD_EXIT "EXIT"


//socket eval macros
#define CHK(eval) if(eval < 0){perror("eval"); exit(-1);}
#define CHK2(res, eval) if((res = eval) < 0){perror("eval"); exit(-1);}

//encryption stuff
#define KEY_LEN AES::DEFAULT_KEYLENGTH
byte AES_KEY[KEY_LEN];

#define IV_LEN AES::BLOCKSIZE
byte IV[IV_LEN];

CFB_Mode<AES>::Encryption *AESEncryption;
CFB_Mode<AES>::Decryption *AESDecryption;


// chat message buffer
char message[BUF_SIZE];

// for debug mode
int DEBUG_MODE = 0;

/*
  We use 'fork' to make two process.
    Child process:
    - waiting for user's input message;
    - and sending all users messages to parent process through pipe.
    ('man pipe' has good example how to do it)

    Parent process:
    - waiting for incoming messages(EPOLLIN):
    -- from server(socket) to display;
    -- from child process(pipe) to transmit to server(socket)

*/

struct Args {
    int d;
    int p;
    int m;
    char ip[16];
};

void print_help() {
    printf("Usage: client [OPTION]\n"
           "-v, --verbose\t\t\tDebug mode ON\n"
           "-p, --port\t\t\tspecify port\n"
           "-ip\t\t\t\tspecify ip\n"
           "-m=INT, --maxmsglen=INT\t\tset user limit\n"
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
    } else if (strncmp(arg, "-m=", 3) == 0) {
        args->m = (int) strtol(arg + 3, nullptr, 10);
    } else if (strncmp(arg, "--maxmsglen=", 12) == 0) {
        args->m = (int) strtol(arg + 12, nullptr, 10);
    } else if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
        print_help();
        exit(0);
    } else {
        printf("%sERROR: argument not recognized -- %s\n", RED, arg);
        exit(0);
    }

    return 1;
}

void init_encryption() {
    FileSource("key", true, new ArraySink(AES_KEY, sizeof(AES_KEY))); // NOLINT(bugprone-unused-raii)
    FileSource("iv", true, new ArraySink(IV, sizeof(IV))); // NOLINT(bugprone-unused-raii)
    AESEncryption = new CFB_Mode<AES>::Encryption(AES_KEY, KEY_LEN, IV);
    AESDecryption = new CFB_Mode<AES>::Decryption(AES_KEY, KEY_LEN, IV);
}

void reinit_encryption() {
    delete AESEncryption;
    delete AESDecryption;
    AESEncryption = new CFB_Mode<AES>::Encryption(AES_KEY, KEY_LEN, IV);
    AESDecryption = new CFB_Mode<AES>::Decryption(AES_KEY, KEY_LEN, IV);
}

void encrypt() {
    reinit_encryption();
    byte enc_buf[BUF_SIZE];
    memcpy(enc_buf, message, BUF_SIZE);
    AESEncryption->ProcessData(enc_buf, enc_buf, BUF_SIZE);
    memcpy(message, enc_buf, BUF_SIZE);
}

void decrypt() {
    reinit_encryption();
    byte enc_buf[BUF_SIZE];
    memcpy(enc_buf, message, BUF_SIZE);
    AESDecryption->ProcessData(enc_buf, enc_buf, BUF_SIZE);
    memcpy(message, enc_buf, BUF_SIZE);
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

    if (args.d) {
        DEBUG_MODE = 1;
        printf("%sDebug:%d%s\n", YEL, args.d, DEF);
        printf("%sPort:%d%s\n", YEL, args.p, DEF);
        printf("%sMessage size:%d%s\n", YEL, args.m, DEF);
        printf("%sIP:%s%s\n", YEL, args.ip, DEF);
    }


    if (DEBUG_MODE) {
        printf("Debug mode is ON!\n");
        printf("MAIN: argc = %d\n", argc);
        for (int i = 0; i < argc; i++)
            printf(" argv[%d] = %s\n", i, argv[i]);
    } else printf("Debug mode is OFF!\n");

    init_encryption();

    // *** Define values
    //     socket connection with server(sock)
    //     process ID(pid)
    //     pipe between children & parent processes(pipe_fd)
    //     epoll descriptor to watch events
    int sock, pid, pipe_fd[2], epfd;

    //     define ip & ports for server(addr)
    struct sockaddr_in addr{};
    addr.sin_family = PF_INET;
    addr.sin_port = htons(args.p);
    addr.sin_addr.s_addr = inet_addr(args.ip);

    //     event template for epoll_ctl(ev)
    //     storage array for incoming events from epoll_wait(events)
    //     and maximum events count could be 2
    //     'sock' from server and 'pipe' from parent process(user inputs)
    static struct epoll_event ev, events[2]; // Socket(in|out) & Pipe(in)
    ev.events = EPOLLIN | EPOLLET;

    //     if it's zero, we should should down client
    int continue_to_work = 1;

    // *** Setup socket connection with server
    CHK2(sock, socket(PF_INET, SOCK_STREAM, 0))
    CHK(connect(sock, (struct sockaddr *) &addr, sizeof(addr)) < 0)

    // *** Setup pipe to send messages from child process to parent
    CHK(pipe(pipe_fd))
    if (DEBUG_MODE)
        printf("Created pipe with pipe_fd[0](read part): %d and pipe_fd[1](write part): % d\n",
               pipe_fd[0],
               pipe_fd[1]);

    // *** Create & configure epoll
    CHK2(epfd, epoll_create(EPOLL_SIZE))
    if (DEBUG_MODE) printf("Created epoll with fd: %d\n", epfd);

    //     add server connection(sock) to epoll to listen incoming messages from server
    ev.data.fd = sock;
    CHK(epoll_ctl(epfd, EPOLL_CTL_ADD, sock, &ev))
    if (DEBUG_MODE) printf("Socket connection (fd = %d) added to epoll\n", sock);

    //     add read part of pipe(pipe_fd[0]) to epoll
    //     to listen incoming messages from child process
    ev.data.fd = pipe_fd[0];
    CHK(epoll_ctl(epfd, EPOLL_CTL_ADD, pipe_fd[0], &ev))
    if (DEBUG_MODE) printf("Pipe[0] (read) with fd(%d) added to epoll\n", pipe_fd[0]);

    // Fork
    CHK2(pid, fork())
    switch (pid) { // NOLINT(hicpp-multiway-paths-covered)
        case 0: // child process
            close(pipe_fd[0]); // we don't need read pipe anymore
            printf("Enter 'exit' to exit\n");
            while (continue_to_work) {
                bzero(&message, BUF_SIZE);
                fgets(message, BUF_SIZE, stdin); //read from kbd

                // close while cycle for 'exit' command
                if (strncasecmp(message, CMD_EXIT, strlen(CMD_EXIT)) == 0) {
                    continue_to_work = 0;
                    // send user's message to parent process
                } else CHK(write(pipe_fd[1], message, strlen(message) - 1))
            }
            break;
        default: //parent process
            close(pipe_fd[1]); // we don't need write pipe anymore

            // incoming epoll_wait's events count(epoll_events_count)
            // results of different functions(res)
            int epoll_events_count, res;

            // *** Main cycle(epoll_wait)
            while (continue_to_work) {
                CHK2(epoll_events_count, epoll_wait(epfd, events, 2, EPOLL_RUN_TIMEOUT))
                if (DEBUG_MODE) printf("Epoll events count: %d\n", epoll_events_count);

                for (int i = 0; i < epoll_events_count; i++) {
                    bzero(&message, BUF_SIZE);

                    // EPOLLIN event from server( new message from server)
                    if (events[i].data.fd == sock) {
                        if (DEBUG_MODE) printf("Server sends new message!\n");
                        CHK2(res, recv(sock, message, BUF_SIZE, 0))
                        decrypt();

                        // zero size of result means the server closed connection
                        if (res == 0) {
                            if (DEBUG_MODE) printf("Server closed connection: %d\n", sock);
                            CHK(close(sock))
                            continue_to_work = 0;
                        } else {
                            printf("%s\n", message);
                        }

                        // EPOLLIN event from child process(user's input message)
                    } else {
                        if (DEBUG_MODE) printf("New pipe event!\n");
                        CHK2(res, read(events[i].data.fd, message, BUF_SIZE))

                        // zero size of result means the child process going to exit
                        if (res == 0) continue_to_work = 0; // exit parent to
                            // send message to server
                        else {
                            encrypt();
                            CHK(send(sock, message, BUF_SIZE, 0))
                        }
                    }
                }
            }
    }
    if (pid) {
        if (DEBUG_MODE) printf("Shutting down parent!\n");
        close(pipe_fd[0]);
        close(sock);
    } else {
        if (DEBUG_MODE) printf("Shutting down child!\n");
        close(pipe_fd[1]);
    }

    return 0;
}