CC = gcc
# Added -lws2_32 for Windows Sockets
LDFLAGS = -pthread -lws2_32 -liphlpapi
CFLAGS = -Wall -I./include -g -D_WIN32_WINNT=0x0601

SRCS_COMMON = src/common/ds.c src/common/utils.c
SRCS_SERVER = src/server/server.c $(SRCS_COMMON)
SRCS_CLIENT = src/client/client.c $(SRCS_COMMON)

all: server client

server: $(SRCS_SERVER)
	$(CC) $(CFLAGS) -o darts_server $(SRCS_SERVER) $(LDFLAGS)

client: $(SRCS_CLIENT)
	$(CC) $(CFLAGS) -o darts_client $(SRCS_CLIENT) $(LDFLAGS)

clean:
	rm -f darts_server.exe darts_client.exe