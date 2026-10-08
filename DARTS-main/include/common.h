#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

// --- Windows Specifics ---
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
    
    // Windows requires linking against ws2_32.lib
    // If using MinGW, add -lws2_32 to Makefile
    #pragma comment(lib, "ws2_32.lib")

    #define close closesocket
    #define sleep(x) Sleep(1000 * (x))
    #define usleep(x) Sleep((x) / 1000)
    
    typedef int socklen_t;
    #define ISVALIDSOCKET(s) ((s) != INVALID_SOCKET) 
    #define GETSOCKETERRNO() (WSAGetLastError())
    // Windows socket type is SOCKET (unsigned), not int
    // We can stick to int for logic, but be aware of signed/unsigned warnings
#else
// --- Linux/Mac Specifics ---
    #include <unistd.h>
    #include <arpa/inet.h>
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <sys/select.h>
    #include <netinet/in.h>
    typedef int socklen_t;
#endif

// Threading (Use pthread for both - MinGW on Windows supports this)
#include <pthread.h>

#define PORT 8888 // port address
#define MAX_CLIENTS 100 //max user amount
#define BUFFER_SIZE 2048
#define NAME_LEN 32

typedef enum {
    MSG_LOGIN,
    MSG_PRIVATE,
    MSG_BROADCAST,
    MSG_LIST,
    MSG_EXIT,
    MSG_ERROR,
    MSG_ACK
} MsgType;

typedef struct __attribute__((packed)) {
    MsgType type;
    char source[NAME_LEN];
    char target[NAME_LEN];
    char data[BUFFER_SIZE];
} Packet;

#endif