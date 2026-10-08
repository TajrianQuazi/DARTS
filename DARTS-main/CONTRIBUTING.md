# Contributing to DARTS

Welcome! We're happy you want to contribute to DARTS. This guide will help you get started, whether you're fixing bugs, adding features, or improving documentation.

## Getting Started

### Prerequisites
- **C compiler** (GCC/MinGW on Windows, GCC/Clang on Linux/macOS)
- **`make` utility**
- A text editor or IDE (VS Code, CLion, etc.)
- Git (for cloning and submitting changes)

### Setting up your development environment

1. Clone or fork the repository:
   ```bash
   git clone <repo-url>
   cd darts
   ```

2. Build the project:
   ```bash
   make
   ```

3. Test the build:
   ```bash
   # Terminal A: Start server
   ./darts_server
   
   # Terminal B: Start first client
   ./darts_client 127.0.0.1
   # Enter username: alice
   
   # Terminal C: Start second client
   ./darts_client 127.0.0.1
   # Enter username: bob
   ```

## How to Contribute

### Reporting Bugs

If you find a bug, please open an issue with:
- **Title:** A clear, concise description (e.g., "Client crashes on empty username")
- **Description:** How to reproduce the bug, what you expected, what happened instead
- **Environment:** Your OS (Windows/Linux/macOS), compiler version (e.g., `gcc --version`)
- **Logs:** Console output or error messages

Example:
```
Title: Server hangs when client disconnects abruptly

Steps to reproduce:
1. Start server
2. Connect client
3. Kill client process without /quit
4. Server stops responding

Expected: Server continues running and accepts new connections
Actual: Server prints "Socket disconnected" but does not accept new clients
```

### Suggesting Features

Feature suggestions are welcome! Open an issue with:
- **Title:** Brief description of the feature
- **Use case:** Why is this feature useful?
- **Example usage:** How would users interact with it?

Example:
```
Title: Add /history command to retrieve last 20 messages

Use case: New users want to see what was discussed before they joined.

Example usage:
/history        # shows last 20 messages in the room
/history alice  # shows last 20 messages from user 'alice'
```

### Code Changes

We welcome pull requests for bug fixes and features. Here's the workflow:

#### Step 1: Create a branch
```bash
git checkout -b feature/your-feature-name
# or for bug fixes:
git checkout -b bugfix/issue-description
```

#### Step 2: Make your changes
- Edit the relevant files in `src/`, `include/`, or `Makefile`
- Follow the code style in existing files (see below)
- Test your changes thoroughly

#### Step 3: Test your changes
```bash
make clean
make
# Run manual tests (start server + clients, test the new feature)
```

#### Step 4: Commit with a clear message
```bash
git add .
git commit -m "Add /history command to retrieve recent messages"
```

#### Step 5: Push and submit a pull request
```bash
git push origin feature/your-feature-name
```

In your pull request, describe:
- What problem does this solve?
- How does it work?
- Any testing you did
- Any breaking changes or new dependencies

## Code Style Guide

### Naming Conventions
- **Functions:** snake_case (e.g., `handle_client_message`)
- **Variables:** snake_case (e.g., `recv_thread`, `user_map`)
- **Constants:** UPPER_CASE (e.g., `PORT`, `BUFFER_SIZE`)
- **Structs/Types:** CamelCase (e.g., `Packet`, `HashTable`)

### Formatting
- Use 4 spaces for indentation (no tabs)
- Keep lines under 100 characters when reasonable
- Use braces for all control structures (even single statements):
  ```c
  // Good
  if (running) {
      printf("Active\n");
  }
  
  // Avoid
  if (running)
      printf("Active\n");
  ```

### Comments
- Use `//` for inline comments
- Use `/* */` for multi-line comments
- Explain *why*, not *what* (the code shows what it does):
  ```c
  // Good
  // Use hash table for O(1) username lookup instead of O(n) linear search
  hash_insert(&user_map, pkt.source, fd);
  
  // Avoid
  // Insert into hash table
  hash_insert(&user_map, pkt.source, fd);
  ```

### Function Documentation
Document public functions in headers:
```c
// Send a packet to the socket. Returns bytes sent or -1 on error.
void send_packet(int fd, Packet *pkt);
```

## Project Structure

Where to make changes for common tasks:

| Task | File(s) |
|------|---------|
| Add client command | `src/client/client.c` (main loop) |
| Add server message type | `include/common.h` (MsgType enum), `src/server/server.c` (handle_client_message) |
| Add data structure | `include/ds.h` (header), `src/common/ds.c` (implementation) |
| Fix socket/network issues | `src/common/utils.c`, `include/common.h` |
| Improve error handling | Any `src/` file (add checks, better error messages) |
| Update build config | `Makefile` |

## Common Tasks (Examples)

### Adding a new client command

1. Edit `src/client/client.c`, find the main input loop (around line 130)
2. Add a new `else if` branch:
   ```c
   else if (strncmp(buffer, "/newcmd", 7) == 0) {
       pkt.type = MSG_NEWCMD;
       // Parse and handle
       send_packet(sock_fd, &pkt);
   }
   ```
3. If you need a new packet type, add it to `include/common.h`:
   ```c
   typedef enum {
       // ... existing types
       MSG_NEWCMD,
   } MsgType;
   ```
4. Update server to handle the new type in `src/server/server.c`:
   ```c
   case MSG_NEWCMD:
       // Handle the message
       break;
   ```
5. Test it with both client and server running.

### Adding validation

Example: validate username length on client login:
```c
// In src/client/client.c, after scanf
if (strlen(username) < 1 || strlen(username) > NAME_LEN - 1) {
    printf("Username must be 1-%d characters\n", NAME_LEN - 1);
    return 1;
}
```

## Testing Checklist

Before submitting a pull request, test:
- [ ] Code compiles without errors: `make clean && make`
- [ ] Server starts and listens on port 8888
- [ ] Client connects successfully
- [ ] New feature works as intended
- [ ] No memory leaks (use `valgrind` on Linux if available)
- [ ] Existing commands still work (`/all`, `/pm`, `/users`, `/quit`)
- [ ] Multiple clients can connect simultaneously
- [ ] Client disconnects cleanly with `/quit`

## Platform Testing

If possible, test on:
- **Windows:** MinGW (or WSL2)
- **Linux:** GCC
- **macOS:** Clang

Note any platform-specific issues in your PR description.

## Questions?

- Check the `README-new.md` for project overview and troubleshooting
- Look at existing code in `src/` and `include/` for examples
- Open a discussion issue if you're unsure about something

## License

By contributing, you agree that your contributions will be available under the same license as the project (if applicable).

---

Thank you for contributing to DARTS! 🎯
