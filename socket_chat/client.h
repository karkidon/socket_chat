#ifndef SOCKET_CHAT_CLIENT_H
#define SOCKET_CHAT_CLIENT_H

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

///@file
///namespace resolution
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

///colors
#define RED   "\x1B[31m"
#define GRN   "\x1B[32m"
#define YEL   "\x1B[33m"
#define CYN   "\x1B[36m"
#define MAG   "\x1B[35m"
#define BLU   "\x1B[34m"
#define DEF   "\x1B[0m"


/// Default buffer size
#define BUF_SIZE 4096

/// Default port
#define SERVER_PORT 44444

/// server ip, you should change it to your own server ip address
#define SERVER_HOST "127.0.0.1"

/// Default timeout - http:///linux.die.net/man/2/epoll_wait
#define EPOLL_RUN_TIMEOUT -1

/// Count of connections that we are planning to handle (just hint to kernel)
#define EPOLL_SIZE 10000

/// Command to exit
#define CMD_EXIT "EXIT"

///socket eval macros
#define CHK(eval) if(eval < 0){perror("eval"); exit(-1);}
#define CHK2(res, eval) if((res = eval) < 0){perror("eval"); exit(-1);}

///encryption stuff
#define KEY_LEN AES::DEFAULT_KEYLENGTH
byte AES_KEY[KEY_LEN];

#define IV_LEN AES::BLOCKSIZE
byte IV[IV_LEN];

CFB_Mode<AES>::Encryption *AESEncryption;
CFB_Mode<AES>::Decryption *AESDecryption;


/// chat message buffer
char message[BUF_SIZE];

/// for debug mode
int DEBUG_MODE = 0;

/// "smart" way to store flags
typedef struct {
    int d;
    int p;
    int m;
    char ip[16];
} Args;

#endif ///SOCKET_CHAT_CLIENT_H
