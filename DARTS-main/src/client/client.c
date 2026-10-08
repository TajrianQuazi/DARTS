#include "../../include/common.h"
#include "../../include/utils.h"
#include <stdio.h>
#include <signal.h>
#include <stdarg.h>
#include <time.h>
#ifdef _WIN32
#include <string.h>
#define DARTS_STRNICMP _strnicmp
static int darts_out_tty(void) {
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    if (h == NULL || h == INVALID_HANDLE_VALUE) return 0;
    return GetFileType(h) == FILE_TYPE_CHAR;
}
static int darts_in_tty(void) {
    HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
    if (h == NULL || h == INVALID_HANDLE_VALUE) return 0;
    return GetFileType(h) == FILE_TYPE_CHAR;
}
#define DARTS_OUT_TTY() (darts_out_tty())
#define DARTS_IN_TTY() (darts_in_tty())
#else
#include <strings.h>
#include <unistd.h>
#include <sys/select.h>
#define DARTS_STRNICMP strncasecmp
#define DARTS_OUT_TTY() (isatty(fileno(stdout)))
#define DARTS_IN_TTY() (isatty(fileno(stdin)))
#endif

static int sock_fd;
static char username[NAME_LEN];
static volatile int running = 1;
static pthread_mutex_t client_lock = PTHREAD_MUTEX_INITIALIZER;
static volatile sig_atomic_t g_interrupt = 0;

static void on_client_signal(int sig) {
    (void)sig;
    g_interrupt = 1;
}

static int cmd_prefix(const char *buf, const char *cmd) {
    size_t n = strlen(cmd);
    return DARTS_STRNICMP(buf, cmd, n) == 0 && (buf[n] == '\0' || buf[n] == ' ');
}

static void copy_field(char *dst, size_t dst_sz, const char *src) {
    if (dst_sz == 0) return;
    if (src == NULL) {
        dst[0] = '\0';
        return;
    }
    strncpy(dst, src, dst_sz - 1);
    dst[dst_sz - 1] = '\0';
}

void* receive_handler(void* arg) {
    (void)arg;
    Packet pkt;
    for (;;) {
        int bytes = recv_packet(sock_fd, &pkt);
        if (bytes <= 0) {
            printf("\nServer disconnected.\n");
            fflush(stdout);
            pthread_mutex_lock(&client_lock);
            running = 0;
            pthread_mutex_unlock(&client_lock);
            break;
        }

        if (pkt.type == MSG_PRIVATE) {
            printf("\033[0;33m[PM from %s]: %s\033[0m\n", pkt.source, pkt.data);
        } else if (pkt.type == MSG_ERROR) {
            printf("\033[0;31m[ERROR]: %s\033[0m\n", pkt.data);
        } else if (pkt.type == MSG_ACK) {
            printf("\033[0;32m[INFO]: %s\033[0m\n", pkt.data);
        } else {
            printf("[%s]: %s\n", pkt.source, pkt.data);
            fflush(stdout);
        }
        printf("> ");
        fflush(stdout);
    }
    return NULL;
}

void print_new_ascii_art(void) {
    printf("                   ##########               \n");
    printf("               #####        #####   ###     \n");
    printf("            ####               ######       \n");
    printf("           ###            ##########        \n");
    printf("         ###          #######  #### ###     \n");
    printf("        ###      #########  ######   ###    \n");
    printf("        #   ############  ####### #   ###   \n");
    printf("       ##############   ########    ##  ##   \n");
    printf("     ############    #########       # ##   \n");
    printf("        ######    ###########        ####   \n");
    printf("       ##       ############ ######  ####   \n");
    printf("       ###    #############          ####   \n");
    printf("        ##   ##  ########           ####    \n");
    printf("         ##  ####  #####            ###     \n");
    printf("           # #        #  ##########  #      \n");
    printf("          ####                   ######     \n");
    printf("         ###  ####            ####          \n");
    printf("        ##       ############               \n");
    printf("                                            \n");
    printf("                                            \n");
}

void logo(void) {
    printf(" ::::::::::.  .:::::::::.  ::::::::::.   ::::::::::::    .:::::::::::\n");
    printf(" ::::   ::::  ::::   ::::  ::::   :::::      ::::        ::::\n");
    printf(" ::::   ::::  ::::   ::::  ::::   :::::      ::::        ::::\n");
    printf(" ::::   ::::  :::::::::::  ::::::::::'       ::::        ':::::::::.\n");
    printf(" ::::   ::::  ::::   ::::  ::::   ::::.      ::::               :::::\n");
    printf(" ::::   ::::  ::::   ::::  ::::    :::::     ::::               :::::\n");
    printf(" ::::::::::'  ::::   ::::  ::::     ::::     ::::        ::::::::::'\n");
}

void print_help(void) {
    printf("\n--- DARTS COMMANDS ---\n");
    printf("/all <msg>   : Broadcast message\n");
    printf("/pm <user> <msg> : Private message\n");
    printf("/users       : List online users\n");
    printf("/quit        : Exit\n");
    printf("----------------------\n");
}

