# DARTS Project - Issues & Implementation Plan

**Project:** DARTS (Distributed Asynchronous Real-time Talk System)  
**Type:** C-based TCP Client/Server Chat Application  
**Created:** April 10, 2026  
**Status:** Functional but requires critical fixes before production use

---

## 📋 Executive Summary

The DARTS project has **13 major issues** across **8 categories**, ranging from critical security vulnerabilities to documentation gaps. The most urgent are buffer overflows, thread safety problems, and error handling failures. This document outlines all identified issues and a phased implementation strategy to resolve them.

---

## 🔴 CRITICAL ISSUES (Must Fix)

### Issue 1: Buffer Overflow - Client Username Input
- **File:** [src/client/client.c](src/client/client.c#L102)
- **Severity:** 🔴 CRITICAL
- **Location:** Main function, line ~102
- **Problem:** `scanf("%s", username)` reads unbounded input into fixed 32-byte buffer
- **Risk:** Stack overflow → Crash or Remote Code Execution
- **Current Code:**
  ```c
  printf("Enter Username: ");
  scanf("%s", username);  // NO SIZE LIMIT!
  ```
- **Solution:** Add size limit to scanf
  ```c
  scanf("%31s", username);  // Limit to NAME_LEN - 1
  ```
- **Complexity:** ⭐ Trivial
- **Time:** 2 minutes

---

### Issue 2: Buffer Overflow - Hash Table Username Storage
- **File:** [src/common/ds.c](src/common/ds.c#L67-L72)
- **Severity:** 🔴 CRITICAL
- **Location:** `hash_insert()` function
- **Problem:** `strcpy(new_entry->username, username)` with no bounds checking
- **Risk:** Stack overflow during username registration
- **Current Code:**
  ```c
  void hash_insert(HashTable* ht, const char* username, int fd) {
      unsigned int index = hash_func(username);
      HashEntry* new_entry = (HashEntry*)malloc(sizeof(HashEntry));
      strcpy(new_entry->username, username);  // DANGER!
  ```
- **Solution:** Use safe string copy
  ```c
  strncpy(new_entry->username, username, NAME_LEN - 1);
  new_entry->username[NAME_LEN - 1] = '\0';
  ```
- **Complexity:** ⭐ Trivial
- **Time:** 3 minutes

---

### Issue 3: Buffer Overflow - Hash Remove Function
- **File:** [src/common/ds.c](src/common/ds.c#L81-L98)
- **Severity:** 🔴 CRITICAL
- **Location:** `hash_remove()` function
- **Problem:** Username comparison assumes input is safe; input could be malformed
- **Current Code:**
  ```c
  void hash_remove(HashTable* ht, const char* username) {
      unsigned int index = hash_func(username);
      HashEntry *entry = ht->buckets[index], *prev = NULL;
      while(entry != NULL) {
          if(strcmp(entry->username, username) == 0) {  // Implicitly unsafe input
  ```
- **Solution:** Ensure caller has validated input length OR add defensive check
- **Complexity:** ⭐ Trivial
- **Time:** 2 minutes

---

### Issue 4: Hash Table Duplicate Username Bug
- **File:** [src/common/ds.c](src/common/ds.c#L67-L72)
- **Severity:** 🔴 CRITICAL
- **Location:** `hash_insert()` function
- **Problem:** Function doesn't check if username already exists before inserting
  - Same user can log in multiple times with different socket FDs
  - Hash lookup returns first entry, causing message routing to wrong socket
  - Memory leak: Old entry never freed when duplicate inserted
- **Current Behavior:**
  ```c
  hash_insert(&user_map, pkt.source, fd);  // No uniqueness check!
  ```
- **Impact:**
  - User can login twice: first socket active, second becomes "ghost" entry
  - Sending message to user reaches first (old) socket, not current one
  - Memory leak on re-login
- **Solution:** Check if username exists before insert; update FD if exists instead of creating duplicate
  ```c
  void hash_insert(HashTable* ht, const char* username, int fd) {
      unsigned int index = hash_func(username);
      HashEntry* entry = ht->buckets[index];
      
      // Check if exists - update if found
      while(entry != NULL) {
          if(strcmp(entry->username, username) == 0) {
              entry->socket_fd = fd;  // Update FD instead of creating duplicate
              return;
          }
          entry = entry->next;
      }
      
      // Insert only if not found
      HashEntry* new_entry = (HashEntry*)malloc(sizeof(HashEntry));
      strncpy(new_entry->username, username, NAME_LEN - 1);
      new_entry->username[NAME_LEN - 1] = '\0';
      new_entry->socket_fd = fd;
      new_entry->next = ht->buckets[index];
      ht->buckets[index] = new_entry;
  }
  ```
- **Complexity:** ⭐⭐ Easy
- **Time:** 5 minutes
- **Files to Update:** src/common/ds.c

---

### Issue 5: Thread Safety - Client Global Variables
- **File:** [src/client/client.c](src/client/client.c#L5-L8)
- **Severity:** 🔴 CRITICAL
- **Location:** Global variable declarations and usage
- **Problem:** Global variables accessed by two threads without synchronization:
  - `sock_fd` - shared socket
  - `username` - shared username string
  - `running` - volatile flag modified by receiver thread, read by main thread
  
  Race condition example:
  ```c
  // Main thread
  while(running && fgets(buffer, BUFFER_SIZE, stdin) != NULL) {
      // ... send_packet(sock_fd, &pkt);  ← Could be modified by receiver
  }
  
  // Receiver thread
  if(bytes <= 0) {
      printf("\nServer disconnected.\n");
      running = 0;  ← Writes to shared variable
      exit(0);
  }
  ```
- **Solution:** Use pthread_mutex_t for synchronization
  ```c
  static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
  
  // In receive_handler:
  pthread_mutex_lock(&lock);
  if(bytes <= 0) {
      running = 0;
  }
  pthread_mutex_unlock(&lock);
  
  // In main loop:
  while(1) {
      pthread_mutex_lock(&lock);
      if(!running) break;
      pthread_mutex_unlock(&lock);
      // ...
  }
  ```
- **Complexity:** ⭐⭐ Easy
- **Time:** 10 minutes
- **Files to Update:** src/client/client.c

---

### Issue 6: Thread Safety - Server Global Data Structures
- **File:** [src/server/server.c](src/server/server.c#L6-L9)
- **Severity:** 🔴 CRITICAL
- **Location:** Global variable declarations
- **Problem:** 
  - `client_list` - linked list shared but no protection
  - `user_map` - hash table accessed by select loop and handlers
  - `readfds` - fd_set modified during iteration
  
  Currently appears to be single-threaded in server (no receive threads), but future expansion unsafe. Also the message handler could disconnect client while list is being iterated.
  
- **Current risky pattern:**
  ```c
  // Line 46-55: Iterate and check activity
  Node* curr = client_list;
  while(curr) {
      int sd = curr->socket_fd;
      if(sd > 0) FD_SET(sd, &readfds);
      curr = curr->next;
  }
  
  // Later: Could modify list
  handle_client_message(sd);  // Calls disconnect_client which modifies list
  ```

- **Solution:** Use mutex lock or safer iteration pattern
  ```c
  static pthread_mutex_t server_lock = PTHREAD_MUTEX_INITIALIZER;
  
  // Protect all access to client_list and user_map
  ```
- **Complexity:** ⭐⭐⭐ Moderate
- **Time:** 15 minutes
- **Files to Update:** src/server/server.c, src/common/ds.c (add mutex)

---

### Issue 7: Broken Windows inet_pton Implementation
- **File:** [src/common/utils.c](src/common/utils.c#L8-14)
- **Severity:** 🔴 CRITICAL
- **Location:** `inet_pton()` function for IPv6
- **Problem:** Wrong struct cast for IPv6 address copy
  ```c
  case AF_INET6:
      *(struct in_addr *)dst = ((struct sockaddr_in6 *)&ss)->sin6_addr;  
      // ↑ WRONG! Copying 16-byte IPv6 address into 4-byte IPv4 space!
      return 1;
  ```
- **Risk:** Buffer overflow, memory corruption on Windows
- **Solution:** Allocate correct size
  ```c
  case AF_INET6:
      *(struct in6_addr *)dst = ((struct sockaddr_in6 *)&ss)->sin6_addr;
      return 1;
  ```
- **Complexity:** ⭐ Trivial
- **Time:** 2 minutes
- **Files to Update:** src/common/utils.c

---

## 🟠 HIGH PRIORITY ISSUES

### Issue 8: Silent Error Handling in send_packet
- **File:** [src/common/utils.c](src/common/utils.c#L46-L49)
- **Severity:** 🟠 HIGH
- **Location:** `send_packet()` function
- **Problem:** 
  ```c
  void send_packet(int fd, Packet *pkt) {
      if(send(fd, (char*)pkt, sizeof(Packet), 0) < 0) {
          // perror("Send failed");  ← COMMENTED OUT!
      }
  }
  ```
  - Errors silently ignored
  - Client has no idea if message was delivered
  - Could lose messages without notification
- **Solution:** Implement proper error handling
  ```c
  void send_packet(int fd, Packet *pkt) {
      ssize_t bytes_sent = send(fd, (char*)pkt, sizeof(Packet), 0);
      if(bytes_sent < 0) {
          perror("Send failed");
          log_msg("Failed to send packet to client");
      } else if(bytes_sent != sizeof(Packet)) {
          log_msg("Partial packet sent - possible connection issue");
      }
  }
  ```
- **Complexity:** ⭐⭐ Easy
- **Time:** 5 minutes
- **Files to Update:** src/common/utils.c

---

### Issue 9: No Signal Handling (Graceful Shutdown)
- **Files:** [src/server/server.c](src/server/server.c#L1), [src/client/client.c](src/client/client.c#L1)
- **Severity:** 🟠 HIGH
- **Location:** Main function in both files
- **Problem:** 
  - No SIGINT (Ctrl+C) handler
  - Cannot gracefully close sockets and cleanup
  - Resources left dangling (file descriptors, threads, log files)
  - Thread in client not properly joined/terminated
- **Solution:** Add signal handlers
  ```c
  // Server: Add before main loop
  void handle_signal(int sig) {
      printf("\nShutting down server...\n");
      log_msg("Server shutting down due to signal");
      // Cleanup: close all sockets, free data structures
      exit(0);
  }
  
  signal(SIGINT, handle_signal);
  signal(SIGTERM, handle_signal);
  
  // Client: Similar pattern
  ```
- **Complexity:** ⭐⭐ Easy
- **Time:** 10 minutes
- **Files to Update:** src/server/server.c, src/client/client.c

---

### Issue 10: Memory Leaks - Data Structure Cleanup
- **Files:** Multiple
- **Severity:** 🟠 HIGH
- **Location:** Exit paths in both server and client
- **Problem:**
  - Hash table never freed (100 buckets × multiple entries per collision chain)
  - Client/server linked list never freed
  - Queue structures never freed
  - Thread never joined on client exit
  - Manifests as growing memory usage if processes run for extended periods
- **Solution:** Add cleanup functions
  ```c
  // Add to ds.c
  void hash_cleanup(HashTable* ht) {
      for(int i = 0; i < HASH_SIZE; i++) {
          HashEntry* entry = ht->buckets[i];
          while(entry) {
              HashEntry* temp = entry;
              entry = entry->next;
              free(temp);
          }
      }
  }
  
  void list_cleanup(Node** head) {
      while(*head) {
          Node* temp = *head;
          *head = (*head)->next;
          free(temp);
      }
  }
  
  void queue_cleanup(Queue* q) {
      while(!queue_is_empty(q)) {
          queue_pop(q);
      }
  }
  
  // Call before exit in main()
  ```
- **Complexity:** ⭐⭐ Easy
- **Time:** 10 minutes
- **Files to Update:** src/common/ds.c, src/common/ds.h, src/server/server.c, src/client/client.c

---

### Issue 11: No Connection Timeout/Dead Connection Detection
- **File:** [src/server/server.c](src/server/server.c#L40)
- **Severity:** 🟠 HIGH
- **Location:** Main server loop in select()
- **Problem:**
  - Client can connect but never send LOGIN packet → blocks connection slot
  - Dead clients (disconnected network) stay in list → wastes resources
  - No TCP keepalive or timeout mechanism
  - Server has MAX_CLIENTS (100) but no cleanup for idle/dead connections
- **Solution:** Implement connection timeout
  ```c
  typedef struct {
      int socket_fd;
      time_t last_activity;
      int authenticated;  // Flag for login completion
  } ClientInfo;
  
  // Track last activity time on each socket
  // Periodically scan connections and close if no activity for N seconds
  ```
- **Complexity:** ⭐⭐⭐ Moderate
- **Time:** 20 minutes
- **Files to Update:** src/server/server.c, include/ds.h

---

### Issue 12: Incomplete Input Validation
- **Files:** [src/client/client.c](src/client/client.c#L125-L145), [src/server/server.c](src/server/server.c#L81)
- **Severity:** 🟠 HIGH
- **Location:** Command parsing and message validation
- **Problem:**
  - `/pm` command doesn't validate target is not NULL
  - `/all` command processes message with extra whitespace logic error
  - No length validation on message content
  - No validation that target user is actually online before routing
  - Commands case-sensitive (should be case-insensitive)
  
  Example issue in client.c line ~135:
  ```c
  else if (strncmp(buffer, "/pm", 3) == 0) {
      pkt.type = MSG_PRIVATE;
      strtok(buffer, " ");
      char *target = strtok(NULL, " ");
      char *msg = strtok(NULL, "");
      
      if(target && msg) {  // What if target is empty string?
          while (*msg == ' ') msg++;
          strcpy(pkt.target, target);  // No length check!
  ```
- **Solution:** Add comprehensive validation
  ```c
  // Validate target exists and is non-empty
  // Validate message doesn't exceed BUFFER_SIZE
  // Validate username format
  ```
- **Complexity:** ⭐⭐ Easy
- **Time:** 15 minutes
- **Files to Update:** src/client/client.c, src/server/server.c

---

## 🟡 MEDIUM PRIORITY ISSUES

### Issue 13: Thread Logging Race Condition
- **File:** [src/common/utils.c](src/common/utils.c#L32-L43), [src/server/server.c](src/server/server.c#L152-L165)
- **Severity:** 🟡 MEDIUM
- **Location:** `log_msg()` and `broadcast_message()` functions
- **Problem:** 
  - Multiple threads can call `log_msg()` simultaneously
  - Each call does `fopen("darts_chat.log", "a")` → File handle opened/closed multiple times
  - Race condition: log lines can interleave or be lost
  - Broadcast logging has same issue (line 152)
  ```c
  FILE *logfile = fopen("darts_chat.log", "a");
  if (logfile) {
      fprintf(logfile, "[%s] %s: %s\n", datebuf, pkt->source, pkt->data);
      fclose(logfile);  // Inefficient, not thread-safe
  }
  ```
- **Solution:** Use single file handle with fprintf, or use system logger
  ```c
  // Option 1: Single file handle (requires static file pointer + mutex)
  // Option 2: Use syslog() for Posix systems
  // Option 3: Use Windows Event Log on Windows
  ```
- **Complexity:** ⭐⭐⭐ Moderate
- **Time:** 15 minutes
- **Files to Update:** src/common/utils.c, src/server/server.c

---

## 🟡 FEATURE & DOCUMENTATION GAPS

### Issue 14: Missing Protocol Documentation
- **Severity:** 🟡 MEDIUM
- **Problem:** No document explaining:
  - Packet structure and field meanings
  - Message flow (login → broadcast → logout)
  - Error handling contract
  - Version information
  - Protocol limitations (fixed 2048-byte messages)
- **Solution:** Create PROTOCOL.md
- **Complexity:** ⭐ Trivial
- **Time:** 15 minutes
- **Files to Create:** PROTOCOL.md

---

### Issue 15: Inadequate README
- **Severity:** 🟡 MEDIUM
- **Problem:** Current README.md lacks:
  - Build instructions for different platforms (Windows MinGW vs Linux)
  - Usage examples
  - Known limitations
  - Dependencies
  - Architecture diagram
- **Solution:** Enhance README.md
- **Complexity:** ⭐ Trivial
- **Time:** 15 minutes
- **Files to Update:** README.md

---

### Issue 16: No Unit Tests
- **Severity:** 🟡 MEDIUM
- **Problem:** Only manual test.bat; no test coverage for:
  - Data structure operations (hash_insert, list_add, queue operations)
  - Edge cases (empty queue, hash collisions)
  - Error conditions
  - Protocol edge cases
- **Solution:** Create test suite
- **Complexity:** ⭐⭐⭐ Moderate
- **Time:** 60 minutes
- **Files to Create:** tests/, test_*.c, test makefile

---

### Issue 17: No User Authentication
- **Severity:** 🟡 MEDIUM
- **Problem:** 
  - Login only requires username, no password
  - Anyone can impersonate any user
  - No access control
- **Note:** Depends on project requirements; may be intentional for simple demo
- **Solution:** (Optional based on requirements) Add password support
- **Complexity:** ⭐⭐⭐ Moderate
- **Time:** 30 minutes (if needed)

---

### Issue 18: Missing Features
- **Severity:** 🟡 LOW
- **Features not implemented:**
  - Message history/persistence
  - Admin commands (kick user, shutdown server)
  - Room/channel support
  - Message receipt confirmation
  - User status (online/away/offline)
  - Message timestamp validation
- **Note:** May be out of scope for initial version
- **Files to Create:** Feature spec document

---

## 📋 IMPLEMENTATION PLAN

### Phase 1: Critical Security Fixes (IMMEDIATE - Do First)
**Priority:** 🔴 URGENT  
**Estimated Time:** 30-45 minutes  
**Files to Update:** src/client/client.c, src/common/ds.c, src/common/utils.c

| Task | Issue | Time | Complexity |
|------|-------|------|-----------|
| Fix scanf buffer overflow | Issue 1 | 2 min | ⭐ |
| Fix strcpy buffer overflows | Issues 2, 3 | 5 min | ⭐ |
| Fix hash_insert duplicate bug | Issue 4 | 5 min | ⭐⭐ |
| Fix Windows inet_pton IPv6 | Issue 7 | 2 min | ⭐ |
| Fix send_packet error handling | Issue 8 | 5 min | ⭐⭐ |

**Deliverables:** Stable executable without crash vulnerabilities

**Verification:** 
- Compile without warnings
- Manual testing with oversized inputs
- Verify no crashes on login
- Verify hash table doesn't corrupt

---

### Phase 2: Thread Safety & Resource Management (HIGH PRIORITY)
**Priority:** 🟠 HIGH  
**Estimated Time:** 45-60 minutes  
**Dependencies:** Complete Phase 1 first  
**Files to Update:** src/client/client.c, src/server/server.c, src/common/ds.c

| Task | Issue | Time | Complexity |
|------|-------|------|-----------|
| Add client thread synchronization | Issue 5 | 10 min | ⭐⭐ |
| Add server thread safety (if needed) | Issue 6 | 15 min | ⭐⭐⭐ |
| Add data structure cleanup functions | Issue 10 | 10 min | ⭐⭐ |
| Add signal handlers | Issue 9 | 10 min | ⭐⭐ |
| Add thread join on client exit | Issue 10 | 5 min | ⭐ |

**Deliverables:** Memory leak-free execution, graceful shutdown

**Verification:**
- Run memory profiler (valgrind on Linux)
- Verify Ctrl+C cleanly shuts down both programs
- Monitor memory usage over time (should stay constant)

---

### Phase 3: Robustness & Error Handling (MEDIUM PRIORITY)
**Priority:** 🟡 MEDIUM  
**Estimated Time:** 45-60 minutes  
**Dependencies:** Complete Phases 1 & 2  
**Files to Update:** src/server/server.c, src/client/client.c, src/common/utils.c

| Task | Issue | Time | Complexity |
|------|-------|------|-----------|
| Add connection timeout detection | Issue 11 | 20 min | ⭐⭐⭐ |
| Add input validation | Issue 12 | 15 min | ⭐⭐ |
| Add thread-safe logging | Issue 13 | 15 min | ⭐⭐⭐ |

**Deliverables:** Robust system that handles edge cases gracefully

**Verification:**
- Test with incomplete packets
- Test with very long inputs
- Verify timeouts work
- Verify logging doesn't corrupt data

---

### Phase 4: Documentation (LOW PRIORITY)  
**Priority:** 🟡 LOW  
**Estimated Time:** 30-45 minutes  
**Dependencies:** Phases 1-3 complete (can be done in parallel)  
**Files to Create/Update:** PROTOCOL.md, README.md (enhanced), API_DOCS.md

| Task | Issue | Time | Complexity |
|------|-------|------|-----------|
| Create PROTOCOL.md | Issue 14 | 15 min | ⭐ |
| Enhance README.md | Issue 15 | 15 min | ⭐ |
| Add API documentation | Issue 14 | 15 min | ⭐ |

**Deliverables:** Complete documentation for users and developers

**Verification:**
- Another developer can build from README
- Protocol is unambiguous
- Code examples are concrete

---

### Phase 5: Testing (OPTIONAL - If Time Permits)
**Priority:** 🟡 LOW  
**Estimated Time:** 60+ minutes  
**Dependencies:** Phases 1-3 complete  
**Files to Create:** tests/test_*.c, tests/makefile

| Task | Issue | Time | Complexity |
|------|-------|------|-----------|
| Create unit tests for data structures | Issue 16 | 30 min | ⭐⭐⭐ |
| Create integration tests | Issue 16 | 30 min | ⭐⭐⭐ |
| Create stress tests | General | 30 min | ⭐⭐⭐ |

**Deliverables:** Comprehensive test suite

**Verification:**
- All tests pass
- Code coverage > 70%
- Edge cases handled

---

## 🗺️ DEPENDENCY GRAPH

```
Phase 1: Security Fixes
    ↓
Phase 2: Thread Safety & Cleanup
    ↓
Phase 3: Robustness & Error Handling
    ↓
Phase 4: Documentation (can run in parallel with 2-3)
    ↓
Phase 5: Testing (optional)
```

---

## 📊 EFFORT SUMMARY

| Phase | Priority | Time | Complexity | Risk |
|-------|----------|------|-----------|------|
| Phase 1 | 🔴 URGENT | 30-45 min | Low | Critical if delayed |
| Phase 2 | 🟠 HIGH | 45-60 min | Moderate | High if delayed |
| Phase 3 | 🟡 MEDIUM | 45-60 min | Moderate | Medium |
| Phase 4 | 🟡 LOW | 30-45 min | Low | Low |
| Phase 5 | 🟡 LOW | 60+ min | Moderate | Low |
| **TOTAL** | — | **3-4 hours** | — | — |

---

## ⚠️ QUALITY GATES (Before Moving to Next Phase)

### Phase 1 Completion Criteria
- [ ] No compilation warnings on both Windows (MinGW) and Linux
- [ ] No buffer overflow crashes with oversized input
- [ ] Hash table doesn't create duplicate entries
- [ ] Manual test: Server and 2 clients can chat without crashes

### Phase 2 Completion Criteria
- [ ] Valgrind: No memory leaks detected
- [ ] Ctrl+C cleanly shuts down both server and client
- [ ] No crashes on rapid connect/disconnect cycles
- [ ] Thread safety verified with race condition detector (helgrind)

### Phase 3 Completion Criteria
- [ ] All send operations properly handled
- [ ] Connection timeout triggers correctly
- [ ] Logging is thread-safe and complete
- [ ] All commands properly validated
- [ ] Stress test: 50+ concurrent clients, 10+ minutes runtime

### Phase 4 Completion Criteria
- [x] README has step-by-step build/run instructions
- [x] Protocol document is complete and unambiguous
- [x] All APIs documented with examples

### Phase 5 Completion Criteria
- [ ] Unit test suite passes 100%
- [ ] Integration tests verify client-server communication
- [ ] Stress tests pass under load

---

## 🎯 SUCCESS METRICS

After completing all phases:

1. **Security:** ✅ No known vulnerabilities
2. **Stability:** ✅ Runs 24+ hours without crash
3. **Memory:** ✅ Zero leaks detected by valgrind
4. **Performance:** ✅ Handles 100 concurrent clients
5. **Usability:** ✅ Clear documentation and examples
6. **Maintainability:** ✅ Unit tests + code comments
7. **Reliability:** ✅ Graceful error handling

---

## 📝 NOTES

- **Windows Testing:** Requires MinGW gcc compilation with `-lws2_32` flag
- **Linux Testing:** Use standard gcc; valgrind recommended for leak detection
- **Configuration:** Currently hardcoded to port 8888, NAME_LEN 32, BUFFER_SIZE 2048
- **Thread Library:** Uses pthreads; Windows support via MinGW-w64
- **Logging:** Single file `darts_chat.log` in execution directory

---

## ❓ OPEN QUESTIONS FOR USER

1. **Scope:** Is user authentication (password) required, or is current username-only login acceptable?
2. **Features:** Are group chat/channels needed, or is 1-to-N broadcast sufficient?
3. **Platform:** Primary target: Windows MinGW, Linux, or both equally?
4. **Performance:** Maximum concurrent clients needed? (Current design supports 100)
5. **Persistence:** Should chat history be saved to database?
6. **Production:** This for demo/learning or actual deployment? (Affects security priorities)

---

**Document Version:** 1.0  
**Last Updated:** April 10, 2026  
**Status:** Ready for Implementation Review
