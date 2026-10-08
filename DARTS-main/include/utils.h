#ifndef UTILS_H
#define UTILS_H

#include "common.h"

void die(const char *msg);
void log_msg(const char *msg);
void log_chat_broadcast(const char *source, const char *data);
void send_packet(int fd, Packet *pkt);
int recv_packet(int fd, Packet *pkt);

#ifdef _WIN32
int inet_pton(int af, const char *src, void *dst);
#endif

#endif