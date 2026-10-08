# DARTS Protocol Specification

## Overview
DARTS (Distributed Asynchronous Real-time Talk System) uses a custom, fixed-size binary packet protocol over TCP. Every communication between the server and the client uses the same `Packet` `struct`.

## Packet Structure
The `Packet` structure is exact-size and transmitted directly over the TCP stream. Due to structural packing options `__attribute__((packed))`, it prevents compiler padding.

```c
#define BUFFER_SIZE 2048
#define NAME_LEN 32

typedef struct __attribute__((packed)) {
    MsgType type;
    char source[NAME_LEN];
    char target[NAME_LEN];
    char data[BUFFER_SIZE];
} Packet;
```

**Total Size:** The overall size of a packet depends on the enum size and architectures, but practically it ensures a completely fixed layout length. The maximum length of user data payload per packet is 2047 alphanumeric characters (2048th is null offset).

## Message Types

| Message Type    | ID Enum | Origin   | Description                                                                 |
|-----------------|---------|----------|-----------------------------------------------------------------------------|
| `MSG_LOGIN`     | 0       | Client   | Sent immediately after connection. Contains the requested username.         |
| `MSG_PRIVATE`   | 1       | Client   | Direct message to a specific user. `target` MUST be populated.              |
| `MSG_BROADCAST` | 2       | Client   | Global message sent to all connected users.                                 |
| `MSG_LIST`      | 3       | Client   | Requests the server to list all active, online usernames.                   |
| `MSG_EXIT`      | 4       | Client   | Notification of intentional disconnect. Precedes TCP close.                 |
| `MSG_ERROR`     | 5       | Server   | Indicates client logic failure (e.g. invalid permissions, target missing). |
| `MSG_ACK`       | 6       | Server   | Acknowledgment response payload (e.g. the payload list of active users).    |


## Communication Flows

### 1. Login & Connection Setup
1. Client establishes TCP connection to server (default Port `8888`).
2. Client sends `MSG_LOGIN`.
   - `source` = `requested_username`
   - `target` = (ignored)
   - `data` = (ignored)
3. Server validates input.
   - If acceptable, register username -> Socket mapping. Server triggers a `MSG_BROADCAST` stating `${source} has joined`.
   - If invalid, Server replies with `MSG_ERROR` and forcefully drops the connection.

### 2. Global Chat (Broadcast)
1. Client sends `MSG_BROADCAST`.
   - `source` = `client_username`
   - `data` = `message payload`
2. Server iterates the connected clients list and dispatches the exact same packet out sequentially to each client, exempting the origin sender.

### 3. Private Messaging
1. Client sends `MSG_PRIVATE`.
   - `target` = `recipient_username`
   - `data` = `private payload`
2. Server performs a hash lookup on `target` to map to Socket ID.
   - If found: Forwards packet directly over that user's socket connection.
   - If missing: Replies to calling Client with `MSG_ERROR` claiming `target` is not found.

### 4. Fetching Users
1. Client sends `MSG_LIST`.
2. Server collates iterating hash keys.
3. Server responds with `MSG_ACK`.
   - `data` = Comma-separated list of usernames. 
   - `source` = `SERVER`
   
### 5. Timeouts
- **Login Timeout:** If a client establishes TCP connection but does not send `MSG_LOGIN` within `120` seconds, the server will drop the connection.
- **Idle Timeout:** If a client does not send *any data* over the socket within `600` seconds of the last activity, the server will enforce a forcible disconnection on grounds of inactivity.
