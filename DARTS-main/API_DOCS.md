# DARTS Developer API Documentation

This document describes the primary server and client-side utility APIs maintained in the `src/common/` headers. They manage data structures for TCP multiplexing, mapping routing connections, and providing cross-platform network safety.

---

## 1. Linked List (`include/ds.h`)
Tracks the active TCP sockets currently known by the server instance. 

* **`void list_add(Node** head, int fd)`**
  Allocates a new tracker node for the associated socket `fd`, placing it at the front of the list. Initiates `last_activity` to the exact insert epoch timestamp.

* **`void list_remove(Node** head, int fd)`**
  Traverses list to target element holding `fd`. Safely unbinds and re-stitches `next` pointers to persist chain integrity, before freeing target node memory.

* **`void list_touch_activity(Node* head, int fd)`**
  Updates the `last_activity` timer on socket `fd` to current timestamp. Primarily used to thwart Idle Connection drops.

* **`void list_set_authenticated(Node* head, int fd, int authenticated)`**
  Switches the boolean flag reflecting successful `MSG_LOGIN` authentication. Changes constraint tracking from `LOGIN_TIMEOUT` to `IDLE_TIMEOUT`.

* **`void list_cleanup(Node** head)`**
  Complete destructive iterator. Frees memory of everything remaining in list sequence. Commonly utilized during `SIGINT` (Ctrl+C).

---

## 2. Hash Table (`include/ds.h`)
Stores lookup indices required to match String usernames (e.g., `"alice"`) directly to System FD sockets (e.g., `5`). Built using separate chaining for collision prevention.

* **`void hash_init(HashTable* ht)`**
  Zeroes out `100` (`HASH_SIZE`) bucket memory locations to default `NULL` pointers.

* **`void hash_insert(HashTable* ht, const char* username, int fd)`**
  Maps lookup dictionary string input `username` to assigned socket `fd`. If collision is registered, adds to chain head safely handling edge cases. Also inherently deals with login duplications safely.

* **`int hash_get(HashTable* ht, const char* username)`**
  Fetches underlying socket File descriptor associated with argument `username`. Returns `-1` if absent. Used to power `/pm` (Private Messages).

* **`void hash_remove(HashTable* ht, const char* username)`**
  Destroys hash relationship link mapping. Deallocates dynamically provisioned entry.

* **`char* hash_get_user_by_fd(HashTable* ht, int fd)`**
  Reverse lookup mechanism. Extremely slow $O(N)$ sweep needed only contextually to discern who dropped the link when system only alerts `fd` death via TCP exceptions.

* **`void hash_cleanup(HashTable* ht)`**
  Comprehensive free loop for graceful memory-leak-free server terminations.

---

## 3. Network Utils (`include/utils.h`)
Underlying platform-agnostic packet routing systems utilizing POSIX threads strictly safely. 

* **`void send_packet(int fd, Packet *pkt)`**
  Sends precisely `sizeof(Packet)` sized struct data to `fd`. Equipped with explicit failure handlers tracking dead sockets, logging partial send exceptions, and triggering system alerts.

* **`int recv_packet(int fd, Packet *pkt)`**
  Synchronous wrapper binding `recv()` against buffer configurations natively loading direct payload results into memory pointer `pkt`. Returns byte-size. 

* **`void log_msg(const char *msg)`**
  Writes globally timestamped updates into concurrent execution streams targeting specific file locations. Safe over extensive asynchronous loads owing strictly to `PTHREAD_MUTEX_INITIALIZER` barriers preventing internal file corruption logging.

* **`int inet_pton(int af, const char *src, void *dst)`** *[Windows specific conditionally-compiled code]*
  Missing functionality patch bridging the gap between historical definitions of local Microsoft compilers targeting legacy distributions by acting as a native wrapper to newer API formats. Safely transfers memory mapping structures supporting IPv6.
