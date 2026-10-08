#include "../../include/common.h"
#include "../../include/ds.h"
#include "../../include/utils.h"
#include <signal.h>
#include <stdarg.h>
#include <time.h>
#ifdef _WIN32
static int darts_srv_out_tty(void) {
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    if (h == NULL || h == INVALID_HANDLE_VALUE) return 0;
    return GetFileType(h) == FILE_TYPE_CHAR;
}
#define DARTS_SRV_OUT_TTY() (darts_srv_out_tty())
#else
#include <unistd.h>
#define DARTS_SRV_OUT_TTY() (isatty(fileno(stdout)))
#endif

// #region agent log


#define LOGIN_TIMEOUT_SEC 120
#define IDLE_TIMEOUT_SEC 600

static Node* client_list = NULL;
static HashTable user_map;
static fd_set readfds;
static int max_sd;
static int server_fd = -1;
static pthread_mutex_t server_lock = PTHREAD_MUTEX_INITIALIZER;
static volatile sig_atomic_t g_server_shutdown = 0;

void handle_client_message(int fd);
void broadcast_message(Packet* pkt, int sender_fd);
void disconnect_client(int fd);

static void on_signal(int sig) {
    (void)sig;
    g_server_shutdown = 1;
}

static int login_source_valid(const Packet* pkt) {
    size_t n = strnlen(pkt->source, NAME_LEN);
    return n > 0 && n < NAME_LEN;
}

static void enforce_timeouts(void) {
    time_t now = time(NULL);
    Node* c = client_list;
    while (c != NULL) {
        Node* next = c->next;
        double idle = difftime(now, c->last_activity);
        if (!c->authenticated && idle > (double)LOGIN_TIMEOUT_SEC) {
            printf("Closing socket %d: login timeout\n", c->socket_fd);
            fflush(stdout);
            disconnect_client(c->socket_fd);
        } else if (c->authenticated && idle > (double)IDLE_TIMEOUT_SEC) {
            printf("Closing socket %d: idle timeout\n", c->socket_fd);
            fflush(stdout);
            disconnect_client(c->socket_fd);
        }
        c = next;
    }
}

int main(void) {
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("Failed. Error Code : %d\n", WSAGetLastError());
        return 1;
    }
#endif

    struct sockaddr_in address;
#ifdef _WIN32
    char opt = 1;
#else
    int opt = 1;
#endif
    int addrlen = sizeof(address);

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    hash_init(&user_map);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
#ifdef _WIN32
    if (server_fd == INVALID_SOCKET) die("socket failed");
#else
    if (server_fd < 0) die("socket failed");
#endif
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) die("setsockopt");

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
#ifdef _WIN32
        printf("WSA Error: %d\n", WSAGetLastError());
#endif
        die("bind failed");
    }
    if (listen(server_fd, 10) < 0) {
        perror("listen failed");
#ifdef _WIN32
        printf("WSA Error: %d\n", WSAGetLastError());
#endif
        die("listen");
    }

    log_msg("DARTS Server started on port 8888...");

    while (!g_server_shutdown) {
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 250000;

        pthread_mutex_lock(&server_lock);
        enforce_timeouts();

        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        max_sd = server_fd;

        Node* curr = client_list;
        while (curr != NULL) {
            int sd = curr->socket_fd;
            if (sd > 0) FD_SET(sd, &readfds);
            if (sd > max_sd) max_sd = sd;
            curr = curr->next;
        }
        pthread_mutex_unlock(&server_lock);

        int activity = select(max_sd + 1, &readfds, NULL, NULL, &tv);

        if (g_server_shutdown) break;

        if (activity < 0) {
#ifndef _WIN32
            if (errno == EINTR) continue;
#endif
            printf("select error\n");
            fflush(stdout);
            continue;
        }

        pthread_mutex_lock(&server_lock);

        if (FD_ISSET(server_fd, &readfds)) {
            int new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
#ifdef _WIN32
            if (new_socket == INVALID_SOCKET) {
#else
            if (new_socket < 0) {
#endif
                perror("accept");
            } else {
                list_add(&client_list, new_socket);
                printf("New connection, socket fd is %d\n", new_socket);
                fflush(stdout);
            }
        }

        curr = client_list;
        while (curr != NULL) {
            int sd = curr->socket_fd;
            Node* next = curr->next;
            if (FD_ISSET(sd, &readfds)) {
                handle_client_message(sd);
            }
            curr = next;
        }
        pthread_mutex_unlock(&server_lock);
    }

    printf("\nShutting down server...\n");
    fflush(stdout);
    log_msg("Server shutting down");

    pthread_mutex_lock(&server_lock);
    while (client_list != NULL) {
        int fd = client_list->socket_fd;
        disconnect_client(fd);
    }
    hash_cleanup(&user_map);
    pthread_mutex_unlock(&server_lock);

    close(server_fd);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}

