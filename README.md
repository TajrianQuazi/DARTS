# 🎯 DARTS 

This repository contains a small, school-style CLI chat application written in C. It implements a simple TCP client/server protocol and demonstrates basic networking and data-structure patterns (linked list, queue, hash table).

This README explains how the project is organized, how to build it on Windows (MinGW) or Linux, how to run the server and client, and how to troubleshoot common issues.

**Quick summary:**
- **Build:** `make` (Makefile is configured for cross-platform use)
- **Run (server):** `./darts_server` (or `.\\darts_server.exe` on Windows)
- **Run (client):** `./darts_client <server_ip>` (or `.\\darts_client.exe <server_ip>`)

**Project layout**
- `Makefile`: build targets for `server` and `client`.
- `include/`: public headers (`common.h`, `ds.h`, `utils.h`).
- `src/server/`: server implementation (`server.c`).
- `src/client/`: client implementation (`client.c`).
- `src/common/`: common helpers and data structures (`ds.c`, `utils.c`).

## How it works (short)
- The server listens on port `8888` and uses `select()` to handle multiple clients without spawning a thread for every connection.
- The client connects to the server and sends/receives fixed-size `Packet` structures (defined in `include/common.h`).
- Basic commands supported by the client: `/all`, `/pm`, `/users`, `/quit`.

### Architecture Flow
```mermaid
sequenceDiagram
    participant C as Client
    participant S as Server
    
    C->>S: Connect (TCP Port 8888)
    C->>S: MSG_LOGIN (Username)
    S-->>S: Add to Client List & Hash Map
    S->>C: MSG_BROADCAST (X has joined)
    
    rect rgb(0, 0, 0, .05)
        Note right of C: Active Session Loop
        C->>S: MSG_BROADCAST (/all)
        S->>C: Routes packet to ALL clients
        
        C->>S: MSG_PRIVATE (/pm User)
        S-->>S: Hash lookup Target User FD
        S->>C: Routes packet to Target Client
    end
    
    C->>S: MSG_EXIT (/quit)
    S-->>S: Remove from Hash Map & List
    S-->>C: Close TCP Socket
```

## Build instructions

Prerequisites
- **Linux / macOS:** `gcc`, `make`.
- **Windows:** MinGW (or MinGW-w64) + `make`. If you're using MSYS2 or similar, install `mingw-w64-x86_64-toolchain` and `mingw-w64-x86_64-pthreads` if needed.

Build steps (from project root):
```powershell
make         # builds both server and client (creates darts_server and darts_client)
make client  # build only the client
make server  # build only the server
make clean   # remove binaries
```

Notes about Windows
- The Makefile already adds `-lws2_32` and `-liphlpapi` and defines `_WIN32_WINNT` so the project should build with MinGW. You may see a harmless warning about `#pragma comment` in `include/common.h`; that's an MSVC pragma and can be ignored.

## Run — basic usage

Start the server (in its own terminal):
```powershell
.\\darts_server.exe
# or on Unix-like:
./darts_server
```

Start a client and connect to the server (use localhost for local testing):
```powershell
.\\darts_client.exe 127.0.0.1
# or on Unix-like:
./darts_client 127.0.0.1
```

When the client starts it will prompt for a username. After login you can type commands.

Useful client commands
- **`/all <msg>`** : Broadcast a message to all connected users.
- **`/pm <user> <msg>`** : Send a private message to `<user>`.
- **`/users`** : Ask the server for the list of online users.
- **`/quit`** : Disconnect and quit the client.

You can also type any text (no leading `/`) and the client will broadcast it.

## Internals (for curious beginners)
- `include/common.h` — contains constants, `Packet` structure and `MsgType` enum. Packets include `type`, `source`, `target`, and `data` fields.
- `src/common/ds.c` — simple data-structures: linked list (connected clients), queue (message history), and a small hash table (username → socket FD).
- `src/common/utils.c` — helper functions for sending/receiving packets and a small Windows-compatible `inet_pton` wrapper.
- `src/server/server.c` — main server loop, `select()` usage, accept/connect handling, and message routing.
- `src/client/client.c` — input parsing, login flow, send/receive loop, and a reader thread for incoming messages.

## Troubleshooting

- **Client: "Connection refused" / Winsock Error 10061**
  - Ensure the server is running and listening on port `8888`.
  - Check with PowerShell: `netstat -an | findstr 8888` — you should see `LISTENING` on `0.0.0.0:8888`.
  - Windows Firewall may block the server; allow the binary or open the port.

- **Build/link errors about `inet_pton` or missing Winsock functions**
  - The project includes a Windows wrapper for `inet_pton` in `src/common/utils.c`. The `Makefile` links `-lws2_32`. If you still get errors, ensure MinGW is up to date and that you are using the correct toolchain (mingw-w64 recommended).

- **Warnings about `#pragma comment`**
  - This is an MSVC-specific pragma found in `include/common.h`. MinGW prints a harmless `ignoring #pragma comment` warning — safe to ignore.

## Common development tasks
- To add a new client command: edit `src/client/client.c` and parse the new command in the main loop.
- To add server-side logic: edit `src/server/server.c` — implement behavior in `handle_client_message`.

## Example quick test (local)
1. In Terminal A:
```powershell
.\\darts_server.exe
```
2. In Terminal B:
```powershell
.\\darts_client.exe 127.0.0.1
```
Enter `alice` as username.
3. In Terminal C:
```powershell
.\\darts_client.exe 127.0.0.1
```
Enter `bob` as username.
4. From `alice` type:
```
/all Hello everyone!
```
Both clients will receive the message.

## Roadmap & Upcoming Features (Localized Secure WiFi Chat)
- **Auto-Discovery:** Implement mDNS for automatic LAN/WiFi server discovery.
- **Security:** Add secure username/password authentication using a lightweight crypto library.
- **Client-Side Storage:** Integrate SQLite for persistent local chat histories.
- **Bundled Dependencies:** Include necessary dependencies locally for seamless, standalone compilation.
- **Stability Fixes:** Address identified test script pipeline issues and edge cases with piped input (see `byg.md`).

----
About US:-
1. Sadat Shaharier Sahaf
2. Tajrian Quazi
3. Abidur Rahman Dipto
4. Ahnaf Mushfiq Nafees
5. Md. Abdur Rahim
