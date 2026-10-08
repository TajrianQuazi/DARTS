#ifndef DS_H
#define DS_H

#include "common.h"
#include <time.h>

// --- Linked List (For Active Clients) ---
typedef struct Node {
    int socket_fd;
    time_t last_activity;
    int authenticated; /* set after MSG_LOGIN */
    struct Node* next;
} Node;

// --- Queue (For Message Buffering) ---
typedef struct QueueNode {
    Packet packet;
    struct QueueNode* next;
} QueueNode;

typedef struct {
    QueueNode *front, *rear;
    int count;
} Queue;

// --- Hash Table (For Username -> Socket lookup) ---
#define HASH_SIZE 100

typedef struct HashEntry {
    char username[NAME_LEN];
    int socket_fd;
    struct HashEntry* next; // Chaining for collisions
} HashEntry;
//---- Initializing Hash Table ----
typedef struct {
    HashEntry* buckets[HASH_SIZE];
} HashTable;

// Function Prototypes
void list_add(Node** head, int fd);
void list_remove(Node** head, int fd);
void queue_init(Queue* q);
void queue_push(Queue* q, Packet p);
Packet queue_pop(Queue* q);
int queue_is_empty(Queue* q);
void hash_init(HashTable* ht);
unsigned int hash_func(const char* key);
void hash_insert(HashTable* ht, const char* username, int fd);
int hash_get(HashTable* ht, const char* username);
void hash_remove(HashTable* ht, const char* username);
char* hash_get_user_by_fd(HashTable* ht, int fd); // Reverse lookup

void hash_cleanup(HashTable* ht);
void list_cleanup(Node** head);
void queue_cleanup(Queue* q);
void list_touch_activity(Node* head, int fd);
void list_set_authenticated(Node* head, int fd, int authenticated);

#endif