int main(int argc, char const *argv[]) {
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("Failed. Error Code : %d\n", WSAGetLastError());
        return 1;
    }
#endif

    if (argc != 2) {
        printf("Usage: %s <server_ip>\n", argv[0]);
        return 1;
    }

    struct sockaddr_in serv_addr;
#ifdef _WIN32
    if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) die("Socket creation error");
#else
    if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) die("Socket creation error");
#endif

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, argv[1], &serv_addr.sin_addr) <= 0) die("Invalid address");

    if (connect(sock_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) die("Connection Failed");

    printf("Enter Username: ");
    fflush(stdout);
    if (scanf("%31s", username) != 1) {
        fprintf(stderr, "Invalid username input.\n");
        close(sock_fd);
        return 1;
    }
    {
        int ch;
        while ((ch = getchar()) != '\n' && ch != EOF) { }
    }
    

    Packet login;
    memset(&login, 0, sizeof(login));
    login.type = MSG_LOGIN;
    copy_field(login.source, NAME_LEN, username);
    send_packet(sock_fd, &login);

    signal(SIGINT, on_client_signal);
#ifndef _WIN32
    signal(SIGTERM, on_client_signal);
#endif

    pthread_t recv_thread;
    if (pthread_create(&recv_thread, NULL, receive_handler, NULL) != 0) die("Thread failed");

    print_new_ascii_art();
    logo();
    print_help();
    printf("> ");
    fflush(stdout);

    char buffer[BUFFER_SIZE];
   
    for (;;) {
        pthread_mutex_lock(&client_lock);
        int still = running;
        pthread_mutex_unlock(&client_lock);
        if (g_interrupt || !still) break;

        if (fgets(buffer, BUFFER_SIZE, stdin) == NULL) {
            /* Piped stdin closed: bounded wait for inbound TCP so recv thread can drain */
#ifdef _WIN32
            {
                fd_set rfds;
                struct timeval tv = {0, 250000};
                FD_ZERO(&rfds);
                FD_SET((SOCKET)sock_fd, &rfds);
                (void)select(0, &rfds, NULL, NULL, &tv);
            }
#else
            {
                fd_set rfds;
                struct timeval tv = {0, 250000};
                FD_ZERO(&rfds);
                FD_SET(sock_fd, &rfds);
                (void)select(sock_fd + 1, &rfds, NULL, NULL, &tv);
            }
#endif
            break;
        }

        buffer[strcspn(buffer, "\n")] = '\0';
        
        pthread_mutex_lock(&client_lock);
        still = running;
        pthread_mutex_unlock(&client_lock);
        if (!still) break;

        Packet pkt;
        memset(&pkt, 0, sizeof(pkt));
        copy_field(pkt.source, NAME_LEN, username);

        if (cmd_prefix(buffer, "/quit")) {
            pkt.type = MSG_EXIT;
            send_packet(sock_fd, &pkt);
            pthread_mutex_lock(&client_lock);
            running = 0;
            pthread_mutex_unlock(&client_lock);
            break;
        }
        if (cmd_prefix(buffer, "/users")) {
            pkt.type = MSG_LIST;
            send_packet(sock_fd, &pkt);
        } else if (cmd_prefix(buffer, "/pm")) {
            pkt.type = MSG_PRIVATE;
            char *p = buffer + 3;
            while (*p == ' ') p++;
            if (*p == '\0') {
                printf("Usage: /pm <user> <msg>\n");
            } else {
                char *sp = strchr(p, ' ');
                if (sp == NULL) {
                    printf("Usage: /pm <user> <msg>\n");
                } else {
                    *sp = '\0';
                    char *msg = sp + 1;
                    while (*msg == ' ') msg++;
                    if (*msg == '\0' || p[0] == '\0') {
                        printf("Usage: /pm <user> <msg>\n");
                    } else {
                        copy_field(pkt.target, NAME_LEN, p);
                        snprintf(pkt.data, BUFFER_SIZE, "%s", msg);
                        send_packet(sock_fd, &pkt);
                    }
                }
            }
        } else if (cmd_prefix(buffer, "/all")) {
            pkt.type = MSG_BROADCAST;
            char *rest = buffer + 4;
            while (*rest == ' ') rest++;
            if (*rest == '\0') {
                printf("Usage: /all <msg>\n");
            } else {
                snprintf(pkt.data, BUFFER_SIZE, "%s", rest);
                send_packet(sock_fd, &pkt);
            }
        } else {
            pkt.type = MSG_BROADCAST;
            snprintf(pkt.data, BUFFER_SIZE, "%s", buffer);
            send_packet(sock_fd, &pkt);
        }

        usleep(10000);
        printf("> ");
        fflush(stdout);
    }

    close(sock_fd);
    pthread_join(recv_thread, NULL);

#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
