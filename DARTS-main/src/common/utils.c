#include "../../include/utils.h"
#include <time.h>

static pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER;

// Windows inet_pton wrapper
#ifdef _WIN32
int inet_pton(int af, const char *src, void *dst) {
    struct sockaddr_storage ss;
    int size = sizeof(ss);
    char src_copy[INET6_ADDRSTRLEN + 1];
    strncpy(src_copy, src, INET6_ADDRSTRLEN);
    src_copy[INET6_ADDRSTRLEN] = '\0';
    
    if (WSAStringToAddressA(src_copy, af, NULL, (struct sockaddr *)&ss, &size) == 0) {
        switch(af) {
            case AF_INET:
                *(struct in_addr *)dst = ((struct sockaddr_in *)&ss)->sin_addr;
                return 1;
            case AF_INET6:
                *(struct in6_addr *)dst = ((struct sockaddr_in6 *)&ss)->sin6_addr;
                return 1;
            default:
                return 0;
        }
    }
    return 0;
}
#endif

void die(const char *msg) {
    perror(msg);
    #ifdef _WIN32
        printf("Winsock Error: %d\n", WSAGetLastError());
    #endif
    exit(1);
}

void log_msg(const char *msg) {
    time_t now;
    time(&now);
    char *date = ctime(&now);
    date[strlen(date) - 1] = '\0';
    printf("[%s] LOG: %s\n", date, msg);
    fflush(stdout);

    pthread_mutex_lock(&log_lock);
    FILE *logfile = fopen("darts_chat.log", "a");
    if (logfile) {
        fprintf(logfile, "[%s] %s\n", date, msg);
        fclose(logfile);
    }
    pthread_mutex_unlock(&log_lock);
}

void log_chat_broadcast(const char *source, const char *data) {
    time_t now;
    time(&now);
    char *date = ctime(&now);
    if (!date) return;
    char datebuf[64];
    strncpy(datebuf, date, sizeof(datebuf) - 1);
    datebuf[sizeof(datebuf) - 1] = '\0';
    size_t n = strlen(datebuf);
    if (n > 0 && datebuf[n - 1] == '\n')
        datebuf[n - 1] = '\0';

    pthread_mutex_lock(&log_lock);
    FILE *logfile = fopen("darts_chat.log", "a");
    if (logfile) {
        fprintf(logfile, "[%s] %s: %s\n", datebuf, source ? source : "", data ? data : "");
        fclose(logfile);
    }
    pthread_mutex_unlock(&log_lock);
}

void send_packet(int fd, Packet *pkt) {
    int bytes_sent = send(fd, (char*)pkt, sizeof(Packet), 0);
    if (bytes_sent < 0) {
        perror("Send failed");
        log_msg("Failed to send packet to peer");
    } else if ((size_t)bytes_sent != sizeof(Packet)) {
        log_msg("Partial packet sent - possible connection issue");
    }
}

int recv_packet(int fd, Packet *pkt) {
    // Replaced read() with recv()
    int bytes = recv(fd, (char*)pkt, sizeof(Packet), 0);
    return bytes;
}