#ifndef SOCKET_CHAT_SERVER_H
#define SOCKET_CHAT_SERVER_H

///@file
//#define TESTING

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
#include <map>
#include <crypto++/cryptlib.h>
#include <crypto++/secblock.h>
#include <crypto++/hrtimer.h>
#include <crypto++/osrng.h>
#include <crypto++/modes.h>
#include <crypto++/aes.h>
#include <crypto++/files.h>


/// namespace resolution
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


/// colors
#define RED   "\x1B[31m"
#define GRN   "\x1B[32m"
#define YEL   "\x1B[33m"
#define CYN   "\x1B[36m"
#define MAG   "\x1B[35m"
#define BLU   "\x1B[34m"
#define DEF   "\x1B[0m"


/// server settings
#define BUF_SIZE 4096
#define SERVER_PORT 44444
#define SERVER_HOST "127.0.0.1"
#define SERVER_USER_LIMIT 30
#define EPOLL_RUN_TIMEOUT -1
#define EPOLL_SIZE 10000


/// string patterns
#define STR_WELCOME "Welcome! You username is: @"
#define STR_DISCONNECT "Server is full. Bye."
#define STR_NO_ONE_CONNECTED (MAG "No one connected to server except you!" DEF)


/// socket eval macros
#define CHK(eval) if(eval < 0){perror("eval"); exit(-1);}
#define CHK2(res, eval) if((res = eval) < 0){perror("eval"); exit(-1);}


/// chat settings
#define CMD_ONLINE "@online"
#define CMD_SET_USERNAME "@name"
#define HISTORY_LEN 10


/// encryption stuff
#define KEY_LEN AES::DEFAULT_KEYLENGTH
byte AES_KEY[KEY_LEN];

#define IV_LEN AES::BLOCKSIZE
byte IV[IV_LEN];

CFB_Mode<AES>::Encryption *AESEncryption;
CFB_Mode<AES>::Decryption *AESDecryption;


/// functions predeclared
int set_non_blocking(int sockfd);

void debug_epoll_event(epoll_event ev);

int handle_message(int client);


/// general globals
list<int> clients_list;
map<int, string> username_dict;
vector<string> message_history;

int DEBUG_MODE = 0;

/// "smart" way to store flags
typedef struct {
    int d;
    int p;
    int u;
    int k;
    char ip[16];
} Args;

#endif //SOCKET_CHAT_SERVER_H