void handle_client_message(int fd) {
    Packet pkt;
    int bytes = recv_packet(fd, &pkt);

    if (bytes <= 0) {
        disconnect_client(fd);
        return;
    }

    list_touch_activity(client_list, fd);

    switch (pkt.type) {
        case MSG_LOGIN: {
            if (!login_source_valid(&pkt)) {
                Packet err;
                err.type = MSG_ERROR;
                strncpy(err.source, "SERVER", NAME_LEN - 1);
                err.source[NAME_LEN - 1] = '\0';
                snprintf(err.data, BUFFER_SIZE, "Invalid username.");
                send_packet(fd, &err);
                disconnect_client(fd);
                break;
            }
            hash_insert(&user_map, pkt.source, fd);
            list_set_authenticated(client_list, fd, 1);
            printf("User logged in: %s (FD: %d)\n", pkt.source, fd);
            fflush(stdout);

            Packet welcome;
            welcome.type = MSG_BROADCAST;
            snprintf(welcome.data, BUFFER_SIZE, "SERVER: %s has joined the chat.", pkt.source);
            strncpy(welcome.source, "SERVER", NAME_LEN - 1);
            welcome.source[NAME_LEN - 1] = '\0';
            broadcast_message(&welcome, fd);
            break;
        }
        case MSG_BROADCAST:
            broadcast_message(&pkt, fd);
            break;
        case MSG_PRIVATE: {
            if (strnlen(pkt.target, NAME_LEN) == 0) {
                Packet err;
                err.type = MSG_ERROR;
                strncpy(err.source, "SERVER", NAME_LEN - 1);
                err.source[NAME_LEN - 1] = '\0';
                snprintf(err.data, BUFFER_SIZE, "Private message: missing target user.");
                send_packet(fd, &err);
                break;
            }
            int target_fd = hash_get(&user_map, pkt.target);
            if (target_fd != -1) {
                send_packet(target_fd, &pkt);
            } else {
                Packet err;
                err.type = MSG_ERROR;
                strncpy(err.source, "SERVER", NAME_LEN - 1);
                err.source[NAME_LEN - 1] = '\0';
                snprintf(err.data, BUFFER_SIZE, "User '%s' not found.", pkt.target);
                send_packet(fd, &err);
            }
            break;
        }
        case MSG_LIST: {
            Packet list_pkt;
            list_pkt.type = MSG_ACK;
            strncpy(list_pkt.source, "SERVER", NAME_LEN - 1);
            list_pkt.source[NAME_LEN - 1] = '\0';
            strncpy(list_pkt.data, "Online users: ", BUFFER_SIZE - 1);
            list_pkt.data[BUFFER_SIZE - 1] = '\0';
            size_t len = strlen(list_pkt.data);

            for (int i = 0; i < HASH_SIZE; i++) {
                HashEntry* e = user_map.buckets[i];
                while (e != NULL) {
                    int n = snprintf(list_pkt.data + len, BUFFER_SIZE - len, "%s, ", e->username);
                    if (n < 0 || (size_t)n >= BUFFER_SIZE - len) {
                        e = NULL;
                        break;
                    }
                    len += (size_t)n;
                    e = e->next;
                }
            }
            send_packet(fd, &list_pkt);
            break;
        }
        case MSG_EXIT:
            disconnect_client(fd);
            break;
        default:
            break;
    }
}

void broadcast_message(Packet* pkt, int sender_fd) {
    Node* curr = client_list;
    while (curr != NULL) {
        if (curr->socket_fd != sender_fd) {
            send_packet(curr->socket_fd, pkt);
        }
        curr = curr->next;
    }

    if (pkt->type == MSG_BROADCAST) {
        log_chat_broadcast(pkt->source, pkt->data);
    }
}

void disconnect_client(int fd) {
    char* name = hash_get_user_by_fd(&user_map, fd);
    if (name != NULL) {
        printf("User disconnected: %s\n", name);
        fflush(stdout);
        hash_remove(&user_map, name);
    } else {
        printf("Socket %d disconnected\n", fd);
        fflush(stdout);
    }
    close(fd);
    list_remove(&client_list, fd);
}